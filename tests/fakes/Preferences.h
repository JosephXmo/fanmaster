#pragma once
#include "Arduino.h"
#include <map>
class Preferences {
public:
    inline static std::vector<uint8_t> data;
    inline static bool writable = true, opens = true;
    inline static std::map<std::string, std::vector<uint8_t>> records;
    inline static size_t writeCount = 0, openCount = 0;
    bool begin(const char *name, bool) { name_ = name; ++openCount; return opens; }
    size_t getBytesLength(const char *key) { return record(key).size(); }
    size_t getBytes(const char *key, void *out, size_t size) {
        const auto &bytes = record(key);
        if (size != bytes.size()) return 0;
        std::memcpy(out, bytes.data(), size); return size;
    }
    size_t putBytes(const char *key, const void *in, size_t size) {
        ++writeCount;
        if (!writable) return 0;
        const auto *bytes = static_cast<const uint8_t *>(in);
        record(key).assign(bytes, bytes + size); return size;
    }
private:
    std::vector<uint8_t> &record(const char *key) {
        return name_ == "fanmaster" ? data : records[name_ + "/" + key];
    }
    std::string name_;
};
