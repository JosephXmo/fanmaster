#include <Arduino.h>

#include "ButtonInputs.h"
#include "Config.h"
#include "ClimateReading.h"
#include "FanController.h"
#include "I2cBus.h"
#include "OledDisplay.h"
#include "SerialConsole.h"
#include "Sht30Sensor.h"
#include "StatusLed.h"
#include "WebPanel.h"
#include "WifiStation.h"

namespace {

FanController fan;
I2cBus peripheralBus;
OledDisplay oled(peripheralBus.wire());
Sht30Sensor sht30;
ButtonInputs buttons;
SerialConsole console;
StatusLed statusLed;
WifiStation wifiStation;
WebPanel webPanel;
uint32_t lastOledRefreshMs = 0;
ClimateReading climate;
uint32_t lastSht30SampleMs = 0;

void haltAfterStartupError()
{
    Serial.println("Program halted.");

    while (true) {
        delay(1000);
    }
}

void reportFanState(float rpm)
{
    if (Config::SerialConsole::PLOTTER_MODE) {
        Serial.print("RPM:");
        Serial.print(rpm, 0);
        Serial.print('\t');
        Serial.print("Duty:");
        Serial.println(fan.dutyPercent());
        return;
    }

    Serial.print("Fan speed: ");
    Serial.print(rpm, 0);
    Serial.print(" RPM | Duty: ");
    Serial.print(fan.dutyPercent());
    Serial.println('%');
}

void reportButtonEvents()
{
    if (Config::SerialConsole::PLOTTER_MODE) {
        return;
    }

    for (size_t index = 0; index < Config::Buttons::COUNT; index++) {
        const ButtonId button = static_cast<ButtonId>(index);

        if (!buttons.wasPressed(button) && !buttons.wasReleased(button)) {
            continue;
        }

        Serial.print("Button ");
        Serial.print(index + 1);
        Serial.println(buttons.wasPressed(button) ? " pressed" : " released");
    }
}

void adjustDutyFromButtons()
{
    const bool increase = buttons.wasPressed(ButtonId::Button1);
    const bool decrease = buttons.wasPressed(ButtonId::Button2);

    if (increase == decrease) {
        return;
    }

    const uint8_t step = buttons.isPressed(ButtonId::Button3)
        ? Config::Buttons::MODIFIED_DUTY_STEP_PERCENT
        : Config::Buttons::DUTY_STEP_PERCENT;
    const int16_t signedStep = increase ? step : -step;
    int16_t nextDuty = fan.dutyPercent() + signedStep;

    if (nextDuty < 0) {
        nextDuty = 0;
    } else if (nextDuty > 100) {
        nextDuty = 100;
    }

    fan.setDutyPercent(static_cast<uint8_t>(nextDuty));
}

void toggleStatusLedFromButton()
{
    if (buttons.wasPressed(ButtonId::Button4)) {
        statusLed.toggleEnabled();
    }
}

void updateOled()
{
    if (!oled.isReady()) {
        return;
    }

    const uint32_t now = millis();
    if (now - lastOledRefreshMs < Config::Oled::REFRESH_INTERVAL_MS) {
        return;
    }

    lastOledRefreshMs = now;
    oled.showStatus(
        fan.currentRpm(),
        fan.dutyPercent(),
        wifiStation.isConnected(),
        wifiStation.localIp(),
        Config::VERSION,
        climate.valid,
        climate.temperatureC,
        climate.humidityPercent
    );
}

void sampleSht30()
{
    if (!sht30.isReady()) {
        climate.valid = false;
        return;
    }

    const uint32_t now = millis();
    if (now - lastSht30SampleMs < Config::Sht30::SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSht30SampleMs = now;

    float temperatureC;
    float humidityPercent;
    if (sht30.read(temperatureC, humidityPercent)) {
        climate.temperatureC = temperatureC;
        climate.humidityPercent = humidityPercent;
        climate.valid = true;
    } else {
        climate.valid = false;
    }
}

void serviceLocalControls()
{
    buttons.update();
    reportButtonEvents();
    adjustDutyFromButtons();
    toggleStatusLedFromButton();
    console.update(fan);

    float rpm;
    if (fan.updateRpm(rpm)) {
        statusLed.setRpm(rpm);
        reportFanState(rpm);
    }

    statusLed.setDutyPercent(fan.dutyPercent());
    statusLed.update();
    updateOled();
}

}  // namespace

void setup()
{
    Serial.begin(Config::SerialConsole::BAUD_RATE);
    delay(200);

    Serial.println();
    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.println("ESP32 PWM fan controller starting...");
    }

    statusLed.begin();
    buttons.begin();

    // The two reserved peripheral connectors share this I2C bus.
    if (!peripheralBus.begin()) {
        Serial.println("WARNING: Failed to initialize the peripheral I2C bus.");
    } else {
        if (!oled.begin()) {
            Serial.println("WARNING: SSD1306 OLED initialization failed.");
        }

        if (!sht30.begin()) {
            Serial.println("WARNING: SHT30 initialization failed.");
        }
    }

    Serial.println("Starting fan at 100% duty...");
    if (!fan.begin()) {
        Serial.println("ERROR: Failed to initialize fan PWM.");
        haltAfterStartupError();
    }

    Serial.print("Fan PWM frequency: ");
    Serial.print(Config::Fan::PWM_FREQUENCY_HZ);
    Serial.println(" Hz");

    Serial.print("Initial duty cycle: ");
    Serial.print(fan.dutyPercent());
    Serial.println('%');

    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.print("I2C bus: SDA GPIO ");
        Serial.print(Config::Pins::I2C_SDA);
        Serial.print(", SCL GPIO ");
        Serial.println(Config::Pins::I2C_SCL);
    }

    Serial.println();
    Serial.println("Enter an integer from 0 to 100 to change duty cycle.");

    wifiStation.begin();
    webPanel.begin(fan, wifiStation, serviceLocalControls, climate);
}

void loop()
{
    wifiStation.update();
    sampleSht30();
    webPanel.update();
    serviceLocalControls();

    delay(1);
}
