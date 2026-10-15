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
