# Three-point temperature control

The independent Temperature Control card on the Web Panel overrides the current
fixed Duty or RPM mode. Its main button enables/disables THERMAL. The OLED shows
`DUTY <applied>% THERMAL`, and the live telemetry card identifies the same mode.
Enabling it uses the saved curve, not unsaved editor values. Save the curve first
if you want to activate a newly edited curve.

## Curve and editing rules

| Point | Initial temperature | Initial duty |
| --- | --- | --- |
| Low | 25.0°C | 35% |
| Middle | 40.0°C | 65% |
| High | 80.0°C | 90% |

All three temperatures are within **15.0–95.0°C**, in **0.1°C** steps. They must
strictly increase: adjacent points must be at least 0.1°C apart. Duties are whole
percentages in **0–100%**, and cannot decrease from low to high. Equal duties
are allowed and create a flat segment.

The editor sets each input's bounds from its neighbors. On completing an input
change, its value is rounded to the permitted precision and clamped to those
bounds. Partial/invalid edits hide the curve preview and cannot be saved. The
firmware separately validates the complete submitted curve and rejects invalid
curves atomically; it does not silently reorder points. Polling preserves
unsaved edits. Saving applies all three points together and immediately updates
output when THERMAL is active.

For a valid temperature `T`, the applied duty is:

```text
T <= T1: D1
T1 < T <= T2: D1 + (D2 - D1) * (T - T1) / (T2 - T1)
T2 < T < T3: D2 + (D3 - D2) * (T - T2) / (T3 - T2)
T >= T3: D3
```

With the initial curve, 32.5°C produces 50%, and 60°C produces 77.5%, rounded to
78% for the existing integer-percent PWM interface. Output is recomputed from
the cached SHT30 sample (normally once per second); repeated local-loop visits
do not rewrite unchanged PWM. This is direct temperature-to-duty control: the
RPM PI regulator remains inactive during THERMAL. There is no extra temperature
filter, hysteresis or slew limit in this revision.

## Override and local controls

- The preceding fixed mode and its manual duty / target RPM remain available.
  Thermal duty never replaces the manual setpoint.
- Fixed Duty/RPM web inputs and submit buttons are disabled. The firmware also
  rejects fixed HTTP/serial changes while THERMAL is active.
- Buttons 1/2 are ignored, including fine adjustments and long-press repeats.
- A standalone button-3 click exits THERMAL and restores the preceding fixed
  mode and its settings. That click does not also toggle Duty/RPM. A later
  standalone click performs the normal Duty/RPM switch.
- Function (button 4) + button 3 still toggles the LED, without exiting THERMAL.
- Turning off the web toggle restores the same fixed state. Restored RPM mode
  resumes the smoothed regulator from the currently applied output.

Both widgets share one status poller and one command lock. Successful commands
return an authoritative status snapshot used immediately by both cards.
Responses from polls started before a command are discarded. A local exit is
reflected on the next successful status poll (normally one second; network
latency can extend this). OLED state comes from the same controller snapshot.

## Temperature failure

A fresh, valid, finite temperature is required to enable THERMAL. The default
stale threshold is **3000 ms**, configured by `Thermal::SENSOR_STALE_MS`; the
SHT30 normally samples every 1000 ms. The sample timestamp is recorded on a
successful read, and timeout calculations tolerate `millis()` rollover.

A failed sensor read, non-finite temperature or expired sample exits THERMAL in
the local control loop and restores the preceding fixed state. The Web Panel
reports the sensor-triggered exit. Sensor recovery does **not** automatically
re-enable THERMAL; use the toggle explicitly. This intentionally follows the
selected fallback policy and does not force full fan speed. A sensor that fails
initialization at boot still requires the existing sensor recovery/restart
workflow before temperature control can be used.

## Configuration and persistence

`Config::Thermal` contains the temperature limits, default temperatures (integer
tenths of a degree), default duties and sensor timeout. Default curves are
validated at compile time. Firmware and Web Panel implement the 15–95°C product
range; changing that range requires updating both the firmware and editor.

`OperatingMemory::ENABLED` controls both fixed-setpoint and curve persistence.
Saving a changed curve queues one checked NVS record for the network task;
sensor readings and automatically interpolated outputs do not queue writes.
Writes occur outside the fan mutex and retry with the existing throttle.
A web save confirms acceptance in RAM; power loss before the queued write
finishes can lose the latest edit. With memory disabled, edits remain available
only until restart.

