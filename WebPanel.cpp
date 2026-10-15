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
                     void (*serviceLocalControls)())
{
    fan_ = &fan;
    wifiStation_ = &wifiStation;
    serviceLocalControls_ = serviceLocalControls;
    if (!assets_.begin()) Serial.println("[WEB] external assets unavailable; setup fallback enabled");

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/fan/duty", HTTP_POST, [this]() { handleSetDuty(); });
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
    const uint32_t roundedRpm = fan_->currentRpm() <= 0.0f
        ? 0
        : static_cast<uint32_t>(fan_->currentRpm() + 0.5f);

    char response[192];
    snprintf(
        response,
        sizeof(response),
        "{\"rpm\":%lu,\"duty_percent\":%u,\"wifi_connected\":%s,"
        "\"temperature_c\":null,\"humidity_percent\":null,\"poll_interval_ms\":%lu}",
        static_cast<unsigned long>(roundedRpm),
        fan_->dutyPercent(),
        wifiStation_->isConnected() ? "true" : "false",
        static_cast<unsigned long>(Config::Web::POLL_INTERVAL_MS)
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

    fan_->setDutyPercent(requestedDuty);

    char response[48];
    snprintf(
        response,
        sizeof(response),
        "{\"duty_percent\":%u}",
        fan_->dutyPercent()
    );
    sendJson(200, response);
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
    if (!server_.hasArg("duty")) {
        return false;
    }

    const String input = server_.arg("duty");
    if (input.length() == 0) {
        return false;
    }

    uint16_t parsedValue = 0;
    for (size_t index = 0; index < input.length(); index++) {
        const char character = input.charAt(index);
        if (character < '0' || character > '9') {
            return false;
        }

        parsedValue = parsedValue * 10 + (character - '0');
        if (parsedValue > 100) {
            return false;
        }
    }

    dutyResult = static_cast<uint8_t>(parsedValue);
    return true;
}
