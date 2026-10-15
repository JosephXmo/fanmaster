# Build environment: macOS, Linux and Windows

This guide prepares a host for both the Fanmaster application and the separate
W25Q64 resource uploader. Continue with the complete upload sequence in
[FLASH_RESOURCES.md](FLASH_RESOURCES.md#build-and-upload) after setup.
Commands run from the repository root, where `Config.h` and `scripts/` live.
The firmware configuration and wiring are the same on every host OS.

## Tools

| Tool | Purpose | Version / requirement |
| --- | --- | --- |
| Arduino CLI | Install ESP32 support/libraries, compile and flash | 1.5.1 |
| ESP32 Arduino core | ESP32-S3 toolchain and built-in Wi-Fi/FreeRTOS | 3.3.12 |
| Python 3 | Stage sketches, pack/verify assets and transfer resources | Use Python 3.9 or newer |
| pyserial | Transfer the resource image and list serial ports | 3.5 |
| Git | Obtain and update the source | Any current release |
| Node.js and a C++17 compiler | Optional host tests only | Node.js 18+; Clang/GCC with thread support |

The ESP32 cross-compiler is installed with the Arduino core. A separate host
C++ compiler and Node.js are not required to build or flash either firmware.

### macOS

Install [Homebrew](https://brew.sh/) if using the following commands:

```sh
brew install arduino-cli python
arduino-cli version
python3 --version
```

Git and optional host C++ tests use Apple's Command Line Tools, installed with
`xcode-select --install` if needed. Node.js is optional (`brew install node`).
Arduino CLI can also be installed from its official downloads below.

### Linux

Install Python and Git through the distribution's package manager. For
Debian/Ubuntu:

```sh
sudo apt update
sudo apt install git python3 python3-venv python3-pip
```

Install Arduino CLI using its [official installation guide](https://arduino.github.io/arduino-cli/latest/installation/)
or [release archive](https://github.com/arduino/arduino-cli/releases) matching
the host architecture (for example Linux x86-64 or ARM64). Put the extracted
`arduino-cli` executable in a directory on `PATH`, such as `~/.local/bin`, and
open a new terminal if `PATH` changed. Check:

```sh
arduino-cli version
python3 --version
```

For optional host tests, install a C++17 compiler (`build-essential` on
Debian/Ubuntu) and Node.js 18 or newer. Other distributions use their own
package names.

### Windows

Use native **PowerShell** for the build/upload examples. Install:

- [Python 3](https://www.python.org/downloads/windows/), including the Python
  launcher (`py`), or an equivalent installation providing `python` on `PATH`.
- [Git for Windows](https://git-scm.com/downloads/win).
- [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/).
  Choose a Windows release matching the host architecture; put `arduino-cli.exe`
  in a directory on `PATH`.

Open a new PowerShell window after installation and check:

```powershell
arduino-cli version
py -3 --version
git --version
```

If `py` is unavailable but `python --version` reports Python 3.9+, replace
`py -3` with `python` in the environment-creation command below. The subsequent
commands call the virtual environment's Python directly. No activation script
or PowerShell execution-policy change is needed. An executable path containing
spaces must be quoted and invoked with `&`, for example
`& "C:/Tools/Arduino CLI/arduino-cli.exe" version`.

WSL is an option for host tests, but the native PowerShell upload commands use
Windows COM ports. WSL has its own Python/Arduino installations and USB access;
a Windows `COM5` port is not automatically a Linux device inside WSL.

## Source checkout

Clone into a directory named `fanmaster`, or open an existing checkout. These
commands work in both the POSIX shells and PowerShell:

```text
git clone https://github.com/JosephXmo/fanmaster.git fanmaster
cd fanmaster
```

Use the staging script even when building from Arduino IDE. It creates a
correctly named sketch directory and includes the shared files; the uploader
under `tools/` cannot be built by itself without those files. Edit originals in
this checkout and stage again after changes.

## ESP32 core and Arduino libraries

The commands below are single lines and work in all three OS environments.
They use the official Espressif board index without replacing an existing CLI
configuration. Downloads require Internet access.

```text
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib update-index
arduino-cli lib install "FastLED@3.10.7" "SPIMemory@3.4.0" "arduino-sht@2.0.0"
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library" "Adafruit BusIO"
arduino-cli core list
arduino-cli lib list
```

The CLI's Arduino directories may differ from Arduino IDE's if either uses a
custom configuration. Check `arduino-cli config dump` when the IDE finds a
library but the CLI does not. Use `--libraries "path/to/libraries"` on the
build script for an additional library directory; it does not install libraries.

The build script defaults to
`esp32:esp32:esp32s3:CDCOnBoot=cdc`. Use the same FQBN for compilation and
upload. The actual ESP32-S3-Zero in the hardware report has 4 MiB internal
flash and 2 MiB PSRAM; the external W25Q64 is a separate 8 MiB device.

## Python environment

Use one project-local virtual environment. It and all build outputs are ignored
by Git. Activation is optional; the examples invoke its interpreter directly.

macOS/Linux:

```sh
python3 -m venv .venv
./.venv/bin/python -m pip install pyserial==3.5
./.venv/bin/python -m serial.tools.list_ports -v
```

Windows PowerShell:

```powershell
py -3 -m venv .venv
& ./.venv/Scripts/python.exe -m pip install pyserial==3.5
& ./.venv/Scripts/python.exe -m serial.tools.list_ports -v
```

Packing and verifying assets needs only Python's standard library. pyserial is
needed for the resource transfer, not for compilation or esptool-based firmware
upload through Arduino CLI.

## Identify and access the serial port

Connect with a data-capable USB cable and run `arduino-cli board list`.
Compare the list before and after connecting if the board name is unknown.

| Host | Typical port | Other way to identify it |
| --- | --- | --- |
| macOS | `/dev/cu.usbmodem101` | Python port listing above; `ls /dev/cu.*` |
| Linux | `/dev/ttyACM0` (native USB), sometimes `/dev/ttyUSB0` (USB-UART bridge) | Python port listing; kernel USB logs |
| Windows | `COM5` | Device Manager → Ports (COM & LPT); Python port listing |

These are examples, not fixed identities. Resetting into the bootloader or
changing USB firmware can change the port, particularly the Windows COM
number. List ports again after flashing if a transfer can no longer open it.
Do not copy the macOS hardware report's port onto another host.

On Linux, check device ownership with `ls -l /dev/ttyACM0` (substitute the
actual port) and check your groups with `id -nG`. On Debian/Ubuntu, membership
in `dialout` normally grants serial access:

```sh
sudo usermod -aG dialout "$USER"
```

Log out and back in for the new group to apply. Other distributions may use
`uucp` or udev rules instead; follow the device's actual ownership and the
distribution's policy. Build and upload as your normal user. If Linux USB
logs show ModemManager probing/resetting the board, check its device exclusion
settings rather than assuming every serial failure is a firmware bug.

On Windows, confirm that the board enumerates as a usable COM device before
flashing. ESP32-S3 native USB normally uses the OS USB serial support; a carrier
with a separate USB-UART bridge may require that bridge vendor's driver.
Close Arduino Serial Monitor, CLI monitor and any terminal using the port
before upload on every OS.

## Optional host checks

On macOS/Linux, with Python 3, a C++17 compiler and Node.js 18+ on `PATH`:

```sh
sh scripts/check_host.sh
node tests/test_setup.mjs
```

The shell script invokes `python3` and `${CXX:-c++}`. Set `CXX=clang++` or
`CXX=g++` if the desired compiler has a different command name. On Windows,
the same checks can run in an Ubuntu WSL shell with `python3`, `build-essential`
and Node.js 18+ installed. Prefer a checkout cloned inside WSL so shell scripts
retain LF line endings. If reusing a Windows checkout (for example,
`C:/src/fanmaster` at `/mnt/c/src/fanmaster`), ensure `.sh` files have LF rather
than CRLF line endings. This host-test route does not require USB forwarding.
The `.sh` script is not a PowerShell
script; native Windows firmware builds use Python and Arduino CLI as above.

Host checks cover input state/timing, shared climate snapshots, Wi-Fi/NVS
failures, HTTP/resource streaming, uploader integrity/OLED status, and browser
setup logic with substitutes. They do not prove hardware or OS USB acceptance.
Linux and Windows commands are documented for use but have not been run on
those systems in this session. Recorded physical-board results are from macOS
and are in [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `arduino-cli` not found | Add the CLI executable's directory to `PATH`, open a new terminal, then run `arduino-cli version`. A custom path can be passed as `--arduino-cli "path/to/arduino-cli.exe"` to the build script. |
| `venv` unavailable on Linux | Install the distribution's Python venv package, then recreate `.venv`. |
| Missing Python `serial` module | Install pyserial using the same `.venv` interpreter used to run `assets.py`. |
| Missing Arduino header/library | Check `arduino-cli core list`, `lib list` and CLI library locations. |
| Port missing or access denied | Check cable, USB enumeration, changed port, Linux groups and competing serial monitors. |
| Board stays in download mode / no uploader PONG | Release BOOT and press RESET, list ports again, then retry. The asset transfer script deasserts DTR/RTS before opening the port. |
| Cannot enter bootloader for firmware upload | Hold BOOT, press/release RESET, release BOOT, identify the bootloader port and retry Arduino CLI upload. Reset after upload if necessary. |
| `ERROR flash unavailable` | Check external flash power, common ground, configured SPI pins and GPIO-controlled CS. CS cannot be tied permanently high. |
| HTTP 502 or page fails through a proxy | Check the current OLED/serial station IP and local network access; check whether the browser proxy handles private addresses correctly. A proxy error does not establish that ESP32 returned the same status. |

### Apple Silicon ctags issue

The recorded macOS host had an Intel-only Arduino `ctags` executable that failed
with `bad CPU type in executable`. A temporary native build was used there,
without changing the global Arduino installation. If you reproduce that exact
failure, use a compatible ctags installation or append an override to the
Python build command:

```sh
--build-property "runtime.tools.ctags.path=/path/to/native-ctags/bin"
```

The earlier local path `/private/tmp/fanmaster-ctags-native/bin` is specific to
that machine. It is not a repository dependency and should not be copied into
Linux or Windows commands. A normal compatible toolchain needs no override.
