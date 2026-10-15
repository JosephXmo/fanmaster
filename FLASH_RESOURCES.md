# External web resources and Wi-Fi provisioning

The application reads static resources from a W25Q64 (64 Mbit / 8 MiB) using
[SPIMemory 3.4.0](https://github.com/Marzogh/SPIMemory). Dynamic HTTP routes stay
in the ESP32 application. Wi-Fi credentials stay in ESP32 NVS, separately from
both the resource image and the source configuration.

## Wiring

Use a module suitable for 3.3 V supply and logic. Check its exact part marking
and module specification before powering it; not every W25Q64 variant uses the
same supply voltage. All grounds must be common.

| W25Q64 connection | ESP32 default | Config.h setting |
| --- | --- | --- |
| CLK | GPIO 11 | `Pins::FLASH_SCK` |
| DI / IO0 (MOSI) | GPIO 5 | `Pins::FLASH_MOSI` |
| DO / IO1 (MISO) | GPIO 6 | `Pins::FLASH_MISO` |
| /CS | GPIO 12 | `Pins::FLASH_CS` |
| /WP / IO2 | Pull high for ordinary SPI | Module wiring |
| /HOLD / IO3 | Pull high for ordinary SPI | Module wiring |

**Connect /CS to GPIO 12, retaining a pull-up if present. Tying /CS directly to
3.3 V prevents all communication.** A single peripheral still needs /CS edges
to delimit commands. Firmware 1.4 adopts the carrier wiring from the telemetry
branch: fan TACH/PWM on GPIO 1/2, I2C on GPIO 3/4, buttons on GPIO 7..10,
and RGB LED on GPIO 21. GPIO 11/5/6/12 are therefore available for SPI.
All four SPI pins remain configurable; GPIO 13 is also unused in this layout.
The adapter checks GPIO collisions at compile time. SPI speed defaults to
10 MHz and is configurable in `Config::Storage`.

## Build and upload

Install ESP32 Arduino core 3.x and these libraries using Arduino Library
Manager or `arduino-cli lib install`:

- `SPIMemory` 3.4.0
- `FastLED` 3.10.7
- `Adafruit SSD1306`, `Adafruit GFX Library`, `Adafruit BusIO`
- Sensirion `arduino-sht` 2.0.0

The build script stages only the selected sketch, with shared files copied from
this checkout. Both programs therefore use the **same Config.h**. The uploader
needs only SPIMemory; it does not run the fan application or change NVS.

1. Build the resource image. HTML/CSS/JS and other assets go in `web/`; no
   Internet-hosted assets are required by either page.

   ```sh
   python3 scripts/assets.py pack --output /tmp/fanmaster-assets.bin
   python3 scripts/assets.py verify /tmp/fanmaster-assets.bin
   ```

2. Build and flash the dedicated uploader. Replace the serial port below with
   the board's actual port. Close Arduino Serial Monitor first.

   ```sh
   python3 scripts/build_firmware.py uploader --work-dir /tmp/fanmaster-uploader
   arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
     --port /dev/cu.usbmodemYOUR_PORT \
     --input-dir /tmp/fanmaster-uploader/build \
     /tmp/fanmaster-uploader/ResourceUploader
   ```

3. Install pyserial into a Python environment, then transfer the image. No
   firmware flashing or serial-port guessing happens in this command.

   ```sh
   python3 -m venv /tmp/fanmaster-python
   /tmp/fanmaster-python/bin/python -m pip install pyserial
   /tmp/fanmaster-python/bin/python scripts/assets.py upload \
     /tmp/fanmaster-assets.bin --port /dev/cu.usbmodemYOUR_PORT
   ```

   The uploader erases only the sectors occupied by the new image, checks every
   page by reading it back, and verifies the image CRC. It writes the header
   last. `DONE verified` confirms the serial transfer and flash readback, not
   browser or radio acceptance. Updating resources replaces the old image; a
   failed/interrupted transfer requires another upload. Old trailing sectors
   are outside the new index and are never served.

4. Build and flash the application without erasing all ESP32 flash/NVS:

   ```sh
   python3 scripts/build_firmware.py main --work-dir /tmp/fanmaster-main
   arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
     --port /dev/cu.usbmodemYOUR_PORT \
     --input-dir /tmp/fanmaster-main/build /tmp/fanmaster-main/fanmaster
   ```

   The external image remains intact. Subsequent firmware updates need only
   this step unless the web resources change. To use Arduino IDE, pass
   `--prepare-only` to the build script and open its printed sketch directory.
   To supply an explicit CLI or extra libraries, use `--arduino-cli PATH` or
   `--libraries DIRECTORY`. Staged copies are generated build inputs; edit the
   original files in this checkout and stage again after any changes.

## Resource format and serving

The image has a 32-byte little-endian header, followed by up to 64 fixed
160-byte entries and contiguous file contents. Each entry carries a path,
content type, offset, byte length and CRC32. The header carries format version,
image length and checksums for the header, index and payload. The host verifies
all checksums before starting an upload. The application validates the header,
index, bounds and ordering at boot, then checks each file's CRC on its first
request after boot. Checksums detect accidental corruption, not malicious edits.

The adapter probes JEDEC identity before SPIMemory initialization and checks
status before runtime reads, so an absent or unselected chip can use the recovery
path without entering the library's long busy wait.

The resource store is read-only in the running fan application. It uses no
filesystem formatting or mount-time writes. Serving uses a 1 KiB buffer and
explicit byte lengths; binary resources are supported too. Large pages are not
copied into a whole-page `String`. Runtime settings should be obtained through
APIs; templates are not rewritten in external HTML. Browser telemetry continues
at one-second intervals.

Local button, serial, RPM and LED processing runs between resource verification
and transmission chunks. PWM remains hardware-driven and TACH interrupts keep
counting. The existing synchronous WebServer can still pause on request parsing
or slow socket writes, so this is not a hard real-time server. OLED refresh is
limited to once per 250 ms. Verify maximum loop gaps on the actual board.

## Provisioning behavior

- No valid NVS record: immediately open `Fanmaster-Setup-<random suffix>`.
  A random suffix is generated once per boot and reused on retries; the
  application does not read or cache the MAC address.
- Saved credentials: try STA first, retry with a 15-second attempt timeout and
  10-second retry delay. Open setup after 45 seconds of continuous failure.
  User submissions can replace a background retry immediately; only another
  user-submitted candidate returns a busy response.
- Join the open setup network and open `http://192.168.4.1/`. Catch-all DNS and
  HTTP redirects handle common captive-portal probes; OS auto-opening is not
  guaranteed, so the explicit address remains available.
- Submit SSID (1..32 UTF-8 bytes) and password (empty for open networks, 8..63
  bytes for WPA passphrases, or 64 hexadecimal characters for a raw key).
- A candidate is held only in RAM until the target network connects. A failed
  candidate leaves the previous NVS record intact and retries that record.
- Save a successful pair as one CRC-protected NVS blob in namespace `fanmaster`,
  key `wifi`. If saving fails, keep setup available and report `storage_failed`.
- After success and a five-second grace period, close DNS and the open AP and
  return to station mode. The UI shows the station address before closing.
- The setup AP stays available while setup is needed, including after an
  unsuccessful replacement. Timings and AP prefix are configurable in Config.h.
- Configuration POSTs are accepted only while setup is active and only through
  the AP interface. Passwords are never logged or echoed in responses. NVS is
  not encrypted by this application, and the open provisioning link is not
  encrypted. Credentials are no longer embedded in Config.h; existing Git
  history is not rewritten.

`GET /api/wifi/status` reports `idle`, `connecting`, `saved`,
`connection_failed`, or `storage_failed`, plus connection state and station IP.
`POST /api/wifi/configure` accepts URL-encoded `ssid` and `password`; returns
202 while connecting, 400 for invalid input, 409 while another submitted
candidate is in progress, or 403 outside setup/AP access. The regular fan API remains unchanged.

If external storage or a setup-page dependency is unavailable, a tiny embedded
HTML form still permits provisioning. The full dashboard requires the resource
image. Use the OLED or serial console to discover the station IP after reboot.

## Target acceptance

After checking /CS wiring, validate on the ESP32:

1. Upload and read back the resource image; reboot into the main firmware.
2. With no saved record, confirm DNS/redirects and manual setup URL work.
3. Submit wrong credentials, then correct credentials; check failed attempts do
   not overwrite a working record and success closes the AP.
4. Power-cycle and verify automatic reconnection from NVS.
5. Make the configured network unavailable; confirm setup returns after the
   bounded failure period while fan PWM, TACH, buttons and LED keep working.
6. Test flash absence and an interrupted upload; the recovery form must remain
   usable. Record loop gaps and heap while fetching dashboard assets.

## Local verification

```sh
sh scripts/check_host.sh
node tests/test_setup.mjs
```

Host tests exercise the resource packer and serial handshake, Wi-Fi state and
NVS failures, HTTP routing and streaming, recovery form, and interrupted uploader
commits using in-memory peripheral substitutes. The JavaScript checks cover
submission, stale responses, channel-change retries and terminal results.
They do not establish successful SPI or Wi-Fi operation on the ESP32.

The installed Arduino `ctags` binary on this Apple Silicon host is still Intel
only and fails with `bad CPU type in executable`. Validation uses a temporary
native build of Arduino ctags 5.8-arduino11 (with its reserved `__unused__` macro
renamed to compile against the current macOS SDK), without changing the global
Arduino installation. While that temporary tool is available, append:

```sh
--build-property runtime.tools.ctags.path=/private/tmp/fanmaster-ctags-native/bin
```

This path is a local build workaround, not a firmware dependency. A native
Arduino ctags installation or a working Intel compatibility runtime removes
the need for the override.
