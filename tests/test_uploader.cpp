#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <SPIMemory.h>
#include "../src/ResourceUploader/ResourceUploader.ino"

int main(int argc, char **argv)
{
    // Offline escape: debounce, exact hold threshold, raw release and rollover.
    for (uint32_t start : {0U, UINT32_MAX - 4000}) {
        fakeMillis = start;
        fakePinLevels[Config::Pins::BUTTON_4] = HIGH;
        ExitButton key; key.begin();
        fakePinLevels[Config::Pins::BUTTON_4] = LOW; assert(!key.update());
        fakeMillis += Config::Buttons::DEBOUNCE_MS; assert(!key.update());
        fakeMillis += Config::Updater::EXIT_HOLD_MS - 1; assert(!key.update());
        ++fakeMillis; assert(key.update());
        assert(!key.update());
        fakePinLevels[Config::Pins::BUTTON_4] = HIGH; assert(!key.update());
        fakePinLevels[Config::Pins::BUTTON_4] = LOW; key.update();
        fakeMillis += Config::Buttons::DEBOUNCE_MS; key.update();
        fakeMillis += Config::Updater::EXIT_HOLD_MS;
        fakePinLevels[Config::Pins::BUTTON_4] = HIGH; assert(!key.update());
    }
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    assert(!image.empty());
    // Progress is live but bounded; phase changes and completion bypass pacing.
    UploadDisplay progress;
    assert(progress.begin());
    progress.show("Write + verify", 0, 1024);
    const size_t firstFrame = fakeOledFrames.size();
    fakeMillis += Config::Oled::REFRESH_INTERVAL_MS - 1;
    progress.show("Write + verify", 256, 1024);
    assert(fakeOledFrames.size() == firstFrame);
    fakeMillis++;
    progress.show("Write + verify", 512, 1024);
    assert(fakeOledFrames.size() == firstFrame + 1);
    assert(fakeOledFrames.back().find("50%") != std::string::npos);
    progress.show("Write + verify", 1024, 1024);
    assert(fakeOledFrames.size() == firstFrame + 2);
    progress.show("Commit failed");
    assert(fakeOledFrames.back().find("Commit failed") != std::string::npos);
    assert(Serial.output.empty());
    fakeOledFrames.clear();
    fakeBootPartition = &fakeSlots[1];
    const int initialWrites = fakeBootWrites;
    setup();
    assert(fakeBootPartition == &fakeSlots[1] && fakeRestartCount == 0 && fakeBootWrites == initialWrites);
    assert(fakePinModes[Config::Pins::FAN_POWER] == OUTPUT);
    assert(fakePinLevels[Config::Pins::FAN_POWER] == (Config::Power::RELAY_ACTIVE_HIGH ? LOW : HIGH));
    assert(flashReady);
    assert(fakeOledFrames.back().find("Waiting for host") != std::string::npos);
    auto transfer = [&](size_t bytes, uint32_t crc) {
        Serial = FakeSerial{};
        Serial.incoming.assign(image.begin(), image.begin() + bytes);
        return uploadImage(image.size(), crc);
    };
    const uint32_t checksum = AssetFormat::crcUpdate(0xffffffff, image.data(), image.size()) ^ 0xffffffff;
    assert(transfer(image.size(), checksum));
    assert(std::equal(image.begin(), image.end(), fakeFlash.begin()));
    assert(Serial.output.find("DONE verified") != std::string::npos);
    assert(fakeOledFrames.back().find("Done verified") != std::string::npos);
    assert(fakeOledFrames.back().find("100%") != std::string::npos);
    // OLED traffic is bounded, not one full I2C frame per 256-byte page.
    assert(fakeOledFrames.size() < 20);
    // Interrupted replacement invalidates the old header, including after payload writes.
    assert(!transfer(256, checksum));
    for (size_t i = 0; i < sizeof(AssetFormat::Header); ++i) assert(fakeFlash[i] == 0xff);
    assert(Serial.output.find("DONE") == std::string::npos);
    assert(fakeOledFrames.back().find("Serial timeout") != std::string::npos);
    assert(!transfer(image.size(), checksum ^ 1));
    assert(fakeOledFrames.back().find("CRC/header invalid") != std::string::npos);
    for (size_t i = 0; i < sizeof(AssetFormat::Header); ++i) assert(fakeFlash[i] == 0xff);
    assert(transfer(image.size(), checksum)); // A subsequent full retry recovers the image.
    // Missing OLED/I2C never prevents serial recovery or verified writes.
    for (bool missingBus : {false, true}) {
        fakeOledFrames.clear();
        fakeWireAvailable = !missingBus;
        fakeOledAvailable = missingBus;
        setup();
        assert(transfer(image.size(), checksum));
        assert(fakeOledFrames.empty());
    }
    fakeWireAvailable = fakeOledAvailable = true;
    setup();
    Serial = FakeSerial{};
    const std::string failedCommand = "UPLOAD " + std::to_string(image.size()) + " 00000000\n";
    Serial.incoming.assign(failedCommand.begin(), failedCommand.end());
    loop();
    assert(!returnPending && fakeRestartCount == 0);
    assert(Serial.output.find("ERROR upload failed") != std::string::npos);
    fakeMillis = UINT32_MAX - 1000;
    Serial = FakeSerial{};
    const std::string command = "UPLOAD " + std::to_string(image.size()) + " " + [] (uint32_t c) {
        char buffer[9]; snprintf(buffer, sizeof(buffer), "%08x", c); return std::string(buffer);
    }(checksum) + "\n";
    Serial.incoming.assign(command.begin(), command.end());
    Serial.incoming.insert(Serial.incoming.end(), image.begin(), image.end());
    loop();
    assert(returnPending && fakeRestartCount == 0);
    assert(fakeBootPartition == &fakeSlots[0]); // Chosen only after verified success.
    loop();
    assert(fakeOledFrames.back().find("app0 in 10 seconds") != std::string::npos);
    // Pending success ignores another upload and preserves the committed image.
    Serial.incoming.insert(Serial.incoming.end(), command.begin(), command.end());
    fakeMillis = verifiedAtMs + Config::Updater::RETURN_DELAY_MS - 1;
    loop();
    assert(fakeRestartCount == 0 && std::equal(image.begin(), image.end(), fakeFlash.begin()));
    loop();
    assert(fakeRestartCount == 1 && fakeBootPartition == &fakeSlots[0]);
    Serial = FakeSerial{};
    const std::string leave = "RETURN\n";
    Serial.incoming.assign(leave.begin(), leave.end());
    loop();
    assert(fakeRestartCount == 2 && fakeBootPartition == &fakeSlots[0]);
    // A selection failure must not reboot into an unknown application.
    scheduleReturn(image.size());
    fakeSlotValid[0] = false;
    fakeMillis += Config::Updater::RETURN_DELAY_MS;
    serviceReturn();
    assert(fakeRestartCount == 2 && !returnPending);
    assert(fakeOledFrames.back().find("Cannot boot app0") != std::string::npos);
    scheduleReturn(image.size());
    assert(!returnPending && fakeRestartCount == 2);
    fakeSlotValid[0] = true;
    returnPending = false;
    fakePinLevels[Config::Pins::BUTTON_4] = HIGH; exitButton.begin();
    fakePinLevels[Config::Pins::BUTTON_4] = LOW; serviceEscape();
    fakeMillis += Config::Buttons::DEBOUNCE_MS; serviceEscape();
    fakeMillis += Config::Updater::EXIT_HOLD_MS;
    assert(serviceEscape() && fakeRestartCount == 3 && fakeBootPartition == &fakeSlots[0]);
    assert(!serviceEscape() && fakeRestartCount == 3);
    std::cout << "Uploader readback, CRC rejection and interrupted commit tests passed\n";
}
