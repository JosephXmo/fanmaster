#pragma once

#include <Adafruit_SSD1306.h>
#include <IPAddress.h>
#include <Wire.h>

class OledDisplay {
public:
    explicit OledDisplay(TwoWire &wire);

    bool begin();
    bool isReady() const;
    void requestPowerHint();
    void showPowerOff(bool waitingForKey);
    void showStatus(
        float rpm,
        uint8_t dutyPercent,
        bool wifiConnected,
        const IPAddress &ipAddress,
        const String &version,
        bool hasClimateReading,
        float temperatureC,
        float humidityPercent,
        bool rpmControl = false,
        uint32_t targetRpm = 0,
        bool thermalControl = false
    );
    Adafruit_SSD1306 &device();

private:
    Adafruit_SSD1306 display_;
    bool ready_ = false;
    enum class View : uint8_t { Unknown, Status, BootPrompt, PowerHint, Off };
    View view_ = View::Unknown;
    bool hintActive_ = false;
    uint32_t hintStartedAtMs_ = 0;
};
