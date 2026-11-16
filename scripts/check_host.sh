#!/bin/sh
# Run platform-independent resource integrity and Wi-Fi state tests.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
check_dir=$(mktemp -d "${TMPDIR:-/tmp}/fanmaster-check.XXXXXX")
trap 'rm -rf "$check_dir"' EXIT HUP INT TERM
cd "$repo_dir"
python3 -m unittest discover -s tests -p 'test_*.py'
python3 scripts/assets.py pack --output "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_buttons.cpp src/fanmaster/ButtonInputs.cpp -o "$check_dir/button-tests"
"$check_dir/button-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_boot.cpp src/fanmaster/FanController.cpp src/fanmaster/SerialConsole.cpp -o "$check_dir/boot-tests"
"$check_dir/boot-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_power.cpp src/fanmaster/FanController.cpp src/fanmaster/OledDisplay.cpp -o "$check_dir/power-tests"
"$check_dir/power-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_rpm.cpp src/fanmaster/FanController.cpp src/fanmaster/OperatingMemory.cpp src/fanmaster/SerialConsole.cpp -o "$check_dir/rpm-tests"
"$check_dir/rpm-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_thermal.cpp src/fanmaster/FanController.cpp src/fanmaster/OperatingMemory.cpp -o "$check_dir/thermal-tests"
"$check_dir/thermal-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I src/fanmaster \
  tests/test_shared_state.cpp -o "$check_dir/shared-state-tests"
"$check_dir/shared-state-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_firmware.cpp src/fanmaster/WifiStation.cpp src/fanmaster/W25q64.cpp src/fanmaster/AssetStore.cpp \
  -o "$check_dir/firmware-tests"
"$check_dir/firmware-tests" "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_web.cpp src/fanmaster/WebPanel.cpp src/fanmaster/WifiStation.cpp src/fanmaster/FanController.cpp src/fanmaster/W25q64.cpp src/fanmaster/AssetStore.cpp \
  -o "$check_dir/web-tests"
"$check_dir/web-tests" "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I src/fanmaster \
  tests/test_uploader.cpp src/fanmaster/W25q64.cpp -o "$check_dir/uploader-tests"
"$check_dir/uploader-tests" "$check_dir/assets.bin"
