# ESP32-S3-Zero hardware acceptance — 2026-10-15

This records measured results from the connected Waveshare ESP32-S3-Zero,
not just host substitutes or successful compilation. The initial round used
merge commit `858150f` (version 1.4) with fixes later committed as `26e14ea`.

## Target and upload

- Actual USB port: `/dev/cu.usbmodem101`.
- ESP32-S3 QFN56 revision 0.2, embedded 4 MiB XMC flash and 2 MiB PSRAM.
- Arduino target: `esp32:esp32:esp32s3:CDCOnBoot=cdc`, ESP32 core 3.3.12.
- W25Q64: SCK 11, MOSI 5, MISO 6, CS 12, ordinary SPI at 10 MHz.
- Dedicated ResourceUploader and application writes passed esptool hash checks.
  No whole-chip erase was performed; the application NVS was preserved.
- The external image contains six files and occupies 23,134 bytes. Transfer
  returned `DONE verified`, after page readback, whole-image CRC and final
  header readback. This checks the occupied image, not every sector of the chip.
- Runtime HTTP downloads of the dashboard HTML and all four CSS/JS assets
  matched their `web/` source bytes exactly. The setup HTML was served from
  external flash during initial provisioning.

## Problems reproduced and fixes

1. Opening native USB with default asserted DTR/RTS left the chip in ROM download
   mode and prevented uploader PING/PONG. Setting both controls low before
   opening the port allowed PONG and a verified resource transfer. The host
   script now does this, with a regression assertion in its protocol test.
2. With USB connected but serial output not drained, blocking CDC logging
   affected physical controls, OLED and HTTP. The application now uses a zero
   CDC transmit timeout. After this change the user confirmed buttons and OLED
   recovered and completed real Wi-Fi provisioning.
3. Normal concurrent HTTP requests exposed a 5,564 ms network/control loop gap.
   DNS/HTTP/flash reads now run in their own task on core 0. Local controls,
   TACH sampling, LED, SHT30 and OLED remain on the Arduino task on core 1.
   Climate snapshots are protected as a complete sample; RPM and connection
   flags are atomic; duty setters serialize the PWM write and cached value.
4. An intentionally incomplete request exposed starvation of IDLE0 when that
   network task ran at priority 1: the task watchdog reset the board. The
   network task now shares idle priority, allowing the parser's yield calls
   to schedule IDLE0. Watchdog protection remains enabled.

## Provisioning and application checks

- A client obtained `192.168.4.2` from the setup AP.
- A real UDP A query for `captive.apple.com` returned `192.168.4.1`.
- `/setup`, its CSS/JS, `/api/status` and `/api/wifi/status` returned HTTP 200.
- `/hotspot-detect.html` led to `/setup`; the real user then confirmed successful
  provisioning. No Wi-Fi password was supplied to the agent or logged.
- The user confirmed physical button and OLED operation after the CDC fix.
- After reset the stored NVS record reconnected without resubmitting credentials,
  and the station was reachable at `192.168.30.215` (DHCP address may change).
- In station mode `/` matched the dashboard HTML, while `/setup` and configuration
  POST returned 403 after the setup AP closed.
- Example live status: approximately 750–840 RPM at 35% duty, 24°C and 53% RH.
- During the CDC regression test, HTTP and sensor updates continued while the
  serial reader deliberately paused for 30 seconds. The observed local loop
  gap before network separation was at most 33 ms in that test.
- The final 85-second run, using the network task at idle priority, completed
  30 requests with three concurrent clients. All succeeded; the longest took
  3.915 seconds. All five downloaded HTML/CSS/JS resources matched source bytes.
- An incomplete HTTP header held the network task for 5,058 ms. The measured
  local loop gaps across that run were 47, 46, 30, then 29 ms. No watchdog reset
  appeared during the run. This verifies isolation of local controls, not
  low-latency HTTP under a slow client; the HTTP server is still synchronous.
- HTTP duty changes to 40% and back to 35% succeeded. The final status reported
  779 RPM, 35% duty, Wi-Fi connected, 24.0°C and 59.2% RH. The board was left
  running the application with 35% duty and its existing NVS credentials.
- The final application build used 1,028,251 bytes of program storage and
  50,844 bytes of static RAM, and its upload passed the esptool hash check.

## Evidence and limits

