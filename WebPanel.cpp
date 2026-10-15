#include "WebPanel.h"

#include <stdio.h>

#include "Config.h"
#include "FanController.h"
#include "WifiStation.h"

static_assert(Config::Storage::HTTP_CHUNK_BYTES > 0 &&
              Config::Storage::HTTP_CHUNK_BYTES <= 4096,
              "HTTP chunk buffer must be between 1 and 4096 bytes");

WebPanel::WebPanel()
    : server_(Config::Web::HTTP_PORT)
{
}

void WebPanel::begin(FanController &fan, WifiStation &wifiStation,
                     void (*serviceLocalControls)(), const ClimateReading &climate)
{
    fan_ = &fan;
    wifiStation_ = &wifiStation;
    climate_ = &climate;
    serviceLocalControls_ = serviceLocalControls;
    if (!assets_.begin()) Serial.println("[WEB] external assets unavailable; setup fallback enabled");

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/fan/duty", HTTP_POST, [this]() { handleSetDuty(); });
    server_.on("/api/fan/rpm", HTTP_POST, [this]() { handleSetRpm(); });
    server_.on("/api/fan/mode", HTTP_POST, [this]() { handleSetMode(); });
    server_.on("/api/thermal/mode", HTTP_POST, [this]() { handleThermalMode(); });
    server_.on("/api/thermal/curve", HTTP_POST, [this]() { handleThermalCurve(); });
    server_.on("/setup", HTTP_GET, [this]() { handleSetup(); });
    server_.on("/api/wifi/configure", HTTP_POST, [this]() { handleProvision(); });
    server_.on("/api/wifi/status", HTTP_GET, [this]() { handleWifiStatus(); });
    server_.onNotFound([this]() { handleNotFound(); });
}

void WebPanel::update()
{
    const uint32_t now = millis();
    if (lastUpdateMs_ != 0) {
        const uint32_t updateGapMs = now - lastUpdateMs_;
        if (updateGapMs > maxUpdateGapMs_) {
            maxUpdateGapMs_ = updateGapMs;
        }
    }
    lastUpdateMs_ = now;

    if (!started_ && (wifiStation_->isConnected() || wifiStation_->isSetupActive())) {
        server_.begin();
        started_ = true;
        lastDiagnosticMs_ = now;

        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.print("[WEB] listening at http://");
            Serial.print(wifiStation_->isSetupActive() ? wifiStation_->setupIp() : wifiStation_->localIp());
            Serial.print(':');
            Serial.print(Config::Web::HTTP_PORT);
            Serial.print("/ free_heap=");
            Serial.println(ESP.getFreeHeap());
        }
    }

    if (started_) {
        server_.handleClient();
        reportDiagnostics(now);
    }
}

void WebPanel::recordRequest(const char *label, bool verbose)
{
    requestCount_++;
    if (!verbose || Config::SerialConsole::PLOTTER_MODE) {
        return;
    }

    Serial.print("[WEB] request ");
    Serial.println(label);
}

void WebPanel::reportDiagnostics(uint32_t now)
{
    if (Config::SerialConsole::PLOTTER_MODE ||
        now - lastDiagnosticMs_ < Config::Diagnostics::REPORT_INTERVAL_MS) {
        return;
    }

    lastDiagnosticMs_ = now;
    Serial.print("[WEB] alive ip=");
    Serial.print(wifiStation_->localIp());
    Serial.print(" requests=");
    Serial.print(requestCount_);
    Serial.print(" max_loop_gap_ms=");
    Serial.print(maxUpdateGapMs_);
    Serial.print(" free_heap=");
    Serial.println(ESP.getFreeHeap());
    maxUpdateGapMs_ = 0;
}

void WebPanel::handleRoot()
{
    recordRequest("GET /", true);
    if (wifiStation_->isSetupActive()) {
        handleSetup();
    } else if (!serveAsset("/index.html")) {
        server_.send(503, "text/plain", "Web assets unavailable. Use /setup or upload a resource image.");
    }
}

