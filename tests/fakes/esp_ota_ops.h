#pragma once
#include <cstdint>
#include <cstring>
using esp_err_t = int;
using esp_partition_subtype_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_ERR_NOT_FOUND = -1;
constexpr int ESP_PARTITION_TYPE_APP = 0;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10, ESP_PARTITION_SUBTYPE_APP_OTA_1 = 0x11;
struct esp_partition_t { int type; int subtype; };
struct esp_app_desc_t { char project_name[32]; char padding[224]; };
inline esp_partition_t fakeSlots[2] = {{0, 0x10}, {0, 0x11}};
inline bool fakeSlotPresent[2] = {true, true}, fakeSlotValid[2] = {true, true};
inline const char *fakeProjects[2] = {"fanmaster", "ResourceUploader"};
inline const esp_partition_t *fakeBootPartition = &fakeSlots[0];
inline int fakeBootWrites = 0;
inline esp_err_t fakeBootWriteResult = ESP_OK;
inline const esp_partition_t *esp_partition_find_first(int, int subtype, const char *) {
    for (int i = 0; i < 2; ++i)
        if (fakeSlotPresent[i] && fakeSlots[i].subtype == subtype) return &fakeSlots[i];
    return nullptr;
}
inline esp_err_t esp_ota_get_partition_description(const esp_partition_t *p, esp_app_desc_t *out) {
    const int i = p == &fakeSlots[1] ? 1 : 0;
    if (!fakeSlotValid[i]) return ESP_ERR_NOT_FOUND;
    std::strncpy(out->project_name, fakeProjects[i], sizeof(out->project_name) - 1);
    return ESP_OK;
}
inline const esp_partition_t *esp_ota_get_boot_partition() { return fakeBootPartition; }
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {
    ++fakeBootWrites;
    if (fakeBootWriteResult == ESP_OK) fakeBootPartition = p;
    return fakeBootWriteResult;
}
inline esp_err_t esp_partition_read(const esp_partition_t *p, size_t offset, void *out, size_t size) {
    const int i = p == &fakeSlots[1] ? 1 : 0;
    if (!fakeSlotValid[i] || offset != 288 || size != 40) return ESP_ERR_NOT_FOUND;
    std::memset(out, 0, size);
    std::memcpy(out, "FMBOOT1", 8);
    std::strncpy(static_cast<char *>(out) + 8, fakeProjects[i], 23);
    return ESP_OK;
}
