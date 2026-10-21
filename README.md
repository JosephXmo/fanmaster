# Fanmaster 1.4

Fanmaster is an ESP32 Arduino firmware for controlling and monitoring a
four-wire PWM fan. The current firmware drives the fan at 25 kHz, measures
speed from its TACH signal, accepts duty-cycle commands over the serial port,
and uses the onboard WS2812 RGB LED to visualize the applied duty setting.

The project is structured as small hardware and application modules rather
than a single sketch file. Hardware pins and user-adjustable constants live in
`Config.h`.

## Current capabilities

- Start the fan at 100% duty for one second, then restore the saved manual duty
  (or the configured default when operating memory is disabled/unavailable).
- Set an integer duty cycle from 0 through 100 over the serial console.
- Calculate fan RPM from a two-pulse-per-revolution TACH signal once per second.
- Indicate fan duty or RPM through configurable WS2812 flash, spectrum, or
  combined flash-and-spectrum modes.
- Initialize one shared I2C bus for an SSD1306 display and an SHT30 sensor.
- Initialize four active-low GPIO button inputs with non-blocking debounce.
- Report every debounced button press and release on the serial console when
  Plotter Mode is disabled.
- Show firmware version, fan RPM, duty, IP address, temperature and humidity on the OLED.
- Adjust duty with buttons 1 and 2 in 5% steps, or 1% while function (button 4) is held.
  Holding an adjustment button repeats after one second, then every 100 ms.
- Toggle RGB status indication with function + button 3. A standalone button-3
  click switches between manual duty and fixed RPM control, or exits THERMAL.
- Hold an initially configured 850 RPM target with filtered PI feedback, a
  deadband, PWM slew limiting and anti-windup. RPM buttons step by 100, or 10
  with the function key; range and steps are configurable.
- Remember each mode's setpoint in NVS on mode exit. Persistence can be disabled
  in Config; automatic PWM corrections never overwrite the manual duty setting.
- Drive the SSD1306 dashboard and wrap the Sensirion arduino-sht sensor.
- Provision Wi-Fi through a Captive Portal and save verified credentials in ESP32 NVS.
- Retry station connections without blocking the local firmware loop.
- Read Web Panel, setup page and static assets from an external W25Q64 through SPIMemory.
- Serve a responsive Web Panel with live telemetry, manual duty and fixed RPM control.
- Override fixed control with an independent three-point temperature widget,
  live curve preview and monotonic 0.1°C / integer-duty editing. Restore the
  preceding fixed state via its toggle, button 3 or sensor-failure fallback.
- Save thermal curves under the same operating-memory switch; boot stays in Duty.
- Expose status and fan-control HTTP API entrypoints.
- Switch the fan supply through a GPIO 13 relay, with boot-off/key wake, an
  eight-second function-key shutdown, OLED sleep/hints and Web Panel power control.

SHT30 is sampled once per second. OLED and HTTP share the latest cached sample;
failed reads display placeholders and produce null API values.

## Default hardware configuration

| Function | Default configuration |
| --- | --- |
| Fan TACH | GPIO 1, active falling edge |
| Fan PWM | GPIO 2, 25 kHz, 8-bit |
| Fan supply relay input | GPIO 13, active high (configurable) |
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

## Build and upload on macOS, Linux or Windows

Start with [BUILDING.md](BUILDING.md) for tool installation, ESP32 support and
libraries, Python virtual environments, serial-port discovery/permissions and
optional host tests. It includes macOS/Linux shell and Windows PowerShell usage.