Local transcripts, probe scripts and JSON measurements are under the ignored
`build/hardware-acceptance/` directory. `probe-results.json` covers setup DNS/HTTP;
`reconnect-70s.log` covers NVS reconnection. The `watchdog-priority1-*` files
preserve the failed task-priority experiment rather than treating it as a pass.
`final-runtime.log` and `final-results.json` record the final task-priority
regression run.

Host checks include Python format/protocol tests, strict C++ state/HTTP/uploader
tests, and a concurrent climate publication/snapshot test. These supplement
the target checks; they do not replace them.

The dependency emits `esp_cache_msync: invalid addr or null pointer` once during
LED initialization. Subsequent LED transfers and control processing continue;
the source of that library diagnostic has not been repaired in this change.
No oscilloscope verification of PWM/LED waveforms, flash-disconnect injection,
deliberate power loss during upload, or long-duration soak was performed.
The user separately reported `ERR_HTTP_RESPONSE_CODE_FAILURE`, followed by
`502 Bad Gateway` in Chrome or another browser. A fresh direct source-server
check returned `HTTP/1.1 200 OK`, `Content-Type: text/html; charset=utf-8`,
`Content-Length: 2832` and `Connection: close` from `192.168.30.215`. The user
then confirmed the HTTP problem was resolved and their proxy had glitched.
No firmware change was needed for that 502 error.

Automated browser rendering was not verified: the embedded Codex browser first
returned `ERR_BLOCKED_BY_CLIENT`, and a subsequent inspection was denied by its
URL security policy. The user's confirmation, source-server HTTP measurements
and successful captive-portal interaction are separate evidence.

## Refinement acceptance at aadcd64

This follow-up uses the committed button-repeat/function-key and OLED-uploader
refinements, on the same board and `/dev/cu.usbmodem101`.

- The previous temporary ctags installation had been deleted. Arduino ctags
  5.8-arduino11 was rebuilt as a native ARM64 executable at
  `build/tools/arduino-ctags/bin/ctags`, using the official source tag and
  renaming its reserved `__unused__` macro for compatibility with the macOS SDK.
  The rebuilt tool is outside `/tmp`; it does not replace global Arduino tools.
- Both firmware builds passed using that ctags override. The application uses
  1,028,671 bytes of program storage and 50,900 bytes static RAM; the uploader
  uses 359,546 bytes and 23,960 bytes respectively.
- The OLED uploader was flashed, with esptool hash verification. The resource
  image again contained six files / 23,134 bytes (whole-image CRC32 `e1f98a63`).
  Actual serial transfer took approximately one second and returned
  `DONE verified`, including page readback, CRC and final header readback.
- The application was then flashed back with esptool hash verification, without
  erasing NVS. It automatically reconnected to `192.168.30.215` about 3.3 seconds
  into the serial observation. The board was left running the application at
  35% duty, with LED indication enabled.
- `/` returned HTTP 200 and matched the dashboard. The other five accessible
  HTML/CSS/JS resources also matched source bytes. `/setup` and configuration
  POST returned 403 after the setup AP closed.
- Thirty requests with three concurrent clients all succeeded; maximum request
  time was 3.511 seconds. HTTP duty changes to 40% and back to 35% succeeded.
- An intentionally incomplete header produced a network-task gap of 5,626 ms.
  Across 227.9 seconds of serial observation, the local-loop gap peaked at 47 ms
  and settled to 28 ms per reporting window. There was one initial USB-induced
  boot and no observed watchdog/panic or subsequent reboot. Fan RPM updates
  continued throughout; the latest successful HTTP sample reported 780 RPM,
  35% duty, 23.2°C and 60.9% RH.
- Later continuous status polling timed out. A subsequent host check found
  `en0` at `10.253.43.254`, with the route to the device now using gateway
  `10.253.0.1`, rather than a direct device-subnet route. Three retries also
  timed out. Serial continued showing local and network task updates without
  resets. Continued HTTP acceptance needs a host route to the device network;
  the initial successful HTTP/concurrency results are not a pass for the
  later unreachable-network interval.
- The user confirmed the requested physical checks all behaved normally:
  button-1/2 taps and holds, the one-second wait before continuous adjustment,
  function/button-4 fine adjustment, function + button 3 LED toggle, no LED
  toggle on button 4 alone, no duty change on standalone button 3, and continued
  OLED/fan operation. This is user-observed physical acceptance; no button
  edges were recorded in the earlier serial observation.
