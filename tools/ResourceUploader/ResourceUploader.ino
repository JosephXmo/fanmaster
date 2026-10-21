#include <Arduino.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "AssetFormat.h"
#include "W25q64.h"
#include "UploadDisplay.h"
#include "FanPowerOutput.h"

// Build with scripts/build_firmware.py: shared files and Config.h are staged
// alongside this sketch. This program keeps the relay off and never accesses NVS.
W25q64 flash;
UploadDisplay uploadDisplay;
bool flashReady = false;

bool failUpload(const char *reason)
{
    uploadDisplay.show(reason);
    return false;
}

bool uploadImage(uint32_t size, uint32_t expectedCrc)
{
    if (!flashReady) return failUpload("Flash unavailable");
    if (size < sizeof(AssetFormat::Header) || size > AssetFormat::CAPACITY)
        return failUpload("Invalid image size");
    uploadDisplay.show("Erasing", 0, size);
    // Erase the header first: a partial upload must not look like a valid image.
    for (uint32_t offset = 0; offset < size; offset += 4096) {
        if (!flash.eraseSector(offset)) return failUpload("Erase failed");
        uploadDisplay.show("Erasing", min(offset + 4096, size), size);
        Serial.print("ERASE ");
        Serial.println(offset);
        yield();
    }
    uploadDisplay.show("Write + verify", 0, size);
    Serial.println("READY");
    AssetFormat::Header header{};
    uint8_t incoming[256], readback[256];
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t offset = 0; offset < size;) {
        const size_t length = min(sizeof(incoming), static_cast<size_t>(size - offset));
        if (Serial.readBytes(incoming, length) != length) return failUpload("Serial timeout");
        const size_t skip = offset == 0 ? sizeof(header) : 0;
        if (skip) memcpy(&header, incoming, sizeof(header));
        if (!flash.programPage(offset + skip, incoming + skip, length - skip) ||
            !flash.read(offset + skip, readback + skip, length - skip)) return failUpload("Flash I/O failed");
        if (skip) memcpy(readback, &header, sizeof(header));
        if (memcmp(incoming, readback, length)) return failUpload("Readback mismatch");
        crc = AssetFormat::crcUpdate(crc, readback, length);
        offset += length;
        uploadDisplay.show("Write + verify", offset, size);
        Serial.print("ACK ");
        Serial.println(offset);
        yield();
    }
    uploadDisplay.show("CRC + header check", size, size);
    if ((crc ^ 0xFFFFFFFF) != expectedCrc ||
        memcmp(header.magic, AssetFormat::MAGIC, 8) || header.version != AssetFormat::VERSION ||
        !header.entryCount || header.entryCount > AssetFormat::MAX_ENTRIES ||
        header.imageBytes != size ||
        sizeof(header) + header.entryCount * sizeof(AssetFormat::Entry) > size ||
        (AssetFormat::crcUpdate(0xFFFFFFFF, &header, 28) ^ 0xFFFFFFFF) != header.headerCrc)
        return failUpload("CRC/header invalid");
    // Commit marker last, only after every byte was read back and CRC checked.
    uploadDisplay.show("Commit header", size, size);
    if (!flash.programPage(0, &header, sizeof(header)) ||
        !flash.read(0, readback, sizeof(header)) || memcmp(&header, readback, sizeof(header)))
        return failUpload("Commit failed");
    uploadDisplay.show("Done verified", size, size);
    Serial.println("DONE verified");
    return true;
}

void setup()
{
    FanPowerOutput::begin();
    Serial.begin(115200);
    Serial.setTimeout(5000);
    uploadDisplay.begin();
    uploadDisplay.show("Initializing flash");
    flashReady = flash.begin();
    uploadDisplay.show(flashReady ? "Waiting for host" : "Flash unavailable");
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
            uploadDisplay.show("Command too long");
            Serial.println("ERROR command too long");
        } else if (!strcmp(command, "PING")) {
            Serial.println(flashReady ? "PONG" : "ERROR flash unavailable; check CS and wiring");
        } else if (sscanf(command, "UPLOAD %" SCNu32 " %" SCNx32 " %c", &size, &checksum, &extra) == 2) {
            if (!uploadImage(size, checksum)) Serial.println("ERROR upload failed; image not committed");
        } else {
            uploadDisplay.show("Unknown command");
            Serial.println("ERROR unknown command");
        }
        used = 0;
        overflow = false;
    }
    delay(1);
}
