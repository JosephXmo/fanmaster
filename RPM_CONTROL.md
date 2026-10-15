# Fixed RPM control

Button 3 now switches between manual duty and fixed RPM on release. Buttons 1/2
adjust the active setting; function (button 4) selects fine adjustment. The
one-second repeat delay and 100 ms repeat cadence also apply to RPM adjustment.
Function + button 3 still toggles the LED and never switches control mode.
LED indication continues to use measured TACH RPM, independent of the setpoint.

## Configuration

All values are in `Config.h`; no wiring changes are needed.

| Config value | Default | Meaning |
| --- | --- | --- |
| `RpmControl::MIN_RPM` / `MAX_RPM` | 550 / 1700 | Inclusive allowed target range |
| `RpmControl::DEFAULT_RPM` | 850 | Initial target without a valid saved record |
| `Buttons::RPM_STEP` / `MODIFIED_RPM_STEP` | 100 / 10 | Normal / function-key RPM increment |
| `Fan::DEFAULT_DUTY_PERCENT` | 35 | Initial manual setting without saved memory |
| `Buttons::DUTY_STEP_PERCENT` / `MODIFIED_DUTY_STEP_PERCENT` | 5 / 1 | Manual duty increments |
| `OperatingMemory::ENABLED` | true | Enable NVS loading and saving of exited setpoints |
| `OperatingMemory::RETRY_INTERVAL_MS` | 1000 | Minimum interval between save attempts |
| `RpmControl::FILTER_ALPHA` | 0.35 | Measurement exponential filter coefficient |
| `RpmControl::DEADBAND_RPM` | 20 | Error tolerance before applying a correction |
| `RpmControl::KP` | 0.012 | Proportional gain in duty percentage points / RPM |
| `RpmControl::KI` | 0.004 | Integral gain in duty percentage points / RPM / second |
| `RpmControl::MAX_DUTY_CHANGE_PER_SECOND` | 2 | Automatic PWM slew limit in percentage points / second |
| `RpmControl::MAX_UPDATE_SECONDS` | 2 | Cap on elapsed time used for one feedback step |

The range is a setpoint constraint, not a measured guarantee that every attached
fan can attain every speed. Configure attainable limits for the actual fan and
supply voltage. The existing one-second, two-pulse-per-revolution counter has
approximately 30 RPM quantization. Fine target increments do not imply 10 RPM
measurement accuracy.

## Feedback and smoothing

Only a newly completed TACH sample advances the regulator. Network reads, LED
updates and OLED refreshes never run extra feedback steps. The raw cached RPM
remains available to telemetry and the LED; the regulator privately filters it:

```text
filtered += alpha * (measured - filtered)
error = sign(target - filtered) * max(abs(target - filtered) - deadband, 0)
candidate = integral + Ki * error * seconds + Kp * error
output = clamp(candidate, previous_output ± slew * seconds, and 0..100)
integral = output - Kp * error
```

Tracking the limited output in the integral prevents windup at both duty limits
and the slew limit. The controller keeps fractional duty internally and rounds
to integer percentages only when applying PWM. Mode entry seeds the filter and
integral from the first fresh measurement and the existing output, avoiding an
immediate proportional jump. Time capping prevents a delayed loop from causing
a large catch-up correction. Target changes also obey the automatic slew limit.
Manual commands, mode exit and the one-second startup kick apply directly.

There is no derivative term. The filter, continuous dead zone and output slew
limit reduce corrections driven by measurement noise. These conservative gains
are starting values; host simulation cannot prove stability on the real fan.
With absent TACH or an unreachable target, feedback gradually reaches a duty
limit. This revision does not distinguish sensor failure from a stopped fan or
add a separate stall-recovery routine.

## Operating memory

Manual duty and target RPM are independent settings. Leaving Duty queues its
manual setpoint for persistence; leaving RPM queues the target RPM. PI output
does not change the remembered manual setting. A restart always enters Duty,
after the existing full-duty startup kick. Switching to RPM uses the saved
target, initially 850 RPM. The last active mode is not persisted.

`OperatingMemory` uses a separate `fanmaster-op` NVS namespace with a versioned
CRC-protected record; Wi-Fi credentials remain untouched. Boot reads it before
starting the fan. Invalid/missing records or unavailable NVS fall back to Config
defaults. Valid saved RPM is clamped to the current range when Config changes.
With `ENABLED=false`, there are no operating-memory NVS reads or writes; each
boot uses defaults, and mode switching still retains settings in RAM.

The network task writes exited settings outside the fan mutex. Changes are
coalesced and retries are throttled; held buttons and periodic feedback do not
write NVS. A failed write remains pending for retry. If the network task cannot
start, local control works but queued changes cannot be saved. Power loss before
a mode exit or before its queued save completes can lose that latest setting.

## Interfaces and deployment

- `POST /api/fan/rpm` with `rpm=850`: validate target, then select RPM mode.
- `POST /api/fan/duty` with `duty=35`: validate duty, then select manual mode.
- `POST /api/fan/mode` with `mode=rpm` or `mode=duty`: switch without editing a setpoint.
- `GET /api/status`: mode, target, manual setting, actual duty, measured RPM and range.
- Serial: `rpm 850`, `mode rpm`, `mode duty`; a plain 0–100 number selects Duty.
- OLED: measured RPM on the large line; manual mode or RPM target on the duty line.

Rebuild the application and repack/re-upload the six-file external asset image
for the new Web Panel. See `BUILDING.md` and `FLASH_RESOURCES.md`. The uploader's
code/protocol is unchanged; an older resource image still offers duty control,
and submitting duty exits RPM mode.

## Validation boundary

Host checks cover button events, RPM/duty steps and clamps, mode transitions,
PWM output, sample timing/rollover, noisy first-order plant simulation, slew and
saturation recovery, serial/API validation, NVS load/save/failure and disabled
memory. The simulation exercises the algorithm on an illustrative plant; it is
not measured acceptance of this fan. The fixed-RPM hardware section in
`HARDWARE_TEST_REPORT.md` separately records the uploaded application/resources,
650/850/1050 RPM step tests, saved settings restored after hardware reset, and
local-loop behavior during concurrent HTTP requests.

Further acceptance requires a cold power-cycle check, external load-disturbance
recovery, independent speed calibration, and characterization of the configured
550/1700 RPM endpoints. The recorded tests used the default feedback gains.
