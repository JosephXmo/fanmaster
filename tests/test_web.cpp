#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <SPIMemory.h>
#include "Config.h"
#include "FanController.h"
#include "WebPanel.h"
#include "WifiStation.h"

unsigned serviceCount = 0;
void localControls() { ++serviceCount; }

int main(int argc, char **argv)
{
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    assert(!image.empty());
    std::copy(image.begin(), image.end(), fakeFlash.begin());
    WifiStation wifi;
    wifi.begin();
    FanController fan;
    fan.setDutyPercent(35);
    WebPanel panel;
    ClimateReading climate;
    panel.begin(fan, wifi, localControls, climate);
    auto &server = *WebServer::instance;
    server.request("/", HTTP_GET);
    assert(server.status == 200 && server.socket.body.find("Connect Fanmaster") != std::string::npos);
    assert(server.contentLength == server.socket.body.size());
    assert(serviceCount > 0);
    server.request("/generate_204", HTTP_GET);
    assert(server.status == 302 && server.headers["Location"] == "http://192.168.4.1/setup");
    server.request("/api/missing", HTTP_GET);
    assert(server.status == 404 && server.contentType.find("application/json") == 0);
    server.request("/api/fan/duty", HTTP_POST, {{"duty", "0.5"}});
    assert(server.status == 400 && fan.dutyPercent() == 35);
    server.request("/api/fan/duty", HTTP_POST, {{"duty", "100"}});
    assert(server.status == 200 && fan.dutyPercent() == 100);
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"duty_percent\":100") != std::string::npos);
    assert(server.socket.body.find("\"poll_interval_ms\":1000") != std::string::npos);
    assert(server.socket.body.find("\"temperature_c\":null") != std::string::npos);
    climate.publish({true, 24.6f, 52.1f});
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"temperature_c\":24.6") != std::string::npos);
    assert(server.socket.body.find("\"humidity_percent\":52.1") != std::string::npos);
    climate.publish({false, 24.6f, 52.1f});
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"temperature_c\":null") != std::string::npos);
    assert(server.socket.body.find("\"humidity_percent\":null") != std::string::npos);
    server.request("/api/wifi/configure", HTTP_POST, {{"ssid", "home"}, {"password", "password"}}, false);
    assert(server.status == 403 && !wifi.isProvisioning());
    server.request("/api/wifi/configure", HTTP_POST, {{"ssid", "home"}, {"password", "short"}});
    assert(server.status == 400);
    server.request("/api/wifi/configure", HTTP_POST, {{"ssid", "home"}, {"password", "password"}});
    assert(server.status == 202 && Preferences::data.empty());
    assert(WiFi.requestedSsid.length() == 0); // HTTP response precedes connection attempt.
    server.request("/api/wifi/configure", HTTP_POST, {{"ssid", "other"}, {"password", "password"}});
    assert(server.status == 409);
    wifi.update();
    WiFi.connect();
    wifi.update();
    server.request("/api/wifi/status", HTTP_GET);
    assert(server.socket.body.find("\"state\":\"saved\"") != std::string::npos);
    assert(server.socket.body.find("password") == std::string::npos);
    fakeMillis += Config::Wifi::SETUP_CLOSE_GRACE_MS;
    wifi.update();
    server.request("/setup", HTTP_GET);
    assert(server.status == 403);
    server.request("/api/wifi/configure", HTTP_POST, {{"ssid", "other"}, {"password", "password"}});
    assert(server.status == 403);
    server.request("/", HTTP_GET);
    assert(server.status == 200 && server.socket.body.find("Fanmaster Control Deck") != std::string::npos);
    assert(server.contentLength == server.socket.body.size());

    Preferences::data.clear();
    WiFi = FakeWiFi{};
    WifiStation recoveryWifi;
    recoveryWifi.begin();
    // Keep setup.html healthy but corrupt its JS dependency: use internal recovery form.
    AssetStore store;
    assert(store.begin());
    AssetFormat::Entry entry;
    assert(store.find("/assets/setup.js", entry));
    fakeFlash[entry.offset] ^= 1;
    WebPanel recovery;
    recovery.begin(fan, recoveryWifi, localControls, climate);
    WebServer::instance->request("/", HTTP_GET);
    assert(WebServer::instance->status == 200);
    assert(WebServer::instance->socket.body.find("<form method=post") != std::string::npos);
    std::fill(fakeFlash.begin(), fakeFlash.end(), 0xff);
    WebPanel missingFlash;
    missingFlash.begin(fan, recoveryWifi, localControls, climate);
    WebServer::instance->request("/", HTTP_GET);
    assert(WebServer::instance->status == 200);
    assert(WebServer::instance->socket.body.find("<form method=post") != std::string::npos);
    std::cout << "HTTP routes, static streaming and recovery form tests passed\n";
}
