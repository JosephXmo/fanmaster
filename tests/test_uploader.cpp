#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <SPIMemory.h>
#include "../tools/ResourceUploader/ResourceUploader.ino"

int main(int argc, char **argv)
{
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
    setup();
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
    std::cout << "Uploader readback, CRC rejection and interrupted commit tests passed\n";
}
