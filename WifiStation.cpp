#include "WifiStation.h"

#include "Config.h"

void WifiStation::begin()
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    macAddress_ = WiFi.macAddress();
    startConnectionAttempt(millis());
}

void WifiStation::update()
{
    const uint32_t now = millis();
    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        connecting_ = false;

        if (!connected_) {
            connected_ = true;
            if (!Config::SerialConsole::PLOTTER_MODE) {
                Serial.print("[WIFI] connected ip=");
                Serial.print(WiFi.localIP());
                Serial.print(" gateway=");
                Serial.print(WiFi.gatewayIP());
                Serial.print(" subnet=");
                Serial.print(WiFi.subnetMask());
                Serial.print(" rssi_dbm=");
                Serial.print(WiFi.RSSI());
                Serial.print(" channel=");
                Serial.println(WiFi.channel());
            }
        }

        return;
    }

    if (connected_) {
        connected_ = false;
        connecting_ = false;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.println("[WIFI] disconnected; reconnect scheduled");
        }
    }

    if (connecting_) {
        if (now - attemptStartedMs_ < Config::Wifi::CONNECT_TIMEOUT_MS) {
            return;
        }

        WiFi.disconnect(false, false);
        connecting_ = false;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.println("[WIFI] connection timed out; reconnect scheduled");
        }
        return;
    }

    if (static_cast<int32_t>(now - nextAttemptMs_) >= 0) {
        startConnectionAttempt(now);
    }
}

bool WifiStation::isConnected() const
{
    return connected_;
}

IPAddress WifiStation::localIp() const
{
    return WiFi.localIP();
}

const String &WifiStation::macAddress() const
{
    return macAddress_;
}

void WifiStation::startConnectionAttempt(uint32_t now)
{
    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.print("[WIFI] connecting to SSID: ");
        Serial.println(Config::Wifi::SSID);
    }

    WiFi.begin(Config::Wifi::SSID, Config::Wifi::PASSWORD);
    attemptStartedMs_ = now;
    connecting_ = true;
}
