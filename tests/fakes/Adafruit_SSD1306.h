#pragma once
#include "Arduino.h"
#include "Wire.h"
constexpr int SSD1306_SWITCHCAPVCC = 2, SSD1306_WHITE = 1;
inline bool fakeOledAvailable = true;
inline std::vector<std::string> fakeOledFrames;
class Adafruit_SSD1306 {
public:
    Adafruit_SSD1306(int width, int, TwoWire *, int) : width_(width) {}
    bool begin(int, int, bool, bool) { return fakeOledAvailable; }
    void clearDisplay() { text_.clear(); }
    void setTextColor(int) {}
    void setTextSize(int) {}
    void setTextWrap(bool) {}
    void setCursor(int, int) { text_ += '\n'; }
    template<class T> void print(const T &value) { std::ostringstream s; s << value; text_ += s.str(); }
    void drawRect(int, int, int, int, int) {}
    void fillRect(int, int, int, int, int) {}
    int width() const { return width_; }
    void display() { fakeOledFrames.push_back(text_); }
private:
    int width_;
    std::string text_;
};
