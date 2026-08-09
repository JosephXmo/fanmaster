# Fanmaster

Fanmaster is an ESP32 Arduino firmware for controlling and monitoring a
four-wire PWM fan. The current firmware drives the fan at 25 kHz, measures
speed from its TACH signal, accepts duty-cycle commands over the serial port,
and uses the onboard WS2812 RGB LED to visualize the applied duty setting.

The project is structured as small hardware and application modules rather
than a single sketch file. Hardware pins and user-adjustable constants live in
`Config.h`.

## Current capabilities

- Start the fan at 100% duty for one second, then use a configurable default.
- Set an integer duty cycle from 0 through 100 over the serial console.
- Calculate fan RPM from a two-pulse-per-revolution TACH signal once per second.
- Increase the onboard WS2812 LED flash rate as fan duty rises and smoothly
  shift its color through the visible spectrum from violet to red.
- Initialize one shared I2C bus for an SSD1306 display and an SHT30 sensor.
- Initialize four active-low GPIO button inputs with non-blocking debounce.
- Provide thin wrappers around Adafruit SSD1306 and Sensirion arduino-sht.
- Connect to a configured Wi-Fi network in station mode with non-blocking retry.
- Serve a responsive Web Panel with live RPM and fan duty control.
- Expose status and fan-control HTTP API entrypoints.

Display content, button actions, and temperature/humidity sampling are not yet
implemented.

## Default hardware configuration

| Function | Default configuration |
| --- | --- |
| Fan TACH | GPIO 4, active falling edge |
| Fan PWM | GPIO 5, 25 kHz, 8-bit |
| Onboard WS2812 RGB status LED | GPIO 38 |
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| Button 1 | GPIO 10, active low |
| Button 2 | GPIO 11, active low |
| Button 3 | GPIO 12, active low |
| Button 4 | GPIO 13, active low |
| SSD1306 address | `0x3C` |
| SSD1306 reset | `-1` (no dedicated reset pin) |
| SHT30 address | `0x44` |

Buttons are expected to connect their GPIO input to ground when pressed. The
firmware enables the ESP32 internal pull-ups by default. If the module already
provides external pull-ups, `Config::Buttons::ENABLE_INTERNAL_PULLUPS` can be
set to `false`.

Do not wire a switch so that pressing it directly shorts VCC to ground.

## Arduino library dependencies

- ESP32 Arduino core 3.x
- Adafruit SSD1306
- Adafruit GFX Library
- Sensirion arduino-sht (`SHTSensor.h`)
- Adafruit NeoPixel 1.15.5 or later

The Adafruit GFX dependency is normally installed automatically with Adafruit
SSD1306. The display, sensor, and WS2812 libraries are not vendored in this
repository. This workspace uses Adafruit NeoPixel 1.15.5 with ESP32 Arduino
core 3.3.11.

## Onboard LED response

The onboard addressable RGB LED is a WS2812 device and must be driven through
the Adafruit NeoPixel Arduino library. Its indication combines flash frequency
and color so changes in the selected response source are easy to observe:

- At lower non-zero values, the LED flashes more slowly and appears violet.
- As the selected response value rises, the flash frequency increases.
- At the same time, the color changes smoothly in visible-spectrum order:
  violet, blue, cyan, green, yellow, orange, then red at the highest value.
- Intermediate values use interpolated colors rather than abrupt color bands.

The response source is selected in `Config.h`:

```cpp
constexpr ResponseSource RESPONSE_SOURCE = ResponseSource::DUTY;
```

- `ResponseSource::DUTY` maps applied duty from 1% through 100%. This is the
  default while the experimental TACH signal is not stable.
- `ResponseSource::TACH` maps measured speed from 500 through 2500 RPM. Use this
  after the TACH circuit has been improved and produces reliable counts.

The low endpoint maps to violet flashes every 1200 ms and the high endpoint to
red flashes every 150 ms. Values between the configured endpoints are linearly
interpolated for both hue and interval. The LED remains off when the selected
response value is zero.

LED timing must remain non-blocking so fan control, TACH sampling, serial
handling, button debounce, and Web Panel requests continue to run while the
LED is flashing.

## Wi-Fi Web Panel

The responsive Web Panel is served directly by the ESP32. It uses the ESP32
Arduino core's built-in `WiFi` and
`WebServer` libraries, without an asynchronous server dependency.

### Network behavior

- Use hard-coded `Config::Wifi::SSID` and `Config::Wifi::PASSWORD` strings.
- Use station mode only (`WIFI_STA`).
- Never create a fallback access point.
- Keep fan control, TACH sampling, serial handling, and button debounce running
  while Wi-Fi is disconnected.
- Attempt reconnection in the background at a bounded interval instead of
  blocking the main loop indefinitely.
- Print the assigned IP address to the serial console after connecting.

Real credentials must be entered locally before flashing. They should not be
published if this repository is later pushed to a remote host.

### Web interface

The Web Panel is compiled into the firmware and served at `/`. It contains:

