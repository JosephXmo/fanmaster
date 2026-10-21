#pragma once
#include "Arduino.h"
#include "Wire.h"
constexpr int SSD1306_SWITCHCAPVCC = 2, SSD1306_WHITE = 1;
constexpr int SSD1306_DISPLAYOFF = 0xAE, SSD1306_DISPLAYON = 0xAF;
inline bool fakeOledAvailable = true;
inline std::vector<std::string> fakeOledFrames;
inline std::vector<int> fakeOledCommands;
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
    void print(const String &value) { text_ += value.c_str(); }
    void print(float value, int) { print(value); }
    void ssd1306_command(int command) { fakeOledCommands.push_back(command); }
    void drawFastHLine(int, int, int, int) {}
    void drawRect(int, int, int, int, int) {}
    void fillRect(int, int, int, int, int) {}
    int width() const { return width_; }
    void display() { fakeOledFrames.push_back(text_); }
private:
    int width_;
    std::string text_;
};
