#!/bin/sh
# Run platform-independent resource integrity and Wi-Fi state tests.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
check_dir=$(mktemp -d "${TMPDIR:-/tmp}/fanmaster-check.XXXXXX")
trap 'rm -rf "$check_dir"' EXIT HUP INT TERM
cd "$repo_dir"
python3 -m unittest discover -s tests -p 'test_*.py'
python3 scripts/assets.py pack --output "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_buttons.cpp ButtonInputs.cpp -o "$check_dir/button-tests"
"$check_dir/button-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_power.cpp FanController.cpp OledDisplay.cpp -o "$check_dir/power-tests"
"$check_dir/power-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_rpm.cpp FanController.cpp OperatingMemory.cpp SerialConsole.cpp -o "$check_dir/rpm-tests"
"$check_dir/rpm-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_thermal.cpp FanController.cpp OperatingMemory.cpp -o "$check_dir/thermal-tests"
"$check_dir/thermal-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pthread -I . \
  tests/test_shared_state.cpp -o "$check_dir/shared-state-tests"
"$check_dir/shared-state-tests"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_firmware.cpp WifiStation.cpp W25q64.cpp AssetStore.cpp \
  -o "$check_dir/firmware-tests"
"$check_dir/firmware-tests" "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_web.cpp WebPanel.cpp WifiStation.cpp FanController.cpp W25q64.cpp AssetStore.cpp \
  -o "$check_dir/web-tests"
"$check_dir/web-tests" "$check_dir/assets.bin"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I tests/fakes -I . \
  tests/test_uploader.cpp W25q64.cpp -o "$check_dir/uploader-tests"
"$check_dir/uploader-tests" "$check_dir/assets.bin"
