#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <atomic>

class WifiStation {
public:
    void begin();
    void update();
    bool provision(const String &ssid, const String &password);
    bool isConnected() const;
    bool isSetupActive() const;
    bool isConnecting() const;
    bool isProvisioning() const;
    IPAddress localIp() const;
    IPAddress setupIp() const;
    const char *provisionState() const;

private:
    bool loadCredentials();
    bool saveCredentials();
    void startConnectionAttempt(uint32_t now);
    void startSetup();
    void stopSetup();

    Preferences preferences_;
    DNSServer dns_;
    String ssid_;
    String password_;
    String setupName_;
    uint32_t attemptStartedMs_ = 0;
    uint32_t nextAttemptMs_ = 0;
    uint32_t failureStartedMs_ = 0;
    uint32_t connectedAtMs_ = 0;
    uint32_t lastSetupAttemptMs_ = 0;
    bool nvsReady_ = false;
    bool connecting_ = false;
    std::atomic<bool> connected_{false};
    bool setupActive_ = false;
    bool candidate_ = false;
    bool attemptPending_ = false;
    bool setupAttempted_ = false;
    const char *provisionState_ = "idle";
};
