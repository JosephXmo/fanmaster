#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ButtonInputs.h"
#include "ButtonControls.h"
#include "Config.h"
#include "ClimateReading.h"
#include "FanController.h"
#include "I2cBus.h"
#include "OledDisplay.h"
#include "OperatingMemory.h"
#include "SerialConsole.h"
#include "Sht30Sensor.h"
#include "StatusLed.h"
#include "WebPanel.h"
#include "WifiStation.h"

namespace {

FanController fan;
OperatingMemory operatingMemory;
I2cBus peripheralBus;
OledDisplay oled(peripheralBus.wire());
Sht30Sensor sht30;
ButtonInputs buttons;
ButtonControls buttonControls;
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
    Serial.print("% | Mode: ");
    const FanSnapshot state = fan.snapshot();
    Serial.print(state.mode == FanMode::Thermal ? "THERMAL" : state.mode == FanMode::Rpm ? "RPM" : "Duty");
    Serial.print(" | Target RPM: ");
    Serial.println(state.targetRpm);
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

void applyButtonActions()
{
    const ButtonActions actions = buttonControls.update(buttons);
    if (actions.direction) fan.adjustSetting(actions.direction, actions.fine);
    if (actions.toggleLed) statusLed.toggleEnabled();
    if (actions.modeSwitchRequested) {
        fan.toggleMode();
        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.println(fan.snapshot().mode == FanMode::Rpm ? "[BUTTON] RPM control" : "[BUTTON] duty control");
        }
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
    const ClimateSample sample = climate.snapshot();
    const FanSnapshot state = fan.snapshot();
    oled.showStatus(
        state.rpm,
        state.appliedDutyPercent,
        wifiStation.isConnected(),
        wifiStation.localIp(),
        Config::VERSION,
        sample.valid,
        sample.temperatureC,
        sample.humidityPercent,
        state.mode == FanMode::Rpm,
        state.targetRpm,
        state.mode == FanMode::Thermal
    );
}

void sampleSht30()
{
    if (!sht30.isReady()) {
        climate.publish({});
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
        climate.publish({true, temperatureC, humidityPercent, millis()});
    } else {
        climate.publish({});
    }
}

void serviceLocalControls()
{
    fan.updateTemperature(climate.snapshot());
    buttons.update();
    reportButtonEvents();
    applyButtonActions();
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

void networkTask(void *)
{
    // Only this task owns DNS, HTTP, provisioning and external asset reads.
    // Local controls and I2C never run from a resource-serving callback.
    wifiStation.begin();
    webPanel.begin(fan, wifiStation, nullptr, climate);
    while (true) {
        wifiStation.update();
        webPanel.update();
        operatingMemory.update(fan);
        vTaskDelay(1);
    }
}

}  // namespace

void setup()
{
    Serial.begin(Config::SerialConsole::BAUD_RATE);
#if ARDUINO_USB_CDC_ON_BOOT
    // USB can remain connected while no monitor drains its output. Diagnostics
    // must drop bytes instead of waiting seconds per print and stopping controls.
    Serial.setTxTimeoutMs(0);
#endif
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
    operatingMemory.begin(fan);
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
    Serial.println("Or: rpm <target>, mode rpm, mode duty.");

    // WebServer's parser can yield without sleeping. Share idle priority so
    // IDLE0 still runs/feeds its watchdog even during a slow request header.
    if (xTaskCreatePinnedToCore(networkTask, "fanmaster-net", 8192, nullptr,
                                tskIDLE_PRIORITY, nullptr, 0) != pdPASS) {
        Serial.println("ERROR: network task could not start; local controls remain active.");
    }
}

void loop()
{
    static uint32_t previousMs = 0, maxGapMs = 0, lastReportMs = 0;
    const uint32_t now = millis();
    if (previousMs && now - previousMs > maxGapMs) maxGapMs = now - previousMs;
    previousMs = now;
    sampleSht30();
    serviceLocalControls();
    if (!Config::SerialConsole::PLOTTER_MODE &&
        now - lastReportMs >= Config::Diagnostics::REPORT_INTERVAL_MS) {
        Serial.print("[LOCAL] max_loop_gap_ms=");
        Serial.println(maxGapMs);
        maxGapMs = 0;
        lastReportMs = now;
    }

    delay(1);
}
