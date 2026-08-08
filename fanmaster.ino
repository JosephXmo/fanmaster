#include <Arduino.h>

#include "ButtonInputs.h"
#include "Config.h"
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
    webPanel.begin(fan, wifiStation);
}

void loop()
{
    wifiStation.update();
    webPanel.update();
    buttons.update();
    console.update(fan);

    float rpm;
    if (fan.updateRpm(rpm)) {
        reportFanState(rpm);
    }

    statusLed.addCompletedRevolutions(fan.takeCompletedRevolutions());
    statusLed.update();

    delay(1);
}
