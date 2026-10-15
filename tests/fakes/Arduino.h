#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstddef>
#include <vector>
#include <sstream>
using std::size_t;
using std::min;
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline void delay(uint32_t ms) { fakeMillis += ms; }
inline void delayMicroseconds(uint32_t) {}
inline void yield() {}
#define IRAM_ATTR
#define PROGMEM
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(int *) {}
inline void portEXIT_CRITICAL(int *) {}
inline void portENTER_CRITICAL_ISR(int *) {}
inline void portEXIT_CRITICAL_ISR(int *) {}
constexpr int OUTPUT = 1, HIGH = 1, LOW = 0, INPUT_PULLUP = 2, FALLING = 3;
inline bool ledcAttach(int, int, int) { return true; }
inline void ledcWrite(int, int) {}
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int, void (*)(), int) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
class String {
public:
    String() = default;
    String(const char *s) : value_(s) {}
    String(const std::string &s) : value_(s) {}
    size_t length() const { return value_.size(); }
    const char *c_str() const { return value_.c_str(); }
    char operator[](size_t i) const { return value_[i]; }
    char charAt(size_t i) const { return value_[i]; }
    bool startsWith(const char *prefix) const { return value_.rfind(prefix, 0) == 0; }
    void toCharArray(char *out, size_t capacity) const {
        std::strncpy(out, value_.c_str(), capacity - 1); out[capacity - 1] = 0;
    }
    void replace(const char *from, const char *to) {
        size_t pos = 0;
        while ((pos = value_.find(from, pos)) != std::string::npos) {
            value_.replace(pos, std::strlen(from), to); pos += std::strlen(to);
        }
    }
    String substring(size_t from) const { return value_.substr(from); }
    friend String operator+(const String &a, const String &b) { return a.value_ + b.value_; }
    friend bool operator==(const String &a, const String &b) { return a.value_ == b.value_; }
private:
    std::string value_;
};
struct FakeSerial {
    std::string output;
    std::vector<uint8_t> incoming;
    size_t position = 0;
    template<class T> void print(const T &value) {
        std::ostringstream stream; stream << value; output += stream.str();
    }
    void print(const String &value) { output += value.c_str(); }
    template<class T> void println(const T &value) { print(value); output += "\n"; }
    void begin(int) {}
    void setTimeout(int) {}
    int available() { return position < incoming.size(); }
    int read() { return available() ? incoming[position++] : -1; }
    size_t readBytes(uint8_t *out, size_t size) {
        size = std::min(size, incoming.size() - position);
        std::memcpy(out, incoming.data() + position, size);
        position += size;
        return size;
    }
};
inline FakeSerial Serial;
struct FakeESP { uint32_t getFreeHeap() { return 100000; } };
inline FakeESP ESP;
