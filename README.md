# Fanmaster 1.5

ESP32-S3 firmware for a four-wire PWM fan, with local buttons, an OLED dashboard
and a Wi-Fi Web Panel. Supports manual duty, smoothed fixed-RPM feedback,
three-point temperature control and relay-based fan power. Wi-Fi and operating
settings stay in NVS; web assets live on W25Q64 SPI Flash.

The main firmware occupies app0. A resident serial resource uploader in app1
is selected for the next manual restart from the panel, serial console or
button 4. The panel can cancel selection; a verified upload returns to app0
automatically after ten seconds.

The complete manual is an independent static website:

```sh
python3 -m venv .venv-docs
./.venv-docs/bin/python -m pip install -r docs/requirements.txt
./.venv-docs/bin/python scripts/build_docs.py
./.venv-docs/bin/python -m http.server 8000 --bind 127.0.0.1 --directory build/docs
```

Open **http://127.0.0.1:8000**. Windows PowerShell commands and authoring details
are in [Documentation workflow](docs/content/documentation.md).

- [Hardware and pins](docs/content/hardware.md) · [Local controls](docs/content/controls.md)
- [Build environment](docs/content/building.md) · [Installation and web assets](docs/content/resources.md)
- [HTTP API](docs/content/api.md) · [Resident updater](docs/content/updater.md)
- [Flash layout](docs/content/storage.md) · [Hardware validation](docs/content/validation.md)

Edit hardware pins and timings in [src/fanmaster/Config.h](src/fanmaster/Config.h). Firmware staging/build and resource
tools are in `scripts/`; host checks use `sh scripts/check_host.sh` and the three
`tests/test_*.mjs` browser-logic suites. See the manual for prerequisites.

`docs/` and its generated site are separate from `web/`; they are not included
in the W25Q64 image. Building documentation does not access the device.