- OLED acceptance was repeated with the user watching. After another verified
  uploader flash and PONG, the user confirmed `FANMASTER UPLOADER / Waiting for
  host`. Host-side pacing of 120 ms per page extended the same 23,134-byte upload
  to 13.475 seconds without changing firmware or the upload protocol. It again
  returned `DONE verified`, and the user confirmed both increasing progress and
  the final `Done verified / 100%` screen. The application was restored only
  after that visual confirmation.
- After the host returned to `192.168.30.152`, the application was reflashed
  with hash verification and reconnected through the retained NVS record.
  A fresh 30-second run matched all six external resources (including setup
  HTML) against source, matched `/` to the dashboard, confirmed setup/configure
  remained closed with HTTP 403, and completed 12 consecutive status reads.
  Final sampled state: 750 RPM, 35% duty, Wi-Fi connected, 25.5°C and 74.9% RH.
  Local-loop gaps peaked at 47 ms, with no observed watchdog/panic or extra
  reboot. This follow-up passed and resolves the earlier HTTP retest pending
  restoration of host network access.

Follow-up build/flash logs and measurements are stored separately under the
ignored `build/refine-acceptance/` directory so initial-round evidence remains
available. `oled-retry-upload.log` records the paced visual check;
`restored-network-runtime.log` and `restored-network-results.json` record the
successful final application/network check. The board is left running the
application at 35% duty with LED indication enabled and its NVS intact.

## Fixed-RPM working-tree acceptance

This round uses the uncommitted fixed-RPM implementation on `e5f7d01`, with
the default Config gains/range and unchanged experimental wiring. No feedback
gains were retuned during this run. Main-sketch sources were compared byte for
byte with the staged successful build before upload. Evidence is kept under
the ignored `build/rpm-acceptance/` directory; `manifest.json` records source
and binary hashes. This round does not replace the older acceptance evidence.

### Upload and interfaces

- Target: the same ESP32-S3-Zero at `/dev/cu.usbmodem101`. Host LAN address was
  `192.168.30.152`; the device automatically reconnected at `192.168.30.215`.
- Main build: 1,032,411 program bytes / 50,956 static RAM bytes. Uploader:
  359,546 / 23,960. Both compiled for `esp32:esp32:esp32s3:CDCOnBoot=cdc`.
- Uploader and main flashes passed esptool hash verification. The new six-file
  resource image is 26,108 bytes and returned `DONE verified` after transfer.
  All six resources and `/` subsequently matched their source bytes over HTTP.
  NVS was not erased; no Wi-Fi credentials were read out or changed.
- Initial state was Duty 35%, target 850 RPM. Requests for 0, 549, 1701,
  fractional, negative and overflowing RPM values returned HTTP 400 without
  changing mode or target.

### Closed-loop measurements

The test first selected manual Duty 42%, then applied the following targets.
Each row summarizes the last 30 status samples of its observation. Sampling
was approximately once per second and reads the existing completed TACH window.

| Target and transition | Observation | Mean RPM | Raw RPM range | RPM standard deviation | Applied duty range |
| --- | --- | --- | --- | --- | --- |
| Enter 850 | 100 s | 847 | 810–930 | 26.48 | 40–40% |
| 850 → 1050 | 100 s | 1034 | 990–1200 | 43.63 | 52–53% |
| 1050 → 650 | 110 s | 657 | 540–720 | 37.43 | 28–29% |
| 650 → 850 | 100 s | 836 | 810–900 | 27.64 | 38–38% |

The final mean errors were -3, -16, +7 and -14 RPM respectively. PWM did not
show sustained large alternating corrections in these steady portions. Raw TACH
readings still include quantization and occasional larger deviations; a 10 RPM
target step does not establish 10 RPM measurement accuracy or instantaneous
tracking within the deadband.

As a descriptive settling measure, the trailing ten-sample mean entered and
remained within ±30 RPM of target for the rest of the run at approximately
22, 69, 75 and 39 seconds respectively. This retrospective measure is not a
claim that every individual sample stayed inside that band. There was no
independent tachometer or controlled external load disturbance in this test.