bool WebPanel::serveAsset(const char *path)
{
    AssetFormat::Entry entry;
    if (!assets_.find(path, entry, serviceLocalControls_)) return false;
    auto &client = server_.client();
    client.setTimeout(Config::Storage::HTTP_WRITE_TIMEOUT_MS);
    server_.sendHeader("Cache-Control", "no-store");
    server_.setContentLength(entry.length);
    server_.send(200, entry.contentType, "");
    uint8_t buffer[Config::Storage::HTTP_CHUNK_BYTES];
    for (uint32_t offset = 0; offset < entry.length;) {
        const size_t length = min(sizeof(buffer), static_cast<size_t>(entry.length - offset));
        if (!assets_.read(entry, offset, buffer, length)) {
            client.stop();
            return true;
        }
        // A short/failed socket write terminates this response, never loop forever.
        if (client.write(buffer, length) != length) {
            client.stop();
            return true;
        }
        offset += length;
        if (serviceLocalControls_) serviceLocalControls_();
        yield();
    }
    return true;
}

void WebPanel::handleSetup()
{
    if (!wifiStation_->isSetupActive()) {
        server_.send(403, "text/plain", "Setup is available only while the setup AP is active.");
        return;
    }
    AssetFormat::Entry dependency;
    if (assets_.find("/assets/setup.css", dependency, serviceLocalControls_) &&
        assets_.find("/assets/setup.js", dependency, serviceLocalControls_) &&
        serveAsset("/setup.html")) return;
    // Deliberately tiny recovery form, independent of external flash and JavaScript.
    static const char FALLBACK[] PROGMEM =
        "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Fanmaster setup</title><h1>Fanmaster Wi-Fi</h1>"
        "<form method=post action=/api/wifi/configure>"
        "<p><label>SSID <input name=ssid maxlength=32 required></label></p>"
        "<p><label>Password <input name=password type=password maxlength=64></label></p>"
        "<button>Connect and save</button></form>"
        "<p>Save occurs after connection succeeds. The setup network then closes."
        " Check <a href=/api/wifi/status>connection status</a>.</p>";
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html; charset=utf-8", FALLBACK);
}

void WebPanel::handleProvision()
{
    if (!wifiStation_->isSetupActive()) {
        sendJson(403, "{\"error\":\"setup is inactive\"}");
        return;
    }
    // Only accept a form from this device's setup AP address. No CORS is enabled.
    if (server_.client().localIP() != wifiStation_->setupIp()) {
        sendJson(403, "{\"error\":\"connect to the setup network\"}");
        return;
    }
    if (wifiStation_->isProvisioning()) {
        sendJson(409, "{\"error\":\"connection attempt in progress; retry shortly\"}");
        return;
    }
    if (!server_.hasArg("ssid") || !server_.hasArg("password") ||
        !wifiStation_->provision(server_.arg("ssid"), server_.arg("password"))) {
        sendJson(400, "{\"error\":\"invalid SSID or password length/format\"}");
        return;
    }
    sendJson(202, "{\"state\":\"connecting\"}");
}

void WebPanel::handleWifiStatus()
{
    if (!wifiStation_->isSetupActive()) {
        sendJson(403, "{\"error\":\"setup is inactive\"}");
        return;
    }
    char response[224];
    snprintf(response, sizeof(response),
             "{\"state\":\"%s\",\"connected\":%s,\"ip\":\"%s\"}",
             wifiStation_->provisionState(), wifiStation_->isConnected() ? "true" : "false",
             wifiStation_->localIp().toString().c_str());
    sendJson(200, response);
}

void WebPanel::handleStatus()
{
    recordRequest("GET /api/status", false);
    sendStatus();
}

