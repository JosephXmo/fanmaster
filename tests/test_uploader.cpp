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
    setup();
    assert(flashReady);
    auto transfer = [&](size_t bytes, uint32_t crc) {
        Serial = FakeSerial{};
        Serial.incoming.assign(image.begin(), image.begin() + bytes);
        return uploadImage(image.size(), crc);
    };
    const uint32_t checksum = AssetFormat::crcUpdate(0xffffffff, image.data(), image.size()) ^ 0xffffffff;
    assert(transfer(image.size(), checksum));
    assert(std::equal(image.begin(), image.end(), fakeFlash.begin()));
    assert(Serial.output.find("DONE verified") != std::string::npos);
    // Interrupted replacement invalidates the old header, including after payload writes.
    assert(!transfer(256, checksum));
    for (size_t i = 0; i < sizeof(AssetFormat::Header); ++i) assert(fakeFlash[i] == 0xff);
    assert(Serial.output.find("DONE") == std::string::npos);
    assert(!transfer(image.size(), checksum ^ 1));
    for (size_t i = 0; i < sizeof(AssetFormat::Header); ++i) assert(fakeFlash[i] == 0xff);
    assert(transfer(image.size(), checksum)); // A subsequent full retry recovers the image.
    std::cout << "Uploader readback, CRC rejection and interrupted commit tests passed\n";
}
