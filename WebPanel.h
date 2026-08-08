#pragma once

#include <Arduino.h>
#include <WebServer.h>

class FanController;
class WifiStation;

class WebPanel {
public:
    WebPanel();

    void begin(FanController &fan, WifiStation &wifiStation);
    void update();

private:
    void handleRoot();
    void handleStatus();
    void handleSetDuty();
    void handleNotFound();
    void sendJson(int statusCode, const char *body);
    bool parseDutyArgument(uint8_t &dutyResult) const;

    WebServer server_;
    FanController *fan_ = nullptr;
    WifiStation *wifiStation_ = nullptr;
    bool started_ = false;
};