Then follow [FLASH_RESOURCES.md](FLASH_RESOURCES.md#build-and-upload) for the
complete resource pack → uploader flash → resource transfer → application flash
sequence, with commands for each host environment. The guide also covers SPI
wiring, OLED upload progress, Arduino IDE staging and Wi-Fi setup. Build outputs
use the ignored `build/` directory; no machine-specific temporary path is needed.

Linux and Windows instructions have not been exercised on those OSes in this
session. [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md) records the actual
macOS/ESP32 measurements separately.

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

Function (button 4) + button 3 toggles the indication at runtime. Disabling it keeps the FastLED
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
- Run Wi-Fi/DNS/HTTP on a dedicated network task on core 0; local controls,
  sensor sampling and OLED stay on the Arduino loop task on core 1.
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
- An RPM target input bounded by Config limits and a button to activate RPM control.
- Active control mode and target RPM, alongside the actual PWM output.
- A fan power button with authoritative relay/starting state and local-key guidance.
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

Submitting duty explicitly selects manual control. `POST /api/fan/rpm` accepts
`rpm=850` and selects fixed RPM control; the target must be a whole number in
`Config::RpmControl::MIN_RPM..MAX_RPM` (default 550–1700). Both endpoints reject
invalid input before changing mode. `POST /api/fan/mode` accepts `mode=duty` or
`mode=rpm` to switch using the existing setpoint. Status also includes `mode`,
`target_rpm`, `manual_duty_percent`, `min_rpm` and `max_rpm`; `duty_percent`
always means the actual PWM output, including automatic corrections.

The OLED shows `DUTY … MANUAL`, `DUTY … SET <RPM>`, or `DUTY … THERMAL`. Serial commands `rpm 850`,
`mode rpm` and `mode duty` supplement the existing 0–100 manual duty command.
See [RPM_CONTROL.md](RPM_CONTROL.md) for tuning, persistence and validation.
[THERMAL_CONTROL.md](THERMAL_CONTROL.md) describes the independent temperature
widget, curve validation/interpolation, local override, sensor-failure policy,
NVS migration and thermal APIs. Fixed changes return HTTP 409 while THERMAL is
active or fan power is off. Successful fan/thermal commands return the complete status snapshot.
The updated Web Panel requires repacking/re-uploading external flash assets as
well as flashing the main firmware. The uploader now keeps the relay off; its
transfer protocol remains unchanged.

`POST /api/fan/power` accepts `enabled=0` or `enabled=1`. Status adds
`power_enabled`, `power_starting` and `power_waiting_for_key`. Every boot defaults
to relay off with “Press any key to power on.” on OLED. Later local shutdown uses
button 4 alone for eight seconds; wake uses buttons 1 + 4 for one second.
See [POWER_CONTROL.md](POWER_CONTROL.md) for cancellation, OLED hints, mode
retention and the distinction between commanded power and physical relay feedback.

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
- The network task owns provisioning, resource reads and the synchronous HTTP
  server. Its request/socket waits do not run on the local control task.
  It shares idle priority on core 0 so parser yield loops cannot starve the
  idle-task watchdog; watchdog protection stays enabled.
- Temperature/humidity and fan-control snapshots use mutexes; cached RPM and
  connection state are atomic. Mode, setpoint, feedback and PWM updates share
  one fan mutex. NVS writes run in the network task outside this mutex.
- USB CDC logging drops output when the host stops reading instead of blocking
  physical controls. The `[LOCAL]` diagnostic measures control-loop gaps;
  `[WEB]` measures the separate network-task gaps.
- The OLED refreshes at most once per 250 ms so display transfers leave time for
  input and network handling.
- Buttons 1/2 adjust duty by 5%; holding function (button 4) changes the step to
  1%. A press steps immediately; a one-second hold starts repetition every
  100 ms. Configure `REPEAT_DELAY_MS` and `REPEAT_INTERVAL_MS` in `Config::Buttons`.
  Opposing held adjustment buttons cancel; delayed loops never replay a burst
  of missed repeat events. Duty remains clamped to 0–100%.
- In RPM mode those same button events adjust the target by `RPM_STEP` (100)
  or `MODIFIED_RPM_STEP` (10), clamped to `RpmControl::MIN_RPM..MAX_RPM`.
- Function + button 3 toggles RGB indication once per button-3 hold, in either
  press order. Releasing that chord does not emit a mode-switch event. A
  standalone button-3 click switches mode on release. Returning to manual mode
  restores its previous duty; entering RPM mode starts feedback from the
  currently applied PWM output. Button 4 alone has no action.

## Remaining roadmap

- Add sensor recovery and explicit stale/error diagnostics after startup failures.
- Measure fixed-RPM settling time, overshoot and load-disturbance recovery on
  the target fan; tune feedback gains and attainable RPM limits if needed.
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
| `FanController.*`, `RpmRegulator.h`, `ThermalCurve.h` | PWM, TACH, modes, smoothed PI and three-point temperature control |
| `OperatingMemory.*` | Versioned, checked NVS setpoint record and deferred writes |
| `SerialConsole.*` | Non-blocking serial duty commands |
| `StatusLed.*` | WS2812 control with configurable duty/TACH visual response |
| `I2cBus.*` | Shared configurable I2C bus |
| `ButtonInputs.*`, `ButtonControls.h` | Debounce, timed repeat, function chords and mode-switch events |
| `OledDisplay.*` | SSD1306 version, RPM, duty, IP and climate status interface |
| `Sht30Sensor.*`, `ClimateReading.h` | Sensor reads and shared cached climate sample |
| `WifiStation.*` | Non-blocking Wi-Fi station connection and retry |
| `WebPanel.*` | Static resource streaming, recovery setup form and HTTP APIs |
| `W25q64.*`, `AssetStore.*`, `AssetFormat.h` | SPIMemory adapter and checked read-only asset image |
| `web/` | Web Panel and Captive Portal HTML/CSS/JS sources |
| `tools/ResourceUploader/` | Separate serial-to-W25Q64 uploader with optional OLED progress |
| `scripts/` | Resource pack/verify/upload and staged firmware builds |
| `BUILDING.md`, `FLASH_RESOURCES.md` | Cross-platform tool setup, build, upload and provisioning guides |

## Build status

The firmware targets `esp32:esp32:esp32s3` with ESP32 Arduino core 3.3.12 and
FastLED 3.10.7 and SPIMemory 3.4.0.

The earlier button/OLED-uploader refinement at `aadcd64` compiles for `CDCOnBoot=cdc`: application
1,028,671 bytes / 50,900 bytes static RAM; uploader 359,546 bytes / 23,960 bytes
static RAM. `sh scripts/check_host.sh` covers debounce/repeat/chord behavior,
OLED update pacing, headless uploads and failure/success display transitions,
alongside existing state, HTTP and resource-integrity checks. Both refinements
have now been rebuilt and flashed on the connected ESP32-S3, and resource
readback, NVS reconnection, HTTP and local-loop behavior were measured. The user
confirmed physical-button behavior and watched OLED progress through the final
`Done verified / 100%` screen. After a host-network interruption was resolved,
all six resources and 12 consecutive status reads passed a final network check.
See [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md) for measurements and limits.

The fixed-RPM revision builds for the same target: application 1,032,411 bytes /
50,956 bytes static RAM. Its host tests include feedback simulation with noisy
and quantized measurements, control-mode/limit handling, NVS recovery and Web
Panel RPM interactions. This revision has now been flashed and tested on the
connected ESP32-S3: selected 650/850/1050 RPM targets, NVS restoration after reset,
resource readback and HTTP concurrency. See the fixed-RPM section of
[HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md) for measured response and limits;
this does not establish the full speed range or recovery under external load.

The preceding three-point THERMAL revision builds at **1,036,335 program bytes /
51,004 static RAM bytes**. Its **34,340-byte, seven-resource** image and main
application have been flashed on the connected ESP32-S3. Actual HTTP curve,
validation and concurrency checks, user-observed OLED/buttons, temperature-driven
output, sensor-loss fallback, legacy NVS migration and cold power-cycle
persistence were exercised. Gross TACH outliers found during peripheral power
interruption are now rejected before feedback. See
[THERMAL_CONTROL.md](THERMAL_CONTROL.md) for behavior,
[HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md) for measured acceptance and
limits, and [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md) for the exact internal Flash
partition map, firmware section sizes and external SPI resource layout.

The relay-power revision builds at **1,038,711 program bytes / 51,028 static RAM
bytes**. The main image and **36,579-byte, seven-resource** SPI image have been
uploaded and verified on the connected board. Host tests include both relay
polarities; actual HTTP tests cover power commands and retained Duty/RPM/THERMAL
state. The user confirmed boot wake, cancelled shutdown holds, eight-second
shutdown, OLED hints and the one-second wake chord. The relay module is not yet
connected, so physical contact operation and removal of 12 V remain unverified.
See [POWER_CONTROL.md](POWER_CONTROL.md) and the relay-power section of
[HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md).
