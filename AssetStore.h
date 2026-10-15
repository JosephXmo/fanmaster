#pragma once

#include "AssetFormat.h"
#include "W25q64.h"

class AssetStore {
public:
    // Validates the header/index at startup; verifies each file before serving it.
    bool begin();
    bool find(const char *path, AssetFormat::Entry &entry,
              void (*serviceLocalControls)() = nullptr);
    bool read(const AssetFormat::Entry &entry, uint32_t offset,
              void *buffer, size_t length);

private:
    W25q64 flash_;
    AssetFormat::Header header_{};
    uint64_t verified_ = 0;
    uint64_t failed_ = 0;
    bool ready_ = false;
};
