#pragma once
#include "Arduino.h"
struct IPAddress {
    uint32_t value;
    String toString() const { return value == 456 ? "192.168.4.1" : "192.0.2.10"; }
    friend bool operator!=(IPAddress a, IPAddress b) { return a.value != b.value; }
    friend std::ostream &operator<<(std::ostream &out, IPAddress address) {
        return out << address.toString().c_str();
    }
};
constexpr int WIFI_STA = 1, WIFI_AP_STA = 3;
constexpr int WL_CONNECTED = 3, WL_DISCONNECTED = 6;
struct FakeWiFi {
    int state = WL_DISCONNECTED;
    int currentMode = WIFI_STA;
    bool ap = false;
    bool apWorks = true;
    String requestedSsid, associatedSsid;
    void persistent(bool) {}
    void setAutoReconnect(bool) {}
    void mode(int mode) { currentMode = mode; }
    String macAddress() { return "01:02:03:04:05:06"; }
    void begin(const char *ssid, const char *) { requestedSsid = ssid; state = WL_DISCONNECTED; }
    int status() { return state; }
    String SSID() { return associatedSsid; }
    void disconnect(bool, bool) { state = WL_DISCONNECTED; }
    bool softAP(const char *) { ap = apWorks; return ap; }
    void softAPdisconnect(bool) { ap = false; }
    IPAddress localIP() const { return {state == WL_CONNECTED ? 123U : 0U}; }
    IPAddress softAPIP() const { return {456}; }
    void connect() { state = WL_CONNECTED; associatedSsid = requestedSsid; }
};
inline FakeWiFi WiFi;
