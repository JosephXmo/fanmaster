#include "StartFan.h"
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
    assert(fan.begin());
    WebPanel panel;
    ClimateReading climate;
    panel.begin(fan, wifi, localControls, climate);
    auto &server = *WebServer::instance;
    server.request("/api/status", HTTP_GET);
    assert(server.status == 200 && server.socket.body.find("\"power_enabled\":false") != std::string::npos);
    assert(server.socket.body.find("\"power_waiting_for_key\":true") != std::string::npos);
    for (const char *invalid : {"", "2", "-1", "true", "0.5", "4294967297"}) {
        server.request("/api/fan/power", HTTP_POST, {{"enabled", invalid}});
        assert(server.status == 400 && !fan.snapshot().powerEnabled);
    }
    server.request("/api/fan/power", HTTP_POST);
    assert(server.status == 400);
    const auto beforePower = millis();
    server.request("/api/fan/power", HTTP_POST, {{"enabled", "1"}});
    assert(server.status == 200 && millis() == beforePower && fan.snapshot().powerStarting);
    assert(server.socket.body.find("\"power_enabled\":true") != std::string::npos);
    assert(server.socket.body.find("\"power_starting\":true") != std::string::npos);
    fakeMillis += Config::Fan::STARTUP_TIME_MS; fan.updatePower();
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
    assert(server.socket.body.find("\"mode\":\"duty\"") != std::string::npos);
    server.request("/api/fan/rpm", HTTP_POST, {{"rpm", "850"}});
    assert(server.status == 200 && fan.snapshot().mode == FanMode::Rpm);
    for (const char *invalid : {"0", "549", "1701", "850.5", "-850", "", "4294968146"}) {
        server.request("/api/fan/rpm", HTTP_POST, {{"rpm", invalid}});
        assert(server.status == 400 && fan.snapshot().targetRpm == 850);
    }
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"target_rpm\":850") != std::string::npos);
    assert(server.socket.body.find("\"manual_duty_percent\":100") != std::string::npos);
    server.request("/api/fan/mode", HTTP_POST, {{"mode", "invalid"}});
    assert(server.status == 400 && fan.snapshot().mode == FanMode::Rpm);
    server.request("/api/fan/mode", HTTP_POST, {{"mode", "duty"}});
    assert(server.status == 200 && fan.dutyPercent() == 100);
    server.request("/api/fan/mode", HTTP_POST, {{"mode", "rpm"}});
    assert(server.status == 200 && fan.snapshot().mode == FanMode::Rpm);
    server.request("/api/fan/duty", HTTP_POST, {{"duty", "100"}});
    assert(server.status == 200 && fan.snapshot().mode == FanMode::Duty);
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"temperature_c\":null") != std::string::npos);
    climate.publish({true, 24.6f, 52.1f});
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"temperature_c\":24.6") != std::string::npos);
    assert(server.socket.body.find("\"humidity_percent\":52.1") != std::string::npos);
    climate.publish({false, 24.6f, 52.1f});
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"temperature_c\":null") != std::string::npos);
    assert(server.socket.body.find("\"humidity_percent\":null") != std::string::npos);
    server.request("/api/thermal/mode", HTTP_POST, {{"enabled", "1"}});
    assert(server.status == 409 && fan.snapshot().mode == FanMode::Duty);
    fan.updateTemperature({true, 32.5f, 50, millis()});
    server.request("/api/thermal/mode", HTTP_POST, {{"enabled", "1"}});
    assert(server.status == 200 && fan.dutyPercent() == 50);
    assert(server.socket.body.find("\"mode\":\"thermal\"") != std::string::npos);
    assert(server.socket.body.find("\"thermal_points\":[") != std::string::npos);
    assert(server.socket.body.back() == '}'); // Full status fits response buffer.
    server.request("/api/fan/duty", HTTP_POST, {{"duty", "20"}}); assert(server.status == 409);
    server.request("/api/fan/rpm", HTTP_POST, {{"rpm", "900"}}); assert(server.status == 409);
    server.request("/api/fan/mode", HTTP_POST, {{"mode", "duty"}}); assert(server.status == 409);
    const std::map<std::string, String> curve = {{"t1", "25.1"}, {"d1", "35"},
        {"t2", "40.2"}, {"d2", "65"}, {"t3", "80.3"}, {"d3", "90"}};
    for (const char *invalid : {"", "14.9", "95.1", "25.11", "25.", ".5", "-25", "1e2", "4294967321", "40.2"}) {
        auto args = curve; args["t1"] = invalid;
        server.request("/api/thermal/curve", HTTP_POST, args);
        assert(server.status == 400 && fan.snapshot().thermalCurve == ThermalCurve{});
    }
    for (const char *invalid : {"34", "65.1", "101", "-1", "4294967361"}) {
        auto args = curve; args["d2"] = invalid;
        server.request("/api/thermal/curve", HTTP_POST, args);
        assert(server.status == 400 && fan.snapshot().thermalCurve == ThermalCurve{});
    }
    auto missing = curve; missing.erase("t2");
    server.request("/api/thermal/curve", HTTP_POST, missing); assert(server.status == 400);
    server.request("/api/thermal/curve", HTTP_POST, curve);
    assert(server.status == 200 && fan.snapshot().thermalCurve.points[0].temperatureDeciC == 251);
    server.request("/api/thermal/mode", HTTP_POST, {{"enabled", "0"}});
    assert(server.status == 200 && fan.snapshot().mode == FanMode::Duty && fan.dutyPercent() == 100);
    server.request("/api/thermal/mode", HTTP_POST, {{"enabled", "2"}}); assert(server.status == 400);
    server.request("/api/thermal/mode", HTTP_POST); assert(server.status == 400);
    fan.setThermalEnabled(true);
    fan.toggleMode(); // Local exit is reflected in the next web poll.
    server.request("/api/status", HTTP_GET);
    assert(server.socket.body.find("\"mode\":\"duty\"") != std::string::npos);
    assert(server.socket.body.find("\"thermal_exit_reason\":\"manual\"") != std::string::npos);
    server.request("/api/fan/power", HTTP_POST, {{"enabled", "0"}});
    assert(server.status == 200 && !fan.snapshot().powerEnabled && fan.dutyPercent() == 0);
    const auto offRevision = fan.snapshot().controlRevision;
    server.request("/api/fan/power", HTTP_POST, {{"enabled", "0"}});
    assert(server.status == 200 && fan.snapshot().controlRevision == offRevision);
    server.request("/api/fan/duty", HTTP_POST, {{"duty", "20"}}); assert(server.status == 409);
    server.request("/api/fan/rpm", HTTP_POST, {{"rpm", "900"}}); assert(server.status == 409);
    server.request("/api/fan/mode", HTTP_POST, {{"mode", "rpm"}}); assert(server.status == 409);
    server.request("/api/thermal/mode", HTTP_POST, {{"enabled", "1"}}); assert(server.status == 409);
    server.request("/api/thermal/curve", HTTP_POST, curve);
    assert(server.status == 200 && fan.dutyPercent() == 0); // Editable without enabling relay.
    fan.setPowerEnabled(true); // Local restore is visible to the same status endpoint.
    fakeMillis += Config::Fan::STARTUP_TIME_MS; fan.updatePower();
    server.request("/api/status", HTTP_GET);
    assert(server.status == 200 && server.socket.body.find("\"power_enabled\":true") != std::string::npos);
    assert(fan.dutyPercent() == 100 && server.socket.body.back() == '}');
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
