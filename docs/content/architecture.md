# Firmware architecture

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

## Task ownership

| Context | Owns | Shares |
| --- | --- | --- |
| Arduino loop, core 1 | Buttons, serial, TACH/PWM feedback, SHT30, OLED, LED | Controller and climate snapshots |
| Network task, core 0 | Wi-Fi, DNS, HTTP, external resources, deferred operating-memory writes | The same controller instance and cached climate data |
| TACH ISR | Edge count | Counter consumed by the one-second RPM sample |

A separate network task isolates socket/parser waits from local controls. It
does not make every library call asynchronous: SHT30 conversion and individual
SPI erase/program operations are synchronous. Runtime loop-gap measurements
and watchdog fixes are recorded in [validation](validation.md).

## Source map

Main firmware files below are relative to `src/fanmaster/`, unless an explicit
directory is listed. The uploader sources live in `src/ResourceUploader/`; the
build script stages their required shared headers and SPI adapter from the main
source tree into a separate Arduino sketch directory.

| Files | Responsibility |
| --- | --- |
| `fanmaster.ino` | Firmware setup, loop, and module coordination |
| `src/fanmaster/Config.h` | Pins, addresses, timing, and user configuration |
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
| `src/ResourceUploader/` | Separate serial-to-W25Q64 uploader with optional OLED progress |
| `scripts/` | Resource pack/verify/upload and staged firmware builds |
| `docs/content/`, `docs/theme/`, `scripts/build_docs.py` | Documentation sources, theme and static site builder |

## Follow-up work

- Characterize external load-disturbance recovery and independently calibrate
  attainable RPM endpoints on the target fan. Existing step tests are recorded
  separately from these remaining checks.
- Consider TACH edge timestamps and a no-pulse timeout to reduce low-speed
  measurement latency without changing the experimental circuit.
- Complete physical relay/contact and 12 V interruption acceptance when wired.

Current provisioning, SPI transfers, persistence and local-loop behavior already
have target evidence; see the validation log rather than treating them as
unstarted tasks.
