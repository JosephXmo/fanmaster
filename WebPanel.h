#pragma once

#include <Arduino.h>
#include <WebServer.h>

class FanController;
class WifiStation;
class Sht30Sensor;

class WebPanel {
public:
    WebPanel();

    void begin(
        FanController &fan,
        WifiStation &wifiStation,
        Sht30Sensor &sht30Sensor,
        bool &hasClimateReading,
        float &temperatureC,
        float &humidityPercent
    );
    void update();

private:
    void recordRequest(const char *label, bool verbose);
    void reportDiagnostics(uint32_t now);
    void handleRoot();
    void handleStatus();
    void handleSetDuty();
    void handleNotFound();
    void sendJson(int statusCode, const char *body);
    bool parseDutyArgument(uint8_t &dutyResult) const;

    WebServer server_;
    FanController *fan_ = nullptr;
    WifiStation *wifiStation_ = nullptr;
    Sht30Sensor *sht30Sensor_ = nullptr;
    bool *hasClimateReading_ = nullptr;
    float *temperatureC_ = nullptr;
    float *humidityPercent_ = nullptr;
    bool started_ = false;
    uint32_t requestCount_ = 0;
    uint32_t lastUpdateMs_ = 0;
    uint32_t maxUpdateGapMs_ = 0;
    uint32_t lastDiagnosticMs_ = 0;
};
