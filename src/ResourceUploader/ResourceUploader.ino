#include <Arduino.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "AssetFormat.h"
#include "W25q64.h"
#include "UploadDisplay.h"
#include "FanPowerOutput.h"
#include "BootSlots.h"
#include "ExitButton.h"

FANMASTER_APP_IDENTITY
const FirmwareIdentity::Descriptor uploaderIdentity = FirmwareIdentity::make("ResourceUploader");

// Build with scripts/build_firmware.py: shared files and Config.h are staged
// alongside this sketch. This program keeps the relay off and never accesses NVS.
static_assert(Config::Updater::RETURN_DELAY_MS > 0 && Config::Updater::RETURN_DELAY_MS < 0x80000000UL,
              "Updater return timer must be positive and rollover-safe");
W25q64 flash;
UploadDisplay uploadDisplay;
ExitButton exitButton;
bool flashReady = false;
bool returnPending = false;
uint32_t verifiedAtMs = 0, verifiedBytes = 0, displayedSeconds = 0;

bool serviceEscape()
{
    if (!exitButton.update()) return false;
    if (BootSlots::selectMain() == ESP_OK) {
        uploadDisplay.show("Returning to app0");
        ESP.restart();
        return true;
    }
    uploadDisplay.show("Cannot boot app0");
    Serial.println("ERROR cannot select app0; release and hold * to retry");
    return false;
}

bool readUploadBytes(uint8_t *buffer, size_t length)
{
    size_t received = 0;
    uint32_t lastByteMs = millis();
    while (received < length) {
        if (serviceEscape()) return false;
        if (Serial.available()) {
            buffer[received++] = static_cast<uint8_t>(Serial.read());
            lastByteMs = millis();
        } else {
            if (millis() - lastByteMs >= 5000) return false;
            delay(1);
        }
    }
    return true;
}

void scheduleReturn(uint32_t size)
{
    // USB/JTAG serial opens can reset the chip. Stay in app1 until an upload
    // succeeds; only then make subsequent resets boot the main application.
    if (BootSlots::selectMain() != ESP_OK) {
        returnPending = false;
        uploadDisplay.show("Cannot boot app0");
        Serial.println("ERROR assets verified but cannot select app0; use RETURN to retry");
        return;
    }
    returnPending = true;
    verifiedAtMs = millis();
    verifiedBytes = size;
    displayedSeconds = 0;
}

void serviceReturn()
{
    const uint32_t elapsed = millis() - verifiedAtMs;
    if (elapsed >= Config::Updater::RETURN_DELAY_MS) {
        if (BootSlots::selectMain() == ESP_OK) {
            ESP.restart();
        } else {
            uploadDisplay.show("Cannot boot app0");
            Serial.println("ERROR cannot select app0; verified assets retained; use RETURN to retry");
        }
        returnPending = false;
        return;
    }
    const uint32_t seconds = (Config::Updater::RETURN_DELAY_MS - elapsed + 999) / 1000;
    if (seconds != displayedSeconds) {
        displayedSeconds = seconds;
        uploadDisplay.show("Done verified", verifiedBytes, verifiedBytes, seconds);
    }
}

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
        if (serviceEscape()) return false;
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
        if (!readUploadBytes(incoming, length)) return failUpload("Serial timeout");
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
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(0);
#endif
    Serial.setTimeout(5000);
    Serial.print("ResourceUploader version ");
    Serial.println(uploaderIdentity.version);
    returnPending = false;
    exitButton.begin();
    uploadDisplay.begin();
    // Keep app1 selected while waiting/transferring: opening native USB can
    // reset this board. RETURN or a verified upload selects app0 explicitly.
    uploadDisplay.show("Initializing flash");
    flashReady = flash.begin();
    uploadDisplay.show(flashReady ? "Waiting for host" : "Flash unavailable");
    // Do not wait for a serial monitor: the host uses PING to synchronize.
}

void loop()
{
    if (serviceEscape()) return;
    if (returnPending) {
        serviceReturn();
        delay(1);
        return; // Do not accept a second upload during the success countdown.
    }
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
        } else if (!strcmp(command, "RETURN")) {
            if (BootSlots::selectMain() == ESP_OK) ESP.restart();
            else Serial.println("ERROR cannot select app0");
        } else if (sscanf(command, "UPLOAD %" SCNu32 " %" SCNx32 " %c", &size, &checksum, &extra) == 2) {
            if (uploadImage(size, checksum)) scheduleReturn(size);
            else Serial.println("ERROR upload failed; image not committed");
        } else {
            uploadDisplay.show("Unknown command");
            Serial.println("ERROR unknown command");
        }
        used = 0;
        overflow = false;
        if (returnPending) break;
    }
    delay(1);
}
