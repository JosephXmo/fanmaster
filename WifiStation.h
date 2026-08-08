#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiStation {
public:
    void begin();
    void update();

    bool isConnected() const;
    IPAddress localIp() const;

private:
    void startConnectionAttempt(uint32_t now);

    uint32_t attemptStartedMs_ = 0;
    uint32_t nextAttemptMs_ = 0;
    bool connecting_ = false;
    bool connected_ = false;
};