The new 28-byte FMO2 record uses the existing `fanmaster-op/settings` key and
stores the fixed presets plus all three temperature/duty pairs, with CRC32.
A valid 16-byte FMO1 record from the fixed-RPM firmware is read without losing
its Duty/RPM presets; the default curve is supplied, then the next changed
setting is saved as FMO2. Invalid records fall back to Config defaults. Older
firmware cannot read FMO2. The Wi-Fi credential namespace is independent.

Boot selects fixed **Duty** with fan supply **off**. The startup kick runs
only after explicit key/web power-on; see [POWER_CONTROL.md](POWER_CONTROL.md). The
curve is restored if enabled, but THERMAL activation is never persisted.

## HTTP API

All POST bodies are `application/x-www-form-urlencoded`. Successful fan and
thermal commands return HTTP 200 with the same complete snapshot as
`GET /api/status`.

| Endpoint | Fields | Behavior |
| --- | --- | --- |
| `POST /api/thermal/mode` | `enabled=1` or `enabled=0` | Enable THERMAL or restore fixed mode |
| `POST /api/thermal/curve` | `t1=25.0&d1=35&t2=40.0&d2=65&t3=80.0&d3=90` | Validate and replace the entire curve |

Temperatures accept base-10 whole numbers or exactly one fractional digit;
scientific notation, signs, trailing decimal points, extra fractional digits,
overflow, missing fields and out-of-range values are rejected. Duties accept
whole unsigned decimal numbers only. Invalid input/curve returns **400** without
changing the curve. Enabling without fresh temperature or changing fixed
controls during THERMAL returns **409** without applying that command.

Status adds:

- `mode`: `duty`, `rpm`, or `thermal`.
- `fixed_mode`: preceding fixed mode during override, otherwise the current mode.
- `thermal_points`: three `{temperature_c, duty_percent}` objects.
- `thermal_sensor_ready`: whether the controller has a usable fresh temperature.
- `thermal_exit_reason`: `none`, `manual`, or `sensor`; last exit reason until
  the next successful thermal activation.
- `control_revision`: revision of user control changes, including thermal exit;
  automatic RPM/PWM corrections do not increment it.

`manual_duty_percent` and `target_rpm` remain the fixed settings;
`duty_percent` always reports the actual applied output.

## Deployment and validation

Rebuild the main application, repack and upload the **seven-resource** external
image (including `/assets/thermal.js`), then restore the main application using
[FLASH_RESOURCES.md](FLASH_RESOURCES.md). The uploader code and transfer protocol
are unchanged. Update firmware and resources together for the new controls.

Run:

```sh
sh scripts/check_host.sh
node tests/test_setup.mjs
node tests/test_panel.mjs
node tests/test_thermal.mjs
```

Host tests cover curve endpoints/interpolation/precision, invalid curves, fixed
mode restoration, blocked controls, sensor failure/expiry/rollover, no automatic
reentry, FMO1 migration, FMO2 persistence, disabled memory, HTTP validation,
widget bounds and preview, unsaved edits, shared command locks, local-state
polling and old-response rejection. Local browser verification uses a simulated
HTTP service and does not establish ESP32 behavior. Target compilation and asset
verification are recorded separately from physical-board acceptance.

### Hardware validation — 2026-10-15

The final main image, including the gross TACH outlier guard, compiled for
`esp32:esp32:esp32s3:CDCOnBoot=cdc` with ESP32 core 3.3.12:
**1,036,335 program bytes / 51,004 static RAM bytes**. It was flashed with
esptool hash verification. The seven-resource W25Q64 image is **34,340 bytes**,
whole-image CRC32 **ca141fbb**, and returned `DONE verified`; the user confirmed
the uploader OLED sequence. Actual device HTTP matched every resource to source.

Curve mapping, invalid submissions, blocked fixed APIs, mode restoration,
concurrent clients, local controls and real temperature variation were exercised.
The shared-peripheral power test verified sensor-failure fallback and recovery
without automatic thermal reentry. Gross TACH noise discovered during this test
led to the configurable completed-window guard documented in
[RPM_CONTROL.md](RPM_CONTROL.md). The guard does not eliminate all electrical
noise or distinguish an open TACH line from a stopped fan.

A controlled upgrade from legacy firmware restored distinct 43% / 930 RPM
presets and supplied the default curve. After saving non-default points
25.3°C/37%, 41.2°C/66%, 81.1°C/92% and entering THERMAL, the user fully removed
and restored board power. Actual HTTP confirmed the saved points/presets and
startup in Duty 43%, without automatic THERMAL activation. Wi-Fi reconnected
using its retained credentials.

See [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md) for the initial functional
run, final regression and evidence limits, and [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md)
for final binary sizes and storage layout. Host checks and local simulated-browser
checks complement, rather than replace, these target observations. Local
screenshots remain ignored artifacts under `build/thermal-preview/`.