void WebPanel::sendStatus()
{
    const FanSnapshot state = fan_->snapshot();
    const uint32_t roundedRpm = state.rpm <= 0.0f
        ? 0
        : static_cast<uint32_t>(state.rpm + 0.5f);

    char temperature[24] = "null";
    char humidity[24] = "null";
    const ClimateSample sample = climate_->snapshot();
    if (sample.valid && std::isfinite(sample.temperatureC) && std::isfinite(sample.humidityPercent)) {
        snprintf(temperature, sizeof(temperature), "%.1f", sample.temperatureC);
        snprintf(humidity, sizeof(humidity), "%.1f", sample.humidityPercent);
    }
    char response[768];
    snprintf(
        response,
        sizeof(response),
        "{\"rpm\":%lu,\"duty_percent\":%u,\"wifi_connected\":%s,"
        "\"temperature_c\":%s,\"humidity_percent\":%s,\"poll_interval_ms\":%lu,"
        "\"mode\":\"%s\",\"target_rpm\":%lu,\"manual_duty_percent\":%u,"
        "\"min_rpm\":%lu,\"max_rpm\":%lu,\"control_revision\":%lu,"
        "\"fixed_mode\":\"%s\",\"thermal_sensor_ready\":%s,\"thermal_exit_reason\":\"%s\","
        "\"thermal_points\":[{\"temperature_c\":%.1f,\"duty_percent\":%u},"
        "{\"temperature_c\":%.1f,\"duty_percent\":%u},{\"temperature_c\":%.1f,\"duty_percent\":%u}]}",
        static_cast<unsigned long>(roundedRpm),
        state.appliedDutyPercent,
        wifiStation_->isConnected() ? "true" : "false",
        temperature,
        humidity,
        static_cast<unsigned long>(Config::Web::POLL_INTERVAL_MS),
        state.mode == FanMode::Thermal ? "thermal" : state.mode == FanMode::Rpm ? "rpm" : "duty",
        static_cast<unsigned long>(state.targetRpm), state.manualDutyPercent,
        static_cast<unsigned long>(Config::RpmControl::MIN_RPM),
        static_cast<unsigned long>(Config::RpmControl::MAX_RPM),
        static_cast<unsigned long>(state.controlRevision),
        state.fixedMode == FanMode::Rpm ? "rpm" : "duty",
        state.thermalSensorReady ? "true" : "false",
        state.thermalExitReason == ThermalExitReason::Sensor ? "sensor" :
            state.thermalExitReason == ThermalExitReason::Manual ? "manual" : "none",
        state.thermalCurve.points[0].temperatureDeciC / 10.0, state.thermalCurve.points[0].dutyPercent,
        state.thermalCurve.points[1].temperatureDeciC / 10.0, state.thermalCurve.points[1].dutyPercent,
        state.thermalCurve.points[2].temperatureDeciC / 10.0, state.thermalCurve.points[2].dutyPercent
    );
    sendJson(200, response);
}

void WebPanel::handleSetDuty()
{
    recordRequest("POST /api/fan/duty", true);
    uint8_t requestedDuty;
    if (!parseDutyArgument(requestedDuty)) {
        sendJson(
            400,
            "{\"error\":\"duty must be a whole number from 0 through 100\"}"
        );
        return;
    }

    if (!fan_->setDutyPercent(requestedDuty)) {
        sendJson(409, "{\"error\":\"Exit THERMAL before changing fixed controls\"}");
        return;
    }
    sendStatus();
}

void WebPanel::handleSetRpm()
{
    recordRequest("POST /api/fan/rpm", true);
    uint32_t rpm;
    if (!parseUnsignedArgument("rpm", Config::RpmControl::MAX_RPM, rpm) || rpm < Config::RpmControl::MIN_RPM) {
        sendJson(400, "{\"error\":\"RPM must be a whole number within min_rpm and max_rpm\"}");
        return;
    }
    if (!fan_->setTargetRpm(rpm)) {
        sendJson(409, "{\"error\":\"Exit THERMAL before changing fixed controls\"}");
        return;
    }
    sendStatus();
}

void WebPanel::handleSetMode()
{
    recordRequest("POST /api/fan/mode", true);
    if (!server_.hasArg("mode")) {
        sendJson(400, "{\"error\":\"mode must be duty or rpm\"}");
        return;
    }
    const String mode = server_.arg("mode");
    if (!(mode == "duty") && !(mode == "rpm")) {
        sendJson(400, "{\"error\":\"mode must be duty or rpm\"}");
        return;
    }
    if (!fan_->setMode(mode == "rpm" ? FanMode::Rpm : FanMode::Duty)) {
        sendJson(409, "{\"error\":\"Exit THERMAL before changing fixed controls\"}");
        return;
    }
    sendStatus();
}

