#include "AssetStore.h"

#include <string.h>

bool AssetStore::begin()
{
    ready_ = false;
    verified_ = failed_ = 0;
    if (!flash_.begin() || !flash_.read(0, &header_, sizeof(header_))) return false;
    if (memcmp(header_.magic, AssetFormat::MAGIC, 8) ||
        header_.version != AssetFormat::VERSION ||
        !header_.entryCount || header_.entryCount > AssetFormat::MAX_ENTRIES ||
        header_.imageBytes > AssetFormat::CAPACITY ||
        header_.imageBytes < sizeof(header_) + header_.entryCount * sizeof(AssetFormat::Entry) ||
        (AssetFormat::crcUpdate(0xFFFFFFFF, &header_, 28) ^ 0xFFFFFFFF) != header_.headerCrc) {
        return false;
    }
    uint32_t crc = 0xFFFFFFFF;
    uint32_t nextOffset = sizeof(header_) + header_.entryCount * sizeof(AssetFormat::Entry);
    char previousPath[96] = {};
    for (uint32_t i = 0; i < header_.entryCount; ++i) {
        AssetFormat::Entry entry;
        if (!flash_.read(sizeof(header_) + i * sizeof(entry), &entry, sizeof(entry))) return false;
        crc = AssetFormat::crcUpdate(crc, &entry, sizeof(entry));
        if (!memchr(entry.path, 0, sizeof(entry.path)) ||
            !memchr(entry.contentType, 0, sizeof(entry.contentType)) ||
            entry.path[0] != '/' || !entry.contentType[0] ||
            strchr(entry.contentType, '\r') || strchr(entry.contentType, '\n') ||
            entry.reserved || strcmp(previousPath, entry.path) >= 0 ||
            entry.offset != nextOffset || entry.offset > header_.imageBytes ||
            entry.length > header_.imageBytes - entry.offset) return false;
        strcpy(previousPath, entry.path);
        nextOffset += entry.length;
    }
    ready_ = nextOffset == header_.imageBytes && (crc ^ 0xFFFFFFFF) == header_.indexCrc;
    return ready_;
}

bool AssetStore::find(const char *path, AssetFormat::Entry &entry,
                      void (*serviceLocalControls)())
{
    if (!ready_) return false;
    for (uint32_t i = 0; i < header_.entryCount; ++i) {
        if (!flash_.read(sizeof(header_) + i * sizeof(entry), &entry, sizeof(entry))) return false;
        if (strcmp(path, entry.path)) continue;
        const uint64_t bit = 1ULL << i;
        if (failed_ & bit) return false;
        if (!(verified_ & bit)) {
            uint8_t buffer[512];
            uint32_t crc = 0xFFFFFFFF;
            for (uint32_t offset = 0; offset < entry.length;) {
                const size_t length = min(sizeof(buffer), static_cast<size_t>(entry.length - offset));
                if (!read(entry, offset, buffer, length)) return false;
                crc = AssetFormat::crcUpdate(crc, buffer, length);
                offset += length;
                if (serviceLocalControls) serviceLocalControls();
                yield();
            }
            if ((crc ^ 0xFFFFFFFF) != entry.crc) {
                failed_ |= bit;
                return false;
            }
            verified_ |= bit;
        }
        return true;
    }
    return false;
}

bool AssetStore::read(const AssetFormat::Entry &entry, uint32_t offset,
                      void *buffer, size_t length)
{
    return ready_ && offset <= entry.length && length <= entry.length - offset &&
           flash_.read(entry.offset + offset, buffer, length);
}