- A current RPM readout refreshed once per second.
- A numeric spinner with `min=0`, `max=100`, and `step=1`.
- A submit button that applies the requested fan duty cycle.
- Current duty-cycle feedback and clear validation or network error messages.
- Reserved temperature and humidity readouts that show `--` until sensor
  sampling is implemented.

The browser will poll sequentially so a slow request cannot create an
unbounded stack of overlapping one-second requests.

### Entrypoint API

#### `GET /api/status`

Returns the most recent completed TACH sample and current control state:

```json
{
  "rpm": 1234,
  "duty_percent": 35,
  "wifi_connected": true,
  "temperature_c": null,
  "humidity_percent": null
}
```

The `rpm` value is updated by the firmware once per second. Temperature and
humidity remain `null` until the deferred SHT30 work is implemented.

#### `POST /api/fan/duty`

Accepts an `application/x-www-form-urlencoded` body containing one field:

```text
duty=35
```

The server will accept only a complete base-10 integer in the inclusive range
0 through 100. A successful response returns HTTP 200 with the applied duty in
JSON. Malformed, fractional, missing, or out-of-range values return HTTP 400
without changing the fan setting.

Unknown API paths return JSON errors. The Web Panel is intended for a trusted
local network and does not initially include authentication or TLS.

## Implementation details

- `WifiStation` owns station-mode connection attempts and rollover-safe retry
  timing. A failed or lost connection never starts an access point.
- `FanController::currentRpm()` exposes the most recent completed one-second
  TACH sample without recounting pulses during an HTTP request.
- `StatusLed` uses the WS2812 library and maps the configured duty or TACH
  response source to a non-blocking flash interval and spectrum color.
- `WebPanel` owns the embedded HTML, CSS, JavaScript, HTTP routes, and strict
  server-side duty validation.
- HTTP requests, Wi-Fi state updates, TACH sampling, serial commands, and button
  debounce all run cooperatively from the non-blocking main loop.
- Serial and HTTP control call the same `FanController::setDutyPercent()` method
  and therefore share one applied fan state.
- The browser starts its next status poll only after the previous request has
  completed, preventing overlapping requests.

## Deferred roadmap

These features are intentionally outside the first Web Panel implementation:

- Draw Wi-Fi connection state and live RPM on the SSD1306.
- Sample SHT30 temperature and humidity at a sensor-appropriate interval.
- Draw live temperature and humidity on the SSD1306.
- Replace Web Panel temperature/humidity placeholders with sampled values.
- Assign button roles and change duty in 5% steps, clamped to `[0, 100]`.
- Coordinate serial, Web Panel, and button updates through one shared fan-state
  interface.
- Improve the experimental TACH circuit, verify stable pulse counting, then set
  `Config::StatusLed::RESPONSE_SOURCE` to `ResponseSource::TACH`.

### Optional captive portal fallback

The current firmware remains station-only and never creates an access point.
A later optional enhancement may provide Wi-Fi provisioning when the configured
credentials are unavailable:

- After a bounded number or duration of failed station connection attempts,
  create an open setup access point.
- Run a catch-all DNS server that redirects client requests to a local Captive
  Portal.
- Embed a second, deliberately compact webpage that asks for the target Wi-Fi
  SSID and password.
- Validate submitted values without logging or returning the password in HTTP
  responses.
- Attempt the supplied credentials while fan control, TACH sampling, and other
  local firmware work continue to run.
- Stop the open access point and Captive Portal after a successful station
  connection.

Before implementation, decide whether submitted credentials survive a reboot
in ESP32 NVS or remain valid only for the current session, how long the setup
access point remains available, and how it is named. An open provisioning AP
exposes submitted credentials as plaintext on its local radio link, so the
portal should exist only while configuration is necessary.

Two embedded pages will increase flash use and temporary heap pressure. The
main Web Panel may therefore be simplified, or the two pages may share a small
common style vocabulary while remaining independently servable. Resource usage
must be measured on the selected ESP32 target before this fallback is enabled.

## Source layout

| Files | Responsibility |
| --- | --- |
| `fanmaster.ino` | Firmware setup, loop, and module coordination |
| `Config.h` | Pins, addresses, timing, and user configuration |
| `FanController.*` | PWM output, TACH interrupt, and RPM calculation |
| `SerialConsole.*` | Non-blocking serial duty commands |
| `StatusLed.*` | WS2812 control with configurable duty/TACH visual response |
| `I2cBus.*` | Shared configurable I2C bus |
| `ButtonInputs.*` | Four active-low debounced digital inputs |
| `OledDisplay.*` | Adafruit SSD1306 wrapper |
| `Sht30Sensor.*` | Sensirion arduino-sht wrapper |
| `WifiStation.*` | Non-blocking Wi-Fi station connection and retry |
| `WebPanel.*` | Embedded responsive UI and HTTP API entrypoints |

## Build status

The firmware targets `esp32:esp32:esp32s3` with ESP32 Arduino core 3.3.11 and
Adafruit NeoPixel 1.15.5. The Adafruit revision has passed static checks; a full
target build remains to be run.
