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
All four SPI pins remain configurable; GPIO 13 controls the fan-supply relay.
The adapter checks GPIO collisions at compile time. SPI speed defaults to
10 MHz and is configurable in `Config::Storage`.

## Build and upload

First follow [build environment](building.md) for macOS, Linux or Windows tool
installation, the ESP32 core/libraries, the `.venv` environment and serial-port
permissions. The examples below use only repository-local `build/` paths and
run from the repository root. Close any serial monitor before uploading.

The build script stages only the selected sketch, with shared files copied from
this checkout. Both programs therefore use the **same Config.h**. The uploader
uses SPIMemory and the Adafruit SSD1306/GFX/BusIO libraries for optional OLED
progress; it does not run the fan application or change NVS. Its OLED uses the
same I2C pins, address and dimensions from Config as the application.

The Web Panel can cancel a pending updater selection before restarting, returning
the next boot target to app0 without changing fan settings.

Starting with **1.5**, the main application resides in **app0** and the serial
resource uploader resides in **app1**. Install both once; later asset updates
only need boot selection, a manual restart and the existing serial transfer.
See [resident updater](updater.md) for boot semantics, API, button timing and recovery.
Do not use Arduino CLI's normal uploader-sketch upload: it writes app0.

### Initial installation: macOS / Linux shell

Use the actual port (`/dev/ttyACM0` is a common Linux example). Install esptool
in the same virtual environment as pyserial (Python 3.10+ for esptool 5.x), then build both sketches using the
same FQBN and Config. Run commands individually and stop on a failure.

```sh
FM_PYTHON=./.venv/bin/python
FM_PORT=/dev/cu.usbmodem101
"$FM_PYTHON" -m pip install esptool==5.3.1
"$FM_PYTHON" scripts/build_firmware.py main --work-dir build/main
"$FM_PYTHON" scripts/build_firmware.py uploader --work-dir build/uploader
"$FM_PYTHON" scripts/install_updater.py --port "$FM_PORT" --install-both --dry-run
"$FM_PYTHON" scripts/install_updater.py --port "$FM_PORT" --install-both
"$FM_PYTHON" scripts/assets.py pack --output build/fanmaster-assets.bin
"$FM_PYTHON" scripts/assets.py verify build/fanmaster-assets.bin
```

The installer checks both generated partition tables and application role
markers, then writes the bootloader, partition table, OTA initialization, main
image at `0x10000` and uploader at `0x150000`. It does not erase/write the NVS
partition. The default 4 MiB partition layout is required. `--dry-run` prints the
write plan without accessing a device. To install/update **only app1**, omit
`--install-both`; the same build directories are used to check the layout.

For the first asset upload, send **`boot updater`** followed by Enter in the main
firmware's serial console. It sets the next boot to app1 and does **not** restart.
Close the console, manually press RESET, recheck the USB port, then run:

```sh
"$FM_PYTHON" scripts/assets.py upload build/fanmaster-assets.bin --port "$FM_PORT"
```

After `DONE verified`, keep the transfer command running: it holds USB open
through the ten-second countdown and reports `RETURN app0 observed` after restart.
There is no main-firmware reflash step. The new Web Panel becomes available after
this transfer. Future entries can use its maintenance button, the serial command,
or twenty clicks of button 4.

### Initial installation: Windows PowerShell

Set the actual COM port from Device Manager or `arduino-cli board list`. `&`
invokes the Python executable stored in `$FmPython`.

```powershell
$FmPython = "./.venv/Scripts/python.exe"
$FmPort = "COM5"
& $FmPython -m pip install esptool==5.3.1
& $FmPython scripts/build_firmware.py main --work-dir build/main
& $FmPython scripts/build_firmware.py uploader --work-dir build/uploader
& $FmPython scripts/install_updater.py --port $FmPort --install-both --dry-run
& $FmPython scripts/install_updater.py --port $FmPort --install-both
& $FmPython scripts/assets.py pack --output build/fanmaster-assets.bin
& $FmPython scripts/assets.py verify build/fanmaster-assets.bin
```

Send `boot updater` in the main serial console, close it, restart manually and
recheck `$FmPort`. Then transfer:

```powershell
& $FmPython scripts/assets.py upload build/fanmaster-assets.bin --port $FmPort
```

The return to app0 is automatic ten seconds after verification. Later resource
updates reuse the resident uploader; no application replacement is needed.
While waiting or after an error, hold uploader button 4 for ten seconds to
select app0 and restart offline. RESET alone stays in app1 until a successful
upload or explicit return; this avoids USB-open resets bouncing out of recovery.

### Transfer results and subsequent updates

