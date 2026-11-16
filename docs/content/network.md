# Wi-Fi and Web Panel

Wi-Fi credentials are provisioned through a temporary setup access point and
stored in ESP32 NVS. They are not compiled into `src/fanmaster/Config.h` or the external
resource image. The controller does not need an application MAC-address lookup
to name its setup network.

## Provisioning flow

1. With no valid saved credentials, connect to `Fanmaster-Setup-<random suffix>`.
   The random suffix is created once per boot.
2. Open `http://192.168.4.1/setup` if the operating system does not open a captive
   page automatically. Catch-all DNS and HTTP probe redirects assist discovery.
3. Enter the real SSID and password in the setup form. The candidate remains in
   RAM while a station connection is attempted.
4. Only after connection succeeds is the pair saved as one checked NVS record.
   A failed candidate preserves the previous saved record.
5. Read the station IP from the setup result, OLED or serial output. The setup
   AP/DNS close after a five-second grace period following successful storage.

The setup AP remains open if a connection or NVS write fails. A stored network
is retried in the background; setup returns after 45 seconds of continuous
failure. The defaults are configurable in `Config::Wifi`.

The open setup link and application-managed NVS are not encrypted. Passwords
are not echoed or logged. Configuration POSTs are accepted only while setup is
active and through the AP interface. Details and input limits are in [HTTP API](api.md).

## Dashboard

At the station address, `/` serves the Web Panel while setup is inactive.
During setup, `/` serves provisioning instead. The full dashboard reads its
HTML/CSS/JS from W25Q64; a tiny embedded setup form remains available when
external resources or dependencies are missing.

The panel contains live RPM, applied duty, temperature and humidity; independent
fixed and temperature-control cards; a power control; and a maintenance card
for selecting or cancelling the next updater boot.

One shared poller and command lock coordinate all cards. Commands apply the
returned state immediately; stale polls cannot overwrite a newer command, and
unsaved input edits are preserved. Failed samples display `--` and JSON `null`.
The nominal status cadence is one second, extended by request latency.

## Task isolation

Wi-Fi, DNS, HTTP and external asset reads run in a dedicated network task on
core 0. Local buttons, feedback, sensor sampling, OLED and LED remain in the
Arduino loop on core 1. HTTP uses the stock synchronous Arduino WebServer;
separation keeps its waits off the local-control task. See [architecture](architecture.md).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Captive popup does not open | Connect to the setup AP and use the explicit setup URL; OS auto-opening is not guaranteed. |
| Dashboard cannot load | Confirm the latest station IP and that the host is on a reachable LAN. |
| Browser returns 502 | Check local-address proxy bypass. The earlier 502 report came from a host proxy, not proof of that response from ESP32. |
| Setup works but dashboard assets fail | Verify W25Q64 wiring and transfer the complete resource image again. |
| Local controls stall | Check the correct firmware, serial output backpressure and network-task logs; recorded fixes and loop gaps are in validation. |

The HTTP API is intended for a trusted local network; it does not include
authentication, TLS, device discovery or Hub coordination. See [API](api.md)
for the implemented operations rather than inferring features from the UI.
