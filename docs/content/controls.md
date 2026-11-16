# Local controls

Button 1 is increase (`^`), button 2 is decrease, button 3 selects control mode,
and button 4 (`*`) is the function key. All four inputs are active low and use
30 ms debounce by default. These values and pin mappings live in `src/fanmaster/Config.h`.

## Normal operation

| Gesture | Result |
| --- | --- |
| Button 1 / 2 | Increase / decrease the active fixed setpoint |
| Hold button 1 / 2 | Step immediately, wait 1000 ms, then repeat every 100 ms |
| Function + button 1 / 2 | Fine adjustment |
| Click button 3 | Switch Duty ↔ RPM; in THERMAL, only exit the override |
| Function + button 3 | Toggle RGB indication without switching mode |
| Hold function alone for 3000 ms | Turn fan power off |
| Click function twenty times | Select app1 for the next manual restart |

Function + button 3 works in either press order and emits one LED toggle per
button-3 hold. Releasing the chord does not emit a mode-switch click. Standalone
button 3 changes mode on release. Opposing adjustment buttons cancel. Delayed
loops do not replay a burst of missed repeat events.

| Setting | Normal step | Fine step | Bounds |
| --- | ---: | ---: | --- |
| Manual duty | 5% | 1% | 0–100% |
| Fixed RPM | 100 RPM | 10 RPM | 550–1700 RPM by default |

Set steps under `Config::Buttons`; set RPM bounds under `Config::RpmControl`.
During THERMAL, buttons 1/2 and fixed Web Panel controls are disabled. Exit the
override first to adjust a fixed setpoint.

## Boot wake and later wake

On boot, fan power is off and OLED displays “Press any key to power on.”
Any fresh button press powers on and is consumed: it does not also change the
mode, duty or LED. Keys held at initialization must be released first.

After a manual shutdown, OLED sleeps. Key edges briefly show the wake hint,
and **buttons 1 + 4 held together for 500 ms** restore power. The complete wake
gesture is consumed until all keys are released. Web power-on is also available.
A one-second 100% PWM startup boost precedes the retained mode.

The current Config shutdown/wake values are **3000 ms / 500 ms**, respectively.
The OLED wake hint currently says “Hold ^ and * for 1 second to power on the fan.”;
that fixed wording is not a measurement of the configured 500 ms threshold.
Historical test records may describe earlier 8 s / 1 s thresholds.

## Shutdown cancellation

Any other key activity during a function-only shutdown hold cancels that entire
hold session. Releasing the other key does not resume the countdown; release
function too and begin a new hold. This prevents a fine adjustment or LED chord
from unexpectedly powering off the fan. See [fan supply control](power.md).

## Updater gestures

The twenty-click counter works with the fan power on or off while the board is
powered. A click requires a press and release; successive releases must be no
more than one second apart. A hold of one second or another key interrupts the
count. Selecting the updater does not reboot or suppress the normal boot-any-key
wake rule. The Web Panel can cancel a local selection.

Inside app1, hold button 4 for ten seconds to select app0 and restart immediately.
Shorter holds leave the uploader running. See [resident updater](updater.md).

## Serial console

Use 115200 baud with a newline terminator. Close competing monitors before
firmware flashing or resource transfer. Opening native USB/JTAG serial can reset
the tested board; see the updater guide before reconnecting during an upload.

| Command | Effect |
| --- | --- |
| `35` | Select Duty at 35% |
| `rpm 850` | Select RPM control at 850 RPM |
| `mode duty` / `mode rpm` | Switch fixed mode using its existing setpoint |
| `boot updater` | Select app1 for the next boot, without restarting |

Fixed commands are rejected while power is off or THERMAL is active. Serial
commands do not provide a power-on or thermal-curve editor. In app1, the serial
protocol is `PING` / `UPLOAD` / `ACK` / `DONE verified`; `RETURN` explicitly
selects app0 and restarts. The host transfer script handles normal uploading.
