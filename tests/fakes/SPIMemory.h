#pragma once
#include "Arduino.h"
inline std::vector<uint8_t> fakeFlash(8 * 1024 * 1024, 0xff);
inline bool fakeFlashConnected = true;
constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings { SPISettings(uint32_t, int, int) {} };
struct FakeSPI {
    uint8_t opcode = 0;
    size_t position = 0;
    bool begin(int, int, int, int) { return true; }
    void beginTransaction(SPISettings) { opcode = 0; position = 0; }
    void endTransaction() {}
    uint8_t transfer(uint8_t value) {
        if (!fakeFlashConnected) return 0xff;
        if (!opcode) { opcode = value; return 0; }
        const uint8_t id[] = {0xef, 0x40, 0x17};
        return opcode == 0x9f && position < sizeof(id) ? id[position++] : 0;
    }
};
inline FakeSPI SPI;
class SPIFlash {
public:
    SPIFlash(int, FakeSPI *) {}
    void setClock(uint32_t) {}
    bool begin() { return true; }
    uint32_t getCapacity() { return fakeFlash.size(); }
    uint32_t getJEDECID() { return 0xef4017; }
    bool readByteArray(uint32_t offset, uint8_t *out, size_t size) {
        std::memcpy(out, fakeFlash.data() + offset, size); return true;
    }
    bool writeByteArray(uint32_t offset, uint8_t *in, size_t size, bool) {
        for (size_t i = 0; i < size; ++i) fakeFlash[offset + i] &= in[i];
        return !std::memcmp(in, fakeFlash.data() + offset, size);
    }
    bool eraseSector(uint32_t offset) {
        std::fill(fakeFlash.begin() + offset, fakeFlash.begin() + offset + 4096, 0xff);
        return true;
    }
};
