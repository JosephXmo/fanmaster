#pragma once

#include <cstring>
#include <mutex>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include "FirmwareIdentity.h"

// Only otadata is changed here. Neither application image nor user NVS is written.
namespace BootSlots {
inline std::mutex mutex;

inline const esp_partition_t *application(esp_partition_subtype_t slot, const char *role)
{
    const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, slot, nullptr);
    esp_app_desc_t description{};
    FirmwareIdentity::Descriptor identity{};
    constexpr size_t identityOffset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    if (!partition || esp_ota_get_partition_description(partition, &description) != ESP_OK ||
        esp_partition_read(partition, identityOffset, &identity, sizeof(identity)) != ESP_OK ||
        std::memcmp(identity.magic, "FMBOOT1", sizeof(identity.magic)) != 0 ||
        std::strncmp(identity.role, role, sizeof(identity.role)) != 0)
        return nullptr;
    return partition;
}

inline bool updaterAvailable()
{
    std::lock_guard<std::mutex> lock(mutex);
    return application(ESP_PARTITION_SUBTYPE_APP_OTA_1, "ResourceUploader") != nullptr;
}

inline bool updaterSelected()
{
    std::lock_guard<std::mutex> lock(mutex);
    const auto *boot = esp_ota_get_boot_partition();
    return boot && boot->type == ESP_PARTITION_TYPE_APP && boot->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1;
}

inline esp_err_t select(esp_partition_subtype_t slot, const char *role)
{
    std::lock_guard<std::mutex> lock(mutex);
    const auto *partition = application(slot, role);
    if (!partition) return ESP_ERR_NOT_FOUND;
    // ESP-IDF verifies the destination image before committing otadata.
    return esp_ota_set_boot_partition(partition);
}

inline esp_err_t selectUpdater()
{
    return select(ESP_PARTITION_SUBTYPE_APP_OTA_1, "ResourceUploader");
}

inline esp_err_t selectMain()
{
    return select(ESP_PARTITION_SUBTYPE_APP_OTA_0, "fanmaster");
}
}  // namespace BootSlots
