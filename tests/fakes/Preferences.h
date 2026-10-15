#pragma once
#include "Arduino.h"
class Preferences {
public:
    inline static std::vector<uint8_t> data;
    inline static bool writable = true, opens = true;
    bool begin(const char *, bool) { return opens; }
    size_t getBytesLength(const char *) { return data.size(); }
    size_t getBytes(const char *, void *out, size_t size) {
        if (size != data.size()) return 0;
        std::memcpy(out, data.data(), size); return size;
    }
    size_t putBytes(const char *, const void *in, size_t size) {
        if (!writable) return 0;
        const auto *bytes = static_cast<const uint8_t *>(in);
        data.assign(bytes, bytes + size); return size;
    }
};
