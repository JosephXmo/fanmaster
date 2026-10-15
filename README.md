# Fanmaster 1.4

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
- Indicate fan duty or RPM through configurable WS2812 flash, spectrum, or
  combined flash-and-spectrum modes.
- Initialize one shared I2C bus for an SSD1306 display and an SHT30 sensor.
- Initialize four active-low GPIO button inputs with non-blocking debounce.
- Report every debounced button press and release on the serial console when
  Plotter Mode is disabled.
- Show firmware version, fan RPM, duty, IP address, temperature and humidity on the OLED.
- Adjust duty with buttons 1 and 2 in 5% steps, or 1% while button 3 is held.
- Toggle the RGB status indication with button 4.
- Drive the SSD1306 dashboard and wrap the Sensirion arduino-sht sensor.
- Provision Wi-Fi through a Captive Portal and save verified credentials in ESP32 NVS.
- Retry station connections without blocking the local firmware loop.
- Read Web Panel, setup page and static assets from an external W25Q64 through SPIMemory.
- Serve a responsive Web Panel with live RPM and fan duty control.
- Expose status and fan-control HTTP API entrypoints.

SHT30 is sampled once per second. OLED and HTTP share the latest cached sample;
failed reads display placeholders and produce null API values.

## Default hardware configuration

| Function | Default configuration |
| --- | --- |
| Fan TACH | GPIO 1, active falling edge |
| Fan PWM | GPIO 2, 25 kHz, 8-bit |
| Onboard WS2812 RGB status LED | GPIO 21 |
| I2C SDA | GPIO 3 |
| I2C SCL | GPIO 4 |
| Button 1 | GPIO 7, active low |
| Button 2 | GPIO 8, active low |
| Button 3 | GPIO 9, active low |
| Button 4 | GPIO 10, active low |
| SSD1306 address | `0x3C` |
| SSD1306 reset | `-1` (no dedicated reset pin) |
| SHT30 address | `0x44` |
| W25Q64 SCK / MOSI / MISO / CS | GPIO 11 / 5 / 6 / 12 |

Buttons are expected to connect their GPIO input to ground when pressed. The
current carrier board provides external pull-ups, so internal pull-ups are
disabled. Enable `Config::Buttons::ENABLE_INTERNAL_PULLUPS` if external pull-ups
are absent.

Do not wire a switch so that pressing it directly shorts VCC to ground.

## Arduino library dependencies

- ESP32 Arduino core 3.x
- Adafruit SSD1306
- Adafruit GFX Library
- Sensirion arduino-sht (`SHTSensor.h`)
- FastLED 3.10.5 or later
- SPIMemory 3.4.0

The Adafruit GFX dependency is normally installed automatically with Adafruit
SSD1306. The display, sensor, and WS2812 libraries are not vendored in this
repository. Validation uses FastLED 3.10.7, arduino-sht 2.0.0, SPIMemory 3.4.0,
and ESP32 Arduino core 3.3.12.

See [FLASH_RESOURCES.md](FLASH_RESOURCES.md) for SPI wiring, resource packaging,
the independent uploader firmware, application build/flash commands and setup flow.

## Onboard LED response

The onboard addressable RGB LED is a WS2812 device and is driven through the
FastLED Arduino library. `Config::StatusLed::INDICATION_MODE` selects one of
three indication modes:

- `IndicationMode::FLASH` flashes at a response-dependent rate using the fixed
  `FLASH_COLOR_RGB` color, encoded as `0xRRGGBB` and defaulting to red.
- `IndicationMode::SPECTRUM` is the default. It remains lit and changes smoothly
  from violet through the visible spectrum to red as the response rises.
- `IndicationMode::FLASH_AND_SPECTRUM` combines the variable flash rate and
  variable spectrum color.

The response source is selected in `Config.h`:

```cpp
constexpr ResponseSource RESPONSE_SOURCE = ResponseSource::TACH;
constexpr IndicationMode INDICATION_MODE = IndicationMode::SPECTRUM;
constexpr uint32_t FLASH_COLOR_RGB = 0xFF0000;
```

- `ResponseSource::DUTY` maps applied duty from 1% through 100%.
- `ResponseSource::TACH` maps measured speed from 550 through 1700 RPM.

For modes that flash, the low endpoint uses a 1200 ms interval and the high
endpoint a 150 ms interval. Spectrum modes map the low endpoint to violet and
the high endpoint to red. The LED remains off when the response is zero.

Button 4 toggles the indication at runtime. Disabling it keeps the FastLED
controller active but immediately writes black to the RGB LED.

