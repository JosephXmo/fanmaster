#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "AssetStore.h"
#include "ClimateReading.h"

class FanController;
class WifiStation;

class WebPanel {
public:
    WebPanel();

    void begin(FanController &fan, WifiStation &wifiStation,
               void (*serviceLocalControls)(), const ClimateReading &climate);
    void update();

private:
    void recordRequest(const char *label, bool verbose);
    void reportDiagnostics(uint32_t now);
    void handleRoot();
    void handleStatus();
    void handleSetDuty();
    void handleSetRpm();
    void handleSetMode();
    void handlePower();
    void handleThermalMode();
    void handleThermalCurve();
    void sendStatus();
    bool parseTemperature(const char *name, uint16_t &result) const;
    void handleProvision();
    void handleWifiStatus();
    void handleSetup();
    bool serveAsset(const char *path);
    void handleNotFound();
    void sendJson(int statusCode, const char *body);
    bool parseDutyArgument(uint8_t &dutyResult) const;
    bool parseUnsignedArgument(const char *name, uint32_t maximum, uint32_t &result) const;

    WebServer server_;
    FanController *fan_ = nullptr;
    WifiStation *wifiStation_ = nullptr;
    const ClimateReading *climate_ = nullptr;
    AssetStore assets_;
    void (*serviceLocalControls_)() = nullptr;
    bool started_ = false;
    uint32_t requestCount_ = 0;
    uint32_t lastUpdateMs_ = 0;
    uint32_t maxUpdateGapMs_ = 0;
    uint32_t lastDiagnosticMs_ = 0;
};
