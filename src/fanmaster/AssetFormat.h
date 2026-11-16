#pragma once

#include <stddef.h>
#include <stdint.h>

namespace AssetFormat {
constexpr char MAGIC[8] = {'F', 'M', 'A', 'S', 'S', 'E', 'T', '1'};
constexpr uint32_t VERSION = 1;
constexpr uint32_t CAPACITY = 8UL * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 64;

// Little-endian on disk. No pointers or compiler-dependent padding.
struct Header {
    char magic[8];
    uint32_t version;
    uint32_t entryCount;
    uint32_t imageBytes;
    uint32_t indexCrc;
    uint32_t payloadCrc;
    uint32_t headerCrc;
};

struct Entry {
    char path[96];
    char contentType[48];
    uint32_t offset;
    uint32_t length;
    uint32_t crc;
    uint32_t reserved;
};

static_assert(sizeof(Header) == 32, "Asset header layout changed");
static_assert(sizeof(Entry) == 160, "Asset entry layout changed");

inline uint32_t crcUpdate(uint32_t crc, const void *bytes, size_t length)
{
    const auto *data = static_cast<const uint8_t *>(bytes);
    while (length--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1)));
        }
    }
    return crc;
}
}  // namespace AssetFormat
