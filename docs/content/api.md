# HTTP API

All endpoints are served by app0 on port 80. POST bodies use
`application/x-www-form-urlencoded`. Successful fan, thermal and boot-selection
commands return the complete status snapshot. Invalid input is rejected before
changing settings. There is no application OTA-image upload endpoint.

## Status

`GET /api/status` returns the current controller snapshot and cached sensor data.
The following is an illustrative boot-off state using Config defaults; saved
presets can differ.

```json
{
  "firmware_version": "1.5",
  "updater_available": true,
  "updater_next_boot": false,
  "rpm": 0,
  "duty_percent": 0,
  "wifi_connected": true,
  "power_enabled": false,
  "power_starting": false,
  "power_waiting_for_key": true,
  "temperature_c": 24.6,
  "humidity_percent": 52.1,
  "poll_interval_ms": 1000,
  "mode": "duty",
  "target_rpm": 850,
  "manual_duty_percent": 35,
  "min_rpm": 550,
  "max_rpm": 1700,
  "control_revision": 0,
  "fixed_mode": "duty",
  "thermal_sensor_ready": true,
  "thermal_exit_reason": "none",
  "thermal_points": [
    { "temperature_c": 25.0, "duty_percent": 35 },
    { "temperature_c": 40.0, "duty_percent": 65 },
    { "temperature_c": 80.0, "duty_percent": 90 }
  ]
}
```

| Fields | Meaning |
| --- | --- |
| `rpm`, `duty_percent` | Most recent plausible TACH sample and actual applied PWM, not the manual preset |
| `mode`, `fixed_mode` | Active/retained mode; preceding fixed mode during THERMAL |
| `target_rpm`, `manual_duty_percent` | Independent saved-in-RAM fixed setpoints |
| `min_rpm`, `max_rpm` | Inclusive Config target bounds |
| `power_enabled` | Commanded relay state, without electrical feedback |
| `power_starting` | One-second full-duty startup boost is active |
| `power_waiting_for_key` | Initial boot state accepts any fresh button press |
| `temperature_c`, `humidity_percent` | Cached SHT30 measurements; `null` on failed/non-finite sample |
| `thermal_sensor_ready` | Fresh temperature available for enabling THERMAL |
| `thermal_exit_reason` | `none`, `manual` or `sensor` |
| `thermal_points` | Three ordered points, temperatures in °C and duty in integer percent |
| `control_revision` | Controller change revision used for synchronization |
| `updater_available` | Readable app1 role descriptor; not a full-image checksum result |
| `updater_next_boot` | Current OTA metadata selects app1 |
| `poll_interval_ms` | Suggested sequential polling interval |

TACH and SHT30 normally update once per second. Reading status does not trigger
another TACH count or sensor conversion. Telemetry and commanded power are
separate: a fan can coast after its power command is disabled.

## Fixed control and power

| POST path | Body example | Effect |
| --- | --- | --- |
| `/api/fan/duty` | `duty=35` | Select Duty with a whole 0–100% setting |
| `/api/fan/rpm` | `rpm=850` | Select RPM within `min_rpm`–`max_rpm` |
| `/api/fan/mode` | `mode=rpm` | Select `duty` or `rpm` using its existing setpoint |
| `/api/fan/power` | `enabled=1` | Explicitly set the fan power command to 0 or 1 |

Malformed, missing, negative, fractional, overflowing or out-of-range arguments
return **400**. Valid fixed changes return **409** while fan power is off or
THERMAL is active. Power commands are idempotent, return immediately, and do not
wait for the startup boost or persist power in NVS.

## Temperature control

`POST /api/thermal/mode` accepts `enabled=0` or `enabled=1`. Enabling requires
fan power and a fresh temperature; otherwise **409**. Disabling restores the
preceding fixed mode and settings. Invalid booleans return **400**.

`POST /api/thermal/curve` accepts all three points atomically:

```text
t1=25.0&d1=35&t2=40.0&d2=65&t3=80.0&d3=90
```

Temperatures must be 15.0–95.0°C with at most one decimal and strictly increase;
duties must be integer 0–100% and cannot decrease. Any invalid point returns
**400** without partially updating the curve. Curve editing works with power
off or in THERMAL; while active, saving immediately recomputes output.
See [temperature control](thermal.md) for interpolation, persistence and fallback.

## Next-boot updater selection

| POST path | Body | Effect |
| --- | --- | --- |
| `/api/updater/select` | Empty | Validate/select app1 for the next boot |
| `/api/updater/cancel` | Empty | Validate/select app0 for the next boot |

Neither command restarts, enables fan power or changes its settings. Selection
works with power off and during THERMAL. Cancellation is idempotent and does
not need a readable app1. A missing/wrong-role destination returns **503**;
SDK image-validation/boot-selection failure returns **500**. Failure preserves
the previous selection. See [resident updater](updater.md).

## Provisioning

These endpoints require setup to be active; otherwise **403**.

- `GET /api/wifi/status`: returns `state`, `connected`, and station `ip`.
  States are `idle`, `connecting`, `saved`, `connection_failed`, `storage_failed`.
- `POST /api/wifi/configure`: accepts `ssid` and `password` only through the
  setup AP interface. Returns **202** while connecting, **400** for invalid
  input, **409** while another candidate is in progress, or **403** outside
  setup/AP access. SSID is 1–32 UTF-8 bytes; password is empty for an open network,
  8–63 bytes for a passphrase, or 64 hexadecimal characters for a raw key.

Credentials are committed only after successful connection. A failed candidate
preserves the previous record. Passwords are never returned.

## Routes and clients

`/setup` is the explicit setup page; `/` switches between setup and dashboard.
Unknown `/api/` paths return a JSON **404** error. Static resources and captive
probe redirects are handled separately from API routes.

Use explicit power/mode setters rather than client-side relative toggles.
Serialize commands and refresh from their returned status; use a sequential
poller so slow requests do not accumulate. The interface has no authentication,
TLS or multi-device discovery. A future LAN Hub must supply those coordination
and access decisions itself.