`sweep-results.json` contains all 410 status samples. The full sweep lasted
433.5 seconds. The longest status request took 1.5194 seconds. The local-loop
gap peaked at 47 ms and later reporting windows were 28–29 ms. There was one
initial USB-associated boot and no observed watchdog/panic or subsequent reboot.
Temperature and humidity remained available throughout the status checks.

### Operating memory and network load

- Before resetting, the test saved distinct sentinel settings: manual Duty 42%
  and target 930 RPM, by leaving the corresponding modes and allowing the
  queued save to finish. Returning from feedback to Duty applied 42%, not the
  automatically calculated PWM value.
- esptool performed a hardware reset without erasing flash. The restarted
  application entered Duty and reported 42% / 930 RPM, confirming both settings
  were restored rather than falling back to Config's 35% / 850 RPM defaults.
  This is reset persistence; a cold power removal was not performed.
- The test then saved 35% / 850 RPM. A further USB-associated restart again
  reported these values in Duty before the test re-entered RPM mode.
- With feedback active, 30 HTTP requests from three concurrent clients all
  completed; the longest took 4.5535 seconds. This records queueing through the
  synchronous server, not parallel request handling. Physical observation and
  serial diagnostics were recorded in a separate final observation run.

The user confirmed all requested physical checks for this RPM revision were
normal: button-3 mode selection and OLED MANUAL/SET display; RPM adjustment by
100/10 with the function modifier; hold repeat after one second; function +
button 3 LED toggle without a mode change; and continued OLED/fan operation.
The 145.9-second observation also recorded button/chord events, both modes and
targets reaching both Config bounds during deliberate holds. Its 100 HTTP
samples include user adjustment and are not an additional steady-state curve.
Local gaps remained at most 47 ms, with no observed watchdog/panic or extra
reboot after the initial USB-associated boot. The latest LED diagnostic was
enabled. User observations and firmware measurements are separate evidence.

After these checks, the test restored the remembered manual duty to 35% and
target to 850 RPM and left fixed-RPM control active. A final 60-second HTTP run
completed all 60 status reads; the last 30 averaged 836 RPM (810–900 raw RPM)
with PWM constant at 38%. Final sampled climate was 23.6°C / 69.4% RH, with
Wi-Fi connected. Serial handles were closed and the application was left
running. The configured
550/1700 RPM endpoints were reached as setpoints during button tests, but their
steady physical speeds were not characterized. A cold power-cycle check,
independent speed calibration and external load-disturbance recovery remain
unmeasured.

## Three-point THERMAL hardware acceptance — 2026-10-15

### Firmware, resources and host checks

- Host: macOS / Arduino CLI 1.5.1 / ESP32 Arduino core 3.3.12.
- Target: the connected Waveshare ESP32-S3-Zero, ESP32-S3 QFN56 revision 0.2,
  4 MiB internal flash and 2 MiB PSRAM. USB reenumerated from
  `/dev/cu.usbmodem101` to `/dev/cu.usbmodem1101` during preparation; the latter
  was used for this test. The monitor-device serial port was not used.
- FQBN: `esp32:esp32:esp32s3:CDCOnBoot=cdc`. Main application:
  1,036,139 program bytes / 51,004 static RAM bytes before the outlier fix;
  final build 1,036,335 / 51,004. The staged source files
  matched this checkout before flashing, and the application SHA256 matched
  its build manifest.
- Reused the previously verified resource uploader, then flashed the new main
  application. esptool verified both images. No full-chip erase was performed.
- The seven-resource W25Q64 image is 34,340 bytes, whole-image CRC32 `ca141fbb`.
  Upload returned `DONE verified`. The user confirmed waiting screen, live
  progress and final `Done verified / 100%` on the OLED.
- All seven static resources, plus `/` against `web/index.html`, subsequently
  matched source bytes over actual device HTTP. Wi-Fi reconnected using the
  existing NVS credentials at `192.168.30.215`.
- The host Python/C++ suites and all three Node.js suites (setup, fixed panel,
  thermal widget) passed again. The later peripheral power fault test exposed gross TACH noise, fixed and
  retested as described below.

### Curve and HTTP behavior

Actual SHT30 samples drove each output. The curves were deliberately moved
around room temperature to exercise both clamps and both interpolation
segments without requiring the hardware to reach 15°C or 95°C. Each phase
contains eight status samples, spaced about one second apart.

