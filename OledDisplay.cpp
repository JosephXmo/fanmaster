#include "OledDisplay.h"

#include "Config.h"

OledDisplay::OledDisplay(TwoWire &wire)
    : display_(
          Config::I2c::OLED_WIDTH,
          Config::I2c::OLED_HEIGHT,
          &wire,
          Config::Pins::OLED_RESET
      )
{
}

bool OledDisplay::begin()
{
    // I2cBus already owns Wire.begin(), so do not let the library reset it.
    ready_ = display_.begin(
        SSD1306_SWITCHCAPVCC,
        Config::I2c::OLED_ADDRESS,
        true,
        false
    );
    return ready_;
}

bool OledDisplay::isReady() const
{
    return ready_;
}

void OledDisplay::requestPowerHint()
{
    hintActive_ = true;
    hintStartedAtMs_ = millis();
}

void OledDisplay::showPowerOff(bool waitingForKey)
{
    if (!ready_) return;
    if (hintActive_ && millis() - hintStartedAtMs_ >= Config::Power::HINT_MS)
        hintActive_ = false;
    const View next = waitingForKey ? View::BootPrompt : hintActive_ ? View::PowerHint : View::Off;
    if (next == view_) return;
    if (next == View::Off) {
        display_.ssd1306_command(SSD1306_DISPLAYOFF);
    } else {
        display_.clearDisplay();
        display_.setTextColor(SSD1306_WHITE);
        display_.setTextSize(1);
        display_.setTextWrap(false);
        display_.setCursor(0, 14);
        display_.print(waitingForKey ? "Press any key to" : "Hold ^ and * for");
        display_.setCursor(0, 26);
        display_.print(waitingForKey ? "power on." : "1 second to power on");
        if (!waitingForKey) {
            display_.setCursor(0, 38);
            display_.print("the fan.");
        }
        display_.display();
        display_.ssd1306_command(SSD1306_DISPLAYON);
    }
    view_ = next;
}

void OledDisplay::showStatus(
    float rpm,
    uint8_t dutyPercent,
    bool wifiConnected,
    const IPAddress &ipAddress,
    const String &version,
    bool hasClimateReading,
    float temperatureC,
    float humidityPercent,
    bool rpmControl,
    uint32_t targetRpm,
    bool thermalControl
)
{
    if (!ready_) {
        return;
    }

    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextWrap(false);

    display_.setTextSize(1);
    display_.setCursor(0, 0);
    display_.print("FANMASTER  VER ");
    display_.print(version);
    display_.drawFastHLine(0, 10, display_.width(), SSD1306_WHITE);

    display_.setTextSize(2);
    display_.setCursor(0, 14);
    display_.print("RPM ");
    display_.print(rpm, 0);

    display_.setTextSize(1);
    display_.setCursor(0, 34);
    display_.print("DUTY ");
    display_.print(dutyPercent);
    display_.print("%");
    if (thermalControl) {
        display_.print(" THERMAL");
    } else if (rpmControl) {
        display_.print(" SET ");
        display_.print(targetRpm);
    } else {
        display_.print(" MANUAL");
    }

    display_.setCursor(0, 44);
    display_.print("IP   ");
    if (wifiConnected) {
        display_.print(ipAddress);
    } else {
        display_.print("not connected");
    }


    display_.setCursor(0, 54);
    display_.print("TEMP ");
    if (hasClimateReading) {
        display_.print(temperatureC, 1);
        display_.print("C  HUMI ");
        display_.print(humidityPercent, 0);
        display_.print('%');
    } else {
        display_.print("--.-C H --%");
    }

    display_.display();
    if (view_ != View::Status) display_.ssd1306_command(SSD1306_DISPLAYON);
    view_ = View::Status;
    hintActive_ = false;
}

Adafruit_SSD1306 &OledDisplay::device()
{
    return display_;
}
