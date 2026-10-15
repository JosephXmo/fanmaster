#include "OperatingMemory.h"

#include <stddef.h>
#include "AssetFormat.h"

namespace {
constexpr uint32_t LEGACY_MAGIC = 0x314F4D46; // FMO1
constexpr uint32_t MAGIC = 0x324F4D46; // FMO2, independent of Wi-Fi NVS.
struct LegacyRecord {
    uint32_t magic, rpm;
    uint8_t duty, reserved[3];
    uint32_t crc;
};
struct Record {
    uint32_t magic, rpm;
    uint8_t duty, reserved[3];
    uint16_t temperatures[3];
    uint8_t duties[3], reserved2[3];
    uint32_t crc;
};
static_assert(sizeof(LegacyRecord) == 16 && sizeof(Record) == 28, "Operating memory layout changed");
template<class T> uint32_t checksum(const T &record) {
    return AssetFormat::crcUpdate(0xFFFFFFFF, &record, offsetof(T, crc)) ^ 0xFFFFFFFF;
}
}

void OperatingMemory::begin(FanController &fan)
{
    if (!enabled_) return;
    ready_ = preferences_.begin("fanmaster-op", false);
    if (!ready_) {
        Serial.println("[MEMORY] NVS unavailable; using configured defaults");
        return;
    }
    const auto length = preferences_.getBytesLength("settings");
    OperatingSettings settings;
    if (length == sizeof(LegacyRecord)) {
        LegacyRecord legacy{};
        if (preferences_.getBytes("settings", &legacy, sizeof(legacy)) != sizeof(legacy) ||
            legacy.magic != LEGACY_MAGIC || legacy.crc != checksum(legacy) || legacy.duty > 100) return;
        settings.targetRpm = legacy.rpm;
        settings.dutyPercent = legacy.duty;
        // Upgrade lazily on the next changed setting; preserve the existing presets.
    } else if (length == sizeof(Record)) {
        Record record{};
        if (preferences_.getBytes("settings", &record, sizeof(record)) != sizeof(record) ||
            record.magic != MAGIC || record.crc != checksum(record) || record.duty > 100) return;
        settings.targetRpm = record.rpm;
        settings.dutyPercent = record.duty;
        for (size_t i = 0; i < 3; ++i)
            settings.thermalCurve.points[i] = {record.temperatures[i], record.duties[i]};
        if (!settings.thermalCurve.valid()) return;
    } else return;
    // Clamp RPM to current Config limits if the firmware range has changed.
    fan.restoreSettings(settings);
}

void OperatingMemory::update(FanController &fan)
{
    if (!enabled_ || !ready_) return;
    OperatingSettings settings;
    const uint32_t revision = fan.rememberedSettings(settings);
    if (revision == savedRevision_) return;
    const uint32_t now = millis();
    if (attempted_ && now - lastAttemptMs_ < Config::OperatingMemory::RETRY_INTERVAL_MS) return;
    attempted_ = true;
    lastAttemptMs_ = now;
    Record record{};
    record.magic = MAGIC;
    record.rpm = settings.targetRpm;
    record.duty = settings.dutyPercent;
    for (size_t i = 0; i < 3; ++i) {
        record.temperatures[i] = settings.thermalCurve.points[i].temperatureDeciC;
        record.duties[i] = settings.thermalCurve.points[i].dutyPercent;
    }
    record.crc = checksum(record);
    if (preferences_.putBytes("settings", &record, sizeof(record)) == sizeof(record)) {
        savedRevision_ = revision;
    } else {
        Serial.println("[MEMORY] save failed; retry pending");
    }
}
