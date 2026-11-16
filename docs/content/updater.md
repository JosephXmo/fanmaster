# Resident W25Q64 updater — firmware 1.5

The default 4 MiB partition layout is unchanged. **app0** (`0x10000`, 1,310,720
bytes) holds Fanmaster; **app1** (`0x150000`, 1,310,720 bytes) holds only the
ResourceUploader. `otadata` stores the selected boot target. Neither application
updates the other application image over the network. Resource writes still use
the existing `PING` / `UPLOAD` / `ACK` / `DONE verified` USB serial protocol.

## Select the next boot

All three entry points set app1 as the **next boot target only**. The running
main application, fan mode, power setting, display and network continue normally.
Restart manually using RESET or a full board power cycle when ready.

- Web Panel: use **Use updater on next boot** in the maintenance card. While
  selected, the same button becomes **Cancel updater on next boot**. Cancelling
  restores app0 as the next boot target; neither action restarts the board or
  changes fan settings. Selection through buttons or serial can be cancelled
  from the panel too, after its next status poll.
- Serial console: send **`boot updater`**, followed by Enter.
- Physical input: click **button 4 twenty times**. A click requires a debounced
  press and release. Consecutive releases must be no more than 1000 ms apart;
  holds of 1000 ms or longer and any other key activity cancel the count. These
  limits are in `Config::Updater`. The counter works while the fan relay is on
  or off, with the ESP32 powered. Normal boot-any-key wake and long-hold power
  gestures keep their existing behavior; a long hold never counts as clicks.

Selection checks the destination partition, standard image description and a
Fanmaster-specific role marker. ESP-IDF verifies the image before changing
otadata. A missing, unreadable, wrong-role or invalid destination is rejected;
the current application keeps running. The Arduino SDK's default project name
is shared across sketches, so it is insufficient to identify the updater.
Both 1.5 images contain a 40-byte `.rodata_custom_desc` record immediately after
the standard application descriptor (image offset 288): `FMBOOT1\0`, role[24],
version[8]. This is format/role identification, not cryptographic authentication.
Older images lack this record and must be replaced during the initial install.

`GET /api/status` includes `firmware_version`, `updater_available` (readable role
descriptor), and `updater_next_boot` (current otadata selection). Availability
is not a full-image checksum test; selection performs the SDK image verification.
`POST /api/updater/select` takes no parameters and returns the full status on
success (200), 503 for a missing/wrong-role updater, or 500 if boot selection
fails. It is accepted with fan power on or off and during THERMAL. The Web Panel
uses the existing command lock and status poller, so a local/serial selection
appears on the next successful poll. It does not trigger a reboot or enable power.
`POST /api/updater/cancel` also takes no parameters and returns full status (200)
with `updater_next_boot: false` after validating/selecting app0. It returns 503
if app0 is missing/wrong-role or 500 if SDK selection fails; the prior selection
remains unchanged on failure. It is idempotent, works even if app1 is unavailable,
and uses the same shared command lock. A selection from any client is reflected
in other panels by polling. Cancellation is served by app0, before entering the
uploader; use RETURN or the ten-second button hold once in app1.

## In the uploader

On entry, the uploader disables the fan relay and **keeps app1 selected** while
waiting/transferring. Opening native USB/JTAG serial can reset the tested board;
selecting app0 on entry would therefore bounce back to main when the transfer
program opens the port. A reset during waiting/failed upload stays in app1 for
retry. App0 is selected after verified success or an explicit escape operation.

The uploader has no Wi-Fi service, fan control loop or credential access. OLED
progress remains optional. It erases the resource-image sectors, verifies pages,
checks CRC/header and commits the header last. Only a successful committed upload
emits `DONE verified`, selects app0 and starts the **10,000 ms** countdown. The OLED keeps 100%
and displays the seconds until app0. The timer is nonblocking and rollover-safe;
new upload commands are ignored during it. At expiry the uploader reselects and
validates app0, then restarts. The host script keeps the serial port open until
it observes the main startup banner (up to 15 seconds); closing/reopening USB
must not interrupt the countdown. A missing serial reader does not block the
firmware timer. A separate manual/USB reset after success can return sooner.

Failures leave the updater running for a retry and do not arm the success timer.
Send **`RETURN`** plus Enter to return to app0 without a successful upload.
For offline escape, **hold button 4 for ten seconds** (`Updater::EXIT_HOLD_MS`):
the updater selects app0 and restarts immediately. The initial press is debounced;
raw release cancels the timer. Other keys do not affect this hold. It works
while waiting, after errors, during the success countdown and at upload service
points, including serial-byte waits. Individual SPI erase/program operations
remain synchronous. Booting with button 4 held also starts the hold after debounce.
The idle/error screen shows `Hold * 10s: app0`; a missing OLED does not disable it.
If app0 cannot be selected, the
uploader reports the error and stays running instead of intentionally rebooting
into an unknown image. Reinstall a valid main image over USB in that case.

Interrupted resource replacement can leave an invalid asset header. Main still
has its embedded provisioning fallback, and the serial/button updater entries
work without external webpages. Full dashboard assets require another transfer.
Fanmaster restarts with its normal relay-off / any-key boot prompt, rather than
resuming an energized fan automatically. Power state is not persisted.

## Installation and routine updates

Follow [installation and assets](resources.md) for macOS/Linux/Windows commands.
Build both sketches with the same FQBN/Config. Run `scripts/install_updater.py`
with `--install-both --port <port>` to install both slots and initialize boot
choice to app0 without writing NVS. Without `--install-both`, it writes only app1
and leaves otadata untouched. The installer requires matching generated default
partition tables, checks image roles/sizes and supports `--dry-run`. Use it instead
of a normal Arduino upload for the uploader: normal uploads target app0.

Once installed, a resource update consists of packing/verifying assets, selecting
app1, manually restarting and running `scripts/assets.py upload`. The successful
transfer returns to app0 automatically; rebuilding/reflashing either application
is unnecessary unless that application's code changes. USB enumeration can change
the port, so close serial monitors and check the actual port before each transfer.

The application never writes credential/operating NVS during slot selection or
resource upload. Boot selection changes the separate OTA metadata partition.
No application OTA endpoint, updater self-update, rollback guarantee or image
download service is added by this feature.
