# Fanmaster

ESP32-S3 fan control, from a physical button to a local browser.

A four-wire PWM fan, one shared controller state, and three ways to set its
output: manual duty, fixed RPM and a three-point temperature curve. Fanmaster
1.5 keeps the main application in app0 and a resident serial resource uploader
in app1.

<div class="system-map" aria-label="System overview">
  <div><span class="map-label">ESP32 · APP0</span><strong>Fanmaster 1.5</strong><span>Control · telemetry · Wi-Fi</span></div>
  <span class="map-arrow" aria-hidden="true">←</span>
  <div><span class="map-label">W25Q64 · SPI</span><strong>Web resources</strong><span>Panel · setup · HTML / CSS / JS</span></div>
  <span class="map-arrow" aria-hidden="true">←</span>
  <div><span class="map-label">ESP32 · APP1</span><strong>Serial updater</strong><span>Write · verify · return to app0</span></div>
</div>

## Start here

<div class="link-grid">
  <a href="hardware.md"><span>01 / CONNECT</span><strong>Hardware &amp; pins</strong><p>ESP32-S3-Zero, four buttons, SHT30, SSD1306 and SPI Flash.</p></a>
  <a href="building.md"><span>02 / BUILD</span><strong>Prepare your host</strong><p>Arduino CLI and library setup for macOS, Linux and Windows.</p></a>
  <a href="resources.md#build-and-upload"><span>03 / INSTALL</span><strong>Install both applications</strong><p>One initial installation, then serial-only web-resource updates.</p></a>
  <a href="controls.md"><span>04 / OPERATE</span><strong>Use the controller</strong><p>Local buttons, fan power, fixed modes and temperature override.</p></a>
</div>

## What it controls

| Mode | Input | Behavior |
| --- | --- | --- |
| Duty | 0–100% manual duty | Apply the selected PWM directly |
| RPM | Whole RPM within Config limits | Filtered PI feedback with deadband and slew limiting |
| THERMAL | Three ordered temperature/duty points | Linear interpolation, clamped outside the endpoints |

The Web Panel, serial commands and physical controls act on the same controller.
The panel polls authoritative state once per second; local mode changes appear
on the next successful poll. A temperature failure exits THERMAL to the preceding
fixed mode rather than silently choosing a new fan policy.

## Power and memory

Every boot starts with the fan power command **off** and the OLED showing
“Press any key to power on.” A fresh button press or the Web Panel enables it;
manual power-off later uses the configured hold gestures.

With operating memory enabled, exited fixed setpoints and saved thermal curves
are stored in NVS. Wi-Fi credentials are in a separate NVS record. The active
mode and relay power state are not restored on boot. External Flash contains
web resources, not credentials or this documentation website.

## Update web resources

Select the next boot through the maintenance card, `boot updater` on serial,
or twenty clicks of button 4. The Web Panel can cancel that selection before
restarting. Restart manually, transfer the resource image over serial, then
wait for verification and the ten-second automatic return to app0.

The uploader also supports a ten-second button-4 hold to escape offline.
See [resident updater](updater.md) for exact timings, retries and USB reset behavior.

## Project status

The last deployed 1.5 build passed target tests for next-boot selection,
cancellation, SPI transfer/retry, delayed return, and user-observed buttons/OLED.
Earlier RPM and temperature acceptance is preserved with its measurement limits.
Physical relay/contact switching and Linux/Windows runtime acceptance remain
separate from those results. See [validation](validation.md) and
[Flash layout](storage.md) for the recorded images and evidence.

This reference website is built on the host from `docs/`; it has no live device
controls and is independent of the seven files packaged from `web/`.