The transfer script never guesses a port or flashes ESP32 firmware. It sets
DTR/RTS low before opening native USB to avoid accidental ROM download mode.
If the board stays in download mode, release BOOT, press RESET, identify the
port again and retry. See [host troubleshooting](building.md#troubleshooting).

The uploader erases only the sectors occupied by the new image, checks every
page by reading it back, and verifies the image CRC. It writes the header last.
`DONE verified` confirms the serial transfer and flash readback, not browser or
radio acceptance. Updating resources replaces the old image; a failed or
interrupted transfer requires another upload. Old trailing sectors are outside
the new index and are never served.

The OLED shows waiting/flash initialization, erase progress, write/readback
progress in bytes and percent, CRC/header validation, header commit, then
`Done verified` with a ten-second app0 return countdown. An error replaces progress with its cause (for example
`Serial timeout` or `CRC/header invalid`). Success appears only after final
header readback passes. Updates use `Config::Oled::REFRESH_INTERVAL_MS` (250 ms),
with immediate phase changes and completion updates. A missing OLED or failed
I2C initialization leaves the serial uploader usable; display messages never
enter the serial protocol.

For an application-only update, build main and use the usual Arduino CLI upload
for `build/main/fanmaster` with `--input-dir build/main/build`. It writes app0
and selects it, retaining app1, external assets and NVS. Do not erase the whole
chip. For changes to `web/`, pack/verify the image, select the resident updater,
restart manually, and transfer over serial. Neither webpage needs Internet-hosted
assets. Build/download tools need Internet only when dependencies are missing.

If your Python is older than 3.10 or you already have Arduino's standalone
esptool, skip the pip install and pass `--esptool "/path/to/esptool"` to the
installer commands. On Windows the executable is typically named `esptool.exe`.
The bundled path is host-specific; locate it in the installed ESP32 tool package.

### Arduino IDE

Add `--prepare-only` to either build-script command to stage shared files without
compiling. Open `build/main/fanmaster/fanmaster.ino` or
`build/uploader/ResourceUploader/ResourceUploader.ino` in Arduino IDE, select
ESP32S3 Dev Module and the actual port, and enable USB CDC On Boot to match the
CLI target. Edit original sources in this checkout and stage again after changes.
For a custom CLI or library location, pass `--arduino-cli "path/to/executable"`
or `--libraries "path/to/libraries"` to the staging/build script. The optional
[Apple Silicon ctags workaround](building.md#apple-silicon-ctags-issue) applies
only to affected macOS toolchains.

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

The network task on core 0 runs at idle priority and owns Wi-Fi, DNS, resource
reads and HTTP. Sharing idle priority lets IDLE0 run even when the synchronous
parser yields without sleeping; task watchdog protection stays enabled. The
Arduino loop task on core 1 owns buttons, serial commands, RPM, LED, SHT30 and
OLED. No local-control callback runs from resource verification/transmission.
The synchronous WebServer can pause on request parsing or socket writes, but
these waits no longer stop local controls. PWM remains hardware-driven and
TACH interrupts keep counting. Cached climate data and duty updates are
synchronized across tasks. OLED refresh is limited to once per 250 ms.
USB CDC logging uses a zero transmit timeout, so a missing/stalled serial
reader cannot hold up controls. `[LOCAL]` reports control-loop gaps separately
from `[WEB]` network-task gaps. Measure both on the actual board.

## Provisioning behavior

After returning to app0, use the host's Wi-Fi controls to join
`Fanmaster-Setup-<random suffix>` if setup is active:

| Host | Wi-Fi controls |
| --- | --- |
| macOS | Control Center or System Settings → Wi-Fi |
| Linux desktop | Desktop Wi-Fi menu / NetworkManager connection settings |
| Windows | Quick Settings or Settings → Network & Internet → Wi-Fi |

Stay connected even if the OS reports no Internet, and manually open
`http://192.168.4.1/` if a captive window does not appear. After saving and
connecting successfully, the setup AP closes; reconnect the host to the target
LAN and open `http://<station-IP>/` using the address shown on OLED/serial.
The station IP is assigned by the LAN and is not necessarily the address from
the hardware test report. Browser proxies/VPN routing can affect private
addresses independently of the firmware.

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

On macOS/Linux, with the optional host-test tools installed:

```sh
sh scripts/check_host.sh
node tests/test_setup.mjs
```

For prerequisites and the Windows/WSL host-test route, see
[build environment](building.md#optional-host-checks). Host tests exercise the resource
packer and serial handshake, Wi-Fi state and
NVS failures, HTTP routing and streaming, recovery form, and interrupted uploader
commits using in-memory peripheral substitutes. The JavaScript checks cover
submission, stale responses, channel-change retries and terminal results.
They do not establish successful SPI or Wi-Fi operation on the ESP32.

## Updating the temperature-control panel

The three-point THERMAL widget adds `/assets/thermal.js` to the external image
(seven resources total). Repack and upload the complete image and rebuild/flash
the main application when its API changes. For resource-only edits, use the
resident updater sequence above on each host OS. The
uploader transfer protocol is unchanged. The current uploader also forces the
fan supply relay off. Update assets and firmware
together, then reload the panel. See [temperature control](thermal.md) for
mode restoration, sensor fallback, NVS migration and acceptance checks.
