#pragma once
#include <cstdint>
inline bool fakeWireAvailable = true;
struct TwoWire {
    bool begin(int, int, uint32_t) { return fakeWireAvailable; }
};
inline TwoWire Wire;