LED timing must remain non-blocking so fan control, TACH sampling, serial
handling, button debounce, and Web Panel requests continue to run while the
LED is flashing.

## Wi-Fi Web Panel

The responsive Web Panel is served directly by the ESP32. It uses the ESP32
Arduino core's built-in `WiFi` and
`WebServer` libraries, without an asynchronous server dependency.

### Network behavior

- Load credentials from ESP32 NVS; no SSID/password constants are compiled in.
- With no valid credentials, immediately open `Fanmaster-Setup-<random suffix>`.
  The random suffix is generated once per boot; no application MAC lookup is used.
- Otherwise attempt station connection with bounded retries; open setup after
  45 seconds of continuous failure.
- Serve the setup page at `http://192.168.4.1/`, with catch-all DNS and HTTP
  redirects for captive-portal discovery.
- Verify submitted credentials by connecting before saving one NVS record.
  Failed candidates preserve the previous record. Save failures leave setup open.
- Stop the setup AP five seconds after a successful connection and save.
- Keep local firmware processing active between resource transmission chunks.
- Print the station IP on connection. Passwords are never logged or echoed.

The open setup link and default NVS storage are not encrypted. The setup AP
remains available while configuration is needed. See the resource guide for
limits, recovery behavior and target acceptance steps.

### Web interface

The Web Panel is stored in external SPI flash and served at `/` when setup is
inactive. During setup, `/` displays the provisioning page. The dashboard contains:

- A current RPM readout refreshed once per second.
- A numeric spinner with `min=0`, `max=100`, and `step=1`.
- A submit button that applies the requested fan duty cycle.
- Current duty-cycle feedback and clear validation or network error messages.
- Live temperature and humidity readouts; failed samples show `--`.

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
  "temperature_c": 24.6,
  "humidity_percent": 52.1
}
```

The `rpm` value is updated by the firmware once per second. Temperature and
humidity are sampled once per second and return `null` after a failed read.
Sensor measurements use the library's synchronous call (about 15 ms conversion
wait at default accuracy); HTTP handlers only read the cache.

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

- `WifiStation` owns station retries, temporary setup AP/DNS, candidate
  verification and the NVS credential record. It uses rollover-safe timing.
- `W25q64` adapts SPIMemory with configurable pins, frequency and address bounds.
- `AssetStore` validates the resource index and file checksums. `WebPanel`
  streams resources through a fixed buffer, keeping whole pages out of RAM.
- `web/` contains editable HTML/CSS/JS. A tiny internal setup form remains
  available when the external setup resources are missing or corrupt.
- `FanController::currentRpm()` exposes a cached one-second sample based on
  TACH interrupt counts. HTTP requests do not recount pulses.
- Buttons, serial and HTTP all update the same `FanController` instance.
- Local control and LED processing run between resource chunks. The synchronous
  HTTP server still has request/socket waits; target loop gaps must be measured.
- The OLED refreshes at most once per 250 ms so display transfers leave time for
  input and network handling.
- Buttons 1/2 adjust duty by 5%; holding button 3 changes the step to 1%.
  Button 4 toggles RGB indication and transmits black immediately when disabled.

## Remaining roadmap

- Add sensor recovery and explicit stale/error diagnostics after startup failures.
- Optionally calculate RPM from TACH interrupt timestamps, with a no-pulse
  timeout, to improve update latency for dependent features including the LED.
- Validate SPI storage, Captive Portal behavior, NVS persistence and main-loop
  responsiveness on the selected ESP32 target. The experimental TACH circuit
  remains unchanged.

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
| `OledDisplay.*` | SSD1306 version, RPM, duty, IP and climate status interface |
| `Sht30Sensor.*`, `ClimateReading.h` | Sensor reads and shared cached climate sample |
| `WifiStation.*` | Non-blocking Wi-Fi station connection and retry |
| `WebPanel.*` | Static resource streaming, recovery setup form and HTTP APIs |
| `W25q64.*`, `AssetStore.*`, `AssetFormat.h` | SPIMemory adapter and checked read-only asset image |
| `web/` | Web Panel and Captive Portal HTML/CSS/JS sources |
| `tools/ResourceUploader/` | Separate serial-to-W25Q64 uploader sketch |
| `scripts/` | Resource pack/verify/upload and staged firmware builds |

## Build status

The firmware targets `esp32:esp32:esp32s3` with ESP32 Arduino core 3.3.12 and
FastLED 3.10.7 and SPIMemory 3.4.0.