void WebPanel::handleThermalMode()
{
    recordRequest("POST /api/thermal/mode", true);
    uint32_t enabled;
    if (!parseUnsignedArgument("enabled", 1, enabled)) {
        sendJson(400, "{\"error\":\"enabled must be 0 or 1\"}");
        return;
    }
    if (!fan_->setThermalEnabled(enabled != 0)) {
        sendJson(409, "{\"error\":\"A fresh temperature reading is required to enable THERMAL\"}");
        return;
    }
    sendStatus();
}

bool WebPanel::parseTemperature(const char *name, uint16_t &result) const
{
    if (!server_.hasArg(name)) return false;
    const String input = server_.arg(name);
    if (!input.length()) return false;
    uint32_t whole = 0, fraction = 0;
    bool decimal = false;
    for (size_t i = 0; i < input.length(); ++i) {
        const char c = input.charAt(i);
        if (c == '.') {
            if (decimal || i == 0 || i + 2 != input.length()) return false;
            decimal = true;
        } else {
            if (c < '0' || c > '9') return false;
            if (decimal) fraction = c - '0';
            else {
                whole = whole * 10 + c - '0';
                if (whole > 95) return false; // Bound before any possible overflow.
            }
        }
    }
    const uint32_t value = whole * 10 + fraction;
    if (value < Config::Thermal::MIN_TEMPERATURE_DECI_C ||
        value > Config::Thermal::MAX_TEMPERATURE_DECI_C) return false;
    result = static_cast<uint16_t>(value);
    return true;
}

void WebPanel::handleThermalCurve()
{
    recordRequest("POST /api/thermal/curve", true);
    ThermalCurve curve;
    for (size_t i = 0; i < 3; ++i) {
        char temperatureName[] = "t1", dutyName[] = "d1";
        temperatureName[1] += i;
        dutyName[1] += i;
        uint32_t duty;
        if (!parseTemperature(temperatureName, curve.points[i].temperatureDeciC) ||
            !parseUnsignedArgument(dutyName, 100, duty)) {
            sendJson(400, "{\"error\":\"Use temperatures 15.0-95.0 C with at most one decimal and whole duty 0-100\"}");
            return;
        }
        curve.points[i].dutyPercent = static_cast<uint8_t>(duty);
    }
    if (!fan_->setThermalCurve(curve)) {
        sendJson(400, "{\"error\":\"Temperatures must strictly increase; duty must not decrease\"}");
        return;
    }
    sendStatus();
}

void WebPanel::handleNotFound()
{
    recordRequest("not found", true);
    if (server_.uri().startsWith("/api/")) {
        sendJson(404, "{\"error\":\"API endpoint not found\"}");
        return;
    }

    if (server_.method() == HTTP_GET && serveAsset(server_.uri().c_str())) return;
    if (server_.method() == HTTP_GET && wifiStation_->isSetupActive()) {
        // Handles Android, Apple and Windows connectivity probes and arbitrary hosts.
        server_.sendHeader("Location", String("http://") +
                           wifiStation_->setupIp().toString() + "/setup", true);
        server_.send(302, "text/plain", "Open Fanmaster setup");
        return;
    }
    server_.send(404, "text/plain; charset=utf-8", "Not found\n");
}

void WebPanel::sendJson(int statusCode, const char *body)
{
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(statusCode, "application/json; charset=utf-8", body);
}

bool WebPanel::parseDutyArgument(uint8_t &dutyResult) const
{
    uint32_t value;
    if (!parseUnsignedArgument("duty", 100, value)) return false;
    dutyResult = static_cast<uint8_t>(value);
    return true;
}

bool WebPanel::parseUnsignedArgument(const char *name, uint32_t maximum, uint32_t &result) const
{
    if (!server_.hasArg(name)) {
        return false;
    }

    const String input = server_.arg(name);
    if (input.length() == 0) {
        return false;
    }

    uint32_t parsedValue = 0;
    for (size_t index = 0; index < input.length(); index++) {
        const char character = input.charAt(index);
        if (character < '0' || character > '9') {
            return false;
        }

        const uint32_t digit = character - '0';
        if (parsedValue > maximum / 10 ||
            (parsedValue == maximum / 10 && digit > maximum % 10)) {
            return false;
        }
        parsedValue = parsedValue * 10 + digit;
    }

    result = parsedValue;
    return true;
}
