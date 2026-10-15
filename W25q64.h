#pragma once

#include <Arduino.h>
#include <memory>

class SPIFlash;

// Bounds-checked adapter. The runtime only calls begin/read; the separate
// uploader is the only firmware that calls eraseSector/programPage.
class W25q64 {
public:
    W25q64();
    ~W25q64();
    bool begin();
    bool read(uint32_t address, void *buffer, size_t length);
    bool eraseSector(uint32_t address);
    bool programPage(uint32_t address, const void *buffer, size_t length);

private:
    // Keep SPIMemory's generic macros (notably ID) out of DNS/Web headers.
    std::unique_ptr<SPIFlash> flash_;
    bool ready_ = false;
};