| Phase | Curve (temperature °C / duty %) | Measured °C | Applied duty | Result |
| --- | --- | --- | --- | --- |
| Low hold | 30/35, 50/65, 80/90 | 25.8–26.4 | 35% | Held first duty |
| First segment | 15/20, 35/60, 80/90 | 25.2–25.6 | 40–41% | Linear interpolation |
| Second segment | 15/20, 20/40, 40/80 | 25.0–25.1 | 50% | Linear interpolation |
| High hold | 15/20, 16/40, 17/70 | 24.7–24.9 | 70% | Held last duty |
| Plateau | 15/45, 35/45, 80/90 | 24.6–24.7 | 45% | Equal duty values accepted |

All 40 outputs exactly matched the expected rounded integer duty computed from
the reported temperature and that phase's curve. These short phases verify
mapping, not steady-state fan response or temperature calibration.

Malformed/invalid curve submissions returned HTTP 400 and preserved the previous
curve: extra decimal precision, equal/reversed temperatures, out-of-range
limits, descending duty, fractional duty, overflow and a missing point field.
While THERMAL was active, valid fixed-duty, target-RPM and fixed-mode changes
all returned HTTP 409. Web/API exit restored both a preceding RPM 950 setting
and a preceding manual 42% setting in separate tests.

Three concurrent clients completed all 30 status requests; the slowest took
3.3507 seconds. This is a functional concurrency check, not a latency guarantee.

### Local controls, temperature change and sensor failure

The user confirmed on the physical board:

- OLED shows `THERMAL`; fixed web inputs/buttons are disabled.
- Buttons 1/2, including long holds, do not edit fixed settings in THERMAL.
- Function + button 3 toggles LED indication without leaving THERMAL.
- Standalone button 3 exits only the override, restores MANUAL 35%, and the web
  toggle/fixed controls follow that change.
- During a physical temperature-change exercise, OLED, web telemetry and fan
  remained responsive. Recorded temperature rose from 24.8°C to 48.7°C and
  subsequently fell. With the default curve, duty rose from 35% to 70%; the
  48.7°C sample agrees with the second segment. There were 92 samples in the
  bounded warming record; its last sample was 26.7°C / 38%.

The first attempted sensor disconnect was invalidated by accidental contact
with other wiring and unintended button events. It is excluded from acceptance.
The clean retry removed the shared 3.3 V peripheral supply (SHT30, OLED and
W25Q64), without touching buttons/web controls. The mode changed from THERMAL
to RPM 650, preserving the 35% manual preset. Temperature/humidity became null,
then recovered after reconnection; fixed control remained selected. The user
confirmed sensor and OLED recovery. That test did not separately establish an
isolated SHT30-only outage or electrical disconnect-to-reaction timing.

### Fault discovered and corrected

The first clean shared-power test exposed an implausible TACH sample of
1,995,847 RPM that could perturb the PI regulator, despite correct mode fallback.
The final firmware adds `Config::Fan::MAX_VALID_RPM=4000`. A non-finite or
above-ceiling completed sample resets/advances the counter window but preserves
the cached RPM, output, filter and integral. Subsequent valid windows resume
feedback. Host regression explicitly injects a noisy pulse window, checks the
unchanged state, then verifies normal-window and zero-RPM recovery.

After compiling/flashing the correction with hash verification, the shared
peripheral power test was repeated. All 18 unavailable-sensor observations were
in RPM 650 with the manual 35% preset intact. Enabling THERMAL while unavailable
returned HTTP 409. Recovery retained RPM 650 and `thermal_exit_reason="sensor"`;
the user again confirmed OLED/climate/fixed recovery. Eleven rejected TACH
windows were logged, peaking at 2,215,247 RPM before rejection; the outage
watcher's published RPM never exceeded 840. Rejected windows did not drive the
PI regulator. This test demonstrates the gross-outlier guard, not elimination
of electrical noise, edge debouncing, low-level noise rejection below the
ceiling, or an independent tachometer calibration. I²C timeouts while the entire
peripheral rail was off still caused local-loop gaps up to 655 ms; after recovery
reporting windows returned to 28–29 ms. Powering down a connected peripheral rail
is not a normal operating condition.

### Legacy migration and actual cold boot

