#include <Arduino.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "AssetFormat.h"
#include "W25q64.h"

// Build with scripts/build_firmware.py: shared files and Config.h are staged
// alongside this sketch. This program never accesses ESP32 NVS or fan outputs.
W25q64 flash;
bool flashReady = false;

bool uploadImage(uint32_t size, uint32_t expectedCrc)
{
    if (!flashReady || size < sizeof(AssetFormat::Header) || size > AssetFormat::CAPACITY) return false;
    // Erase the header first: a partial upload must not look like a valid image.
    for (uint32_t offset = 0; offset < size; offset += 4096) {
        if (!flash.eraseSector(offset)) return false;
        Serial.print("ERASE ");
        Serial.println(offset);
        yield();
    }
    Serial.println("READY");
    AssetFormat::Header header{};
    uint8_t incoming[256], readback[256];
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t offset = 0; offset < size;) {
        const size_t length = min(sizeof(incoming), static_cast<size_t>(size - offset));
        if (Serial.readBytes(incoming, length) != length) return false;
        const size_t skip = offset == 0 ? sizeof(header) : 0;
        if (skip) memcpy(&header, incoming, sizeof(header));
        if (!flash.programPage(offset + skip, incoming + skip, length - skip) ||
            !flash.read(offset + skip, readback + skip, length - skip)) return false;
        if (skip) memcpy(readback, &header, sizeof(header));
        if (memcmp(incoming, readback, length)) return false;
        crc = AssetFormat::crcUpdate(crc, readback, length);
        offset += length;
        Serial.print("ACK ");
        Serial.println(offset);
        yield();
    }
    if ((crc ^ 0xFFFFFFFF) != expectedCrc ||
        memcmp(header.magic, AssetFormat::MAGIC, 8) || header.version != AssetFormat::VERSION ||
        !header.entryCount || header.entryCount > AssetFormat::MAX_ENTRIES ||
        header.imageBytes != size ||
        sizeof(header) + header.entryCount * sizeof(AssetFormat::Entry) > size ||
        (AssetFormat::crcUpdate(0xFFFFFFFF, &header, 28) ^ 0xFFFFFFFF) != header.headerCrc) return false;
    // Commit marker last, only after every byte was read back and CRC checked.
    if (!flash.programPage(0, &header, sizeof(header)) ||
        !flash.read(0, readback, sizeof(header)) || memcmp(&header, readback, sizeof(header))) return false;
    Serial.println("DONE verified");
    return true;
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(5000);
    flashReady = flash.begin();
    // Do not wait for a serial monitor: the host uses PING to synchronize.
}

void loop()
{
    static char command[80];
    static size_t used = 0;
    static bool overflow = false;
    while (Serial.available()) {
        const char c = Serial.read();
        if (c == '\r') continue;
        if (c != '\n') {
            if (used + 1 < sizeof(command)) command[used++] = c;
            else overflow = true;
            continue;
        }
        command[used] = 0;
        uint32_t size, checksum;
        char extra;
        if (overflow) {
            Serial.println("ERROR command too long");
        } else if (!strcmp(command, "PING")) {
            Serial.println(flashReady ? "PONG" : "ERROR flash unavailable; check CS and wiring");
        } else if (sscanf(command, "UPLOAD %" SCNu32 " %" SCNx32 " %c", &size, &checksum, &extra) == 2) {
            if (!uploadImage(size, checksum)) Serial.println("ERROR upload failed; image not committed");
        } else {
            Serial.println("ERROR unknown command");
        }
        used = 0;
        overflow = false;
    }
    delay(1);
}
