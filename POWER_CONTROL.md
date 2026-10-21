# Fan supply relay and power controls

GPIO **13** controls the input of an external relay module switching the fan's
12 V DC supply. `Config::Pins::FAN_POWER` selects the pin;
`Config::Power::RELAY_ACTIVE_HIGH` defaults to `true` (HIGH = supply on).
Set it to `false` for an active-low input. This is a module/driver input, not a
GPIO connection to a bare electromagnetic coil. The ESP32, OLED, SHT30 and SPI
Flash remain powered; only the fan supply is switched.

The configured active level must correspond to the contact wiring that supplies
the fan. Software reports the commanded relay state; there is no contact or
12 V feedback input. During chip reset, before application code runs, pin state
is controlled by hardware/boot ROM; the relay input's external bias must establish
the desired default-off state. The application first registers GPIO 13 as input,
preloads its inactive output latch, then enables output. ESP32 Arduino 3.x does
not honor `digitalWrite` on a pin that has not yet been registered as GPIO.

## Boot and first power-on

Every ESP32 boot/reset starts with the relay **off** and PWM at 0%, independent
of NVS presets. The selected initial control mode is still Duty; saved fixed
presets/curve load under the existing operating-memory switch. OLED stays on
with the prompt, wrapped over two lines:

> Press any key to power on.

A fresh, debounced press of any of the four buttons enables power and consumes
that gesture. Hold/release/repeat edges cannot also change duty, mode or LED.
Keys already held during initialization must be released before a fresh press.
The Web Panel can also explicitly enable power. Power state is not stored in NVS.

Each off-to-on transition applies the configured full-duty startup boost
(`Fan::STARTUP_TIME_MS`, default 1000 ms), then resumes the retained mode/output.
This boost uses elapsed-time checks in the local loop, not a blocking delay in
an HTTP handler. Repeating an already-on command does not restart it. During the
boost, PWM stays at 100%; accepted mode/setpoint changes take effect afterwards.

## Local power-off

Only hold the function key **button 4 (`*`) alone for eight seconds** to power
off locally. Timing starts after its debounced press. Any other button already
held, subsequently pressed or released during that session cancels shutdown.
The ordinary adjustment/LED chord behavior still applies to those other keys.
Releasing the other key does not restart shutdown while `*` stays held; release
`*` and press again for a new session. Raw release and raw other-key presses take
precedence over the timer, including on the threshold iteration. Input is still
sampled by the local loop, so events wholly between samples cannot be detected.
There is one shutdown event per hold, with rollover-safe elapsed-time checks.

On power-off, the relay opens, applied PWM becomes 0%, RPM feedback stops
advancing its filter/integral, and OLED receives SSD1306 `DISPLAYOFF`. This is
display sleep, not removal of the OLED's own 3.3 V supply. TACH telemetry continues
to sample coasting/stopped rotation; a power-off response need not report zero
RPM immediately. Wi-Fi, sensor sampling and API access continue running.

## Local wake after manual power-off

Any debounced key press/release shows this exact message for **1500 ms**, with
line breaks to fit the 128×64 display:

> Hold ^ and * for 1 second to power on the fan.

A later key edge restarts the hint interval; holding a key does not refresh the
hint through its adjustment-repeat events. Afterwards the OLED sleeps again.
Buttons otherwise do not edit settings, switch mode or toggle the LED while off.

Hold **button 1 (`^`) + button 4 (`*`) together for one continuous second** to
restore power. Either press order works. Releasing either key, or holding button
2/3, resets this wake interval. Once the extra key is released, both intended
keys must again remain down for a full second. The complete wake gesture is
consumed until every key has been released, preventing an accidental fine step,
mode click or new shutdown countdown. A web power transition also consumes any
in-progress local gesture until release.

## Retained controls

Power-off retains the preceding **Duty / RPM / THERMAL** mode and its settings
in RAM. Power-on resumes it after the startup boost. In RPM mode, resume from the
pre-off applied duty with a reset regulator; stopped/kick pulse counts are
excluded from the first new feedback window, preventing integral windup while
the supply is off. Interrupting a boost never saves its 100% output as the normal
resume duty.

Temperature validity is still checked while off. A failed/non-finite/stale sample
exits retained THERMAL to its preceding fixed mode, and recovery never re-enables
THERMAL automatically. Startup completion also checks freshness before restoring
a thermal output. See [THERMAL_CONTROL.md](THERMAL_CONTROL.md).

Fixed changes and enabling THERMAL are blocked while power is off. The saved
curve remains editable, including through the API, without closing the relay.
Power toggles do not replace the existing mode-exit policy for NVS presets, do
not save automatic output, and do not persist the selected mode or power state.

## HTTP and Web Panel

`POST /api/fan/power` accepts form data `enabled=0` or `enabled=1`; success returns
the full status JSON immediately. Missing, fractional, negative, overflowing or
non-0/1 values return HTTP 400 without changing power. Commands are explicit and
idempotent rather than relative toggles. No request waits for the startup boost.

`GET /api/status` and successful commands include:

| Field | Meaning |
| --- | --- |
| `power_enabled` | Commanded relay supply state |
| `power_starting` | Full-duty startup boost currently active |
| `power_waiting_for_key` | Initial boot prompt accepts any key; false after first power-on |
| `mode` | Selected/retained Duty, RPM or THERMAL mode even while off |
| `duty_percent` | Applied PWM: 0 while off, 100 during startup boost |

Valid fixed-duty/RPM/mode changes and enabling THERMAL while off return **409**.
Curve editing is available. The panel's power button shares the existing command
lock/status poller; local changes appear on the next successful poll, and older
poll responses cannot undo a newer command. Fixed controls and the thermal toggle
are disabled while off, while the curve editor remains available.

Repack/upload the updated seven-resource image and flash the main application.
The uploader also initializes the relay **off** and never enables it. Its OLED
upload progress remains active; its protocol and NVS behavior are unchanged.
See [FLASH_RESOURCES.md](FLASH_RESOURCES.md).

## Validation

`sh scripts/check_host.sh` includes power-gesture and controller/OLED tests for
8-second thresholds, interruption lockout, raw release, pre-held keys, boot-any-key,
1-second chord in either order, consumed releases/repeats, timer rollover,
boot-off GPIO/PWM state, nonblocking startup, idempotent requests, suspended PI,
retained modes, sensor fallback, interrupted boosts and 1.5-second OLED sleep.
HTTP tests cover parsing/status/409 behavior and curve editing while off. The
fixed/thermal Node.js suites cover power commands, local-state polling, disabled
handlers, errors and shared command locking. Active-low configuration is also
compiled/exercised in an isolated host-test build.

Target-board results and remaining physical relay checks are recorded separately
in [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md). No software-state test alone
establishes relay contact operation or removal of the fan's actual 12 V supply.
