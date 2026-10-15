# ESP32-S3-Zero hardware acceptance — 2026-10-15

This records measured results from the connected Waveshare ESP32-S3-Zero,
not just host substitutes or successful compilation. Firmware starts from
merge commit `858150f` (version 1.4), with the working-tree fixes described below.

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