To avoid ambiguity from earlier user adjustments, a controlled migration test
briefly restored the preceding fixed-RPM firmware. It saved distinct 43% manual
duty and 930 RPM target presets in a legacy FMO1 record. The final THERMAL
application was then flashed without erasing NVS. It started in Duty 43%, retained
930 RPM, and supplied the default three-point curve. This verifies the actual
old-to-new record path on this board, beyond host serialization tests.

Next, the test saved non-default points 25.3°C/37%, 41.2°C/66%, 81.1°C/92% and
entered THERMAL over RPM 930. The user confirmed complete board power removal
for about five seconds followed by power restoration. HTTP then reported Duty
43%, the remembered 930 RPM target and all three non-default points. THERMAL
was not automatically re-enabled. Existing Wi-Fi credentials reconnected; they
were neither read out nor intentionally changed. This is a real cold power-cycle
check, distinct from an esptool reset or reopening the USB serial port.

### Final-image regression

The final application image is 1,036,480 bytes, SHA256
`9369e762d5fc90274358c2960d374a0646a5a660d10630732e793136e7f8e517`.
Compiler program usage is 1,036,335 / 1,310,720 bytes; static RAM 51,004 / 327,680.
Final-image HTTP regression again matched all seven resource payloads and `/`
against source, rejected invalid curves atomically, rejected all three fixed
APIs during THERMAL, and restored both RPM 950 and Duty 42 on exit.

Five final-image phases again collected eight samples each:

| Phase | Temperature range | Duty range | Mapping |
| --- | --- | --- | --- |
| low_hold | 24.7–24.8°C | 35–35% | Maximum rounded-duty difference 0% |
| segment_one | 24.7–24.8°C | 39–40% | Maximum rounded-duty difference 0% |
| segment_two | 24.7–24.8°C | 49–50% | Maximum rounded-duty difference 0% |
| high_hold | 24.7–24.8°C | 70–70% | Maximum rounded-duty difference 0% |
| plateau | 24.7–24.8°C | 45–45% | Maximum rounded-duty difference 0% |

All 30 status requests from three concurrent clients completed on the final
image; the slowest took **6.7245 seconds** while a separate serial/HTTP
monitor also polled status. The initial-image result was 3.3507 seconds under its
own run conditions. These results confirm eventual service under that load;
the synchronous server still queues requests and is not a latency guarantee.

Evidence remains in ignored `build/thermal-acceptance/`: final build/flash logs,
`initial-automated-results.json`, `automated-results.json`, `warming-results.json`,
`shared-power-results.json`, `final-power-results.json`, `seed-old.json`,
`verify-upgrade.json`, `save-curve.json`, `verify-reboot.json`, and the serial/HTTP
monitor records. Host suites passed after the code correction. Actual browser
and physical-screen checks are user observations; the local simulated-browser
render and screenshots do not substitute for target execution.

For exact firmware sections, internal partitions and the external resource
schema, see [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md). Live NVS page occupancy,
independent temperature/speed calibration, long-duration reliability, isolated
SHT30-only power loss, physical endpoints 550/1700 RPM and external load rejection
remain outside this acceptance. A pre-existing `esp_cache_msync: invalid addr or
null pointer` line still appears at boot, also present in earlier fixed-RPM logs;
it has not been diagnosed here. Startup Wi-Fi association/reconnection produced
initial HTTP timeouts; successful connection and later operation are reported
separately rather than claiming every request from power-on succeeded.

### Final observation and retained state

A separate final-image observation after the cold-boot check ran for **340.292
seconds**, with **306 successful status samples**. Two initial 12-second HTTP
attempts timed out during reconnection; no further HTTP errors were recorded
after connectivity returned. Serial recorded one initial USB-associated boot,
no additional startup, and no panic/backtrace/watchdog. Normal local reporting
windows were about 28–30 ms; the 655 ms maximum coincided with the deliberate
shared-peripheral outage. The pre-existing cache warning described above was
still present at startup.

The last 60 status samples averaged **666.5 RPM**, range **630–720 RPM**, with
applied duty fixed at **28%**. The target remained 650 RPM and the manual preset
35%; this short observation follows the power fault and is not independent
speed calibration. Final climate was 24.8°C / 54.3% RH in that monitor. The board
was left in fixed RPM 650 with default 25/35, 40/65, 80/90 curve, and THERMAL off.
The failure-exit reason remains `sensor` until the next deliberate activation.
Serial monitoring was stopped and the port closed. Seven static payloads were
also checked again after peripheral Flash power recovery.
