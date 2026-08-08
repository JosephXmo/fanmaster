#include "WifiStation.h"

#include "Config.h"

void WifiStation::begin()
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
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
            Serial.print("Wi-Fi connected. Web Panel: http://");
            Serial.print(WiFi.localIP());
            Serial.println('/');
        }

        return;
    }

    if (connected_) {
        connected_ = false;
        connecting_ = false;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
        Serial.println("Wi-Fi disconnected; reconnect scheduled.");
    }

    if (connecting_) {
        if (now - attemptStartedMs_ < Config::Wifi::CONNECT_TIMEOUT_MS) {
            return;
        }

        WiFi.disconnect(false, false);
        connecting_ = false;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
        Serial.println("Wi-Fi connection timed out; reconnect scheduled.");
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

void WifiStation::startConnectionAttempt(uint32_t now)
{
    Serial.print("Connecting to Wi-Fi SSID: ");
    Serial.println(Config::Wifi::SSID);

    WiFi.begin(Config::Wifi::SSID, Config::Wifi::PASSWORD);
    attemptStartedMs_ = now;
    connecting_ = true;
}
