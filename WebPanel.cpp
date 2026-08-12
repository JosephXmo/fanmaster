#include "WebPanel.h"

#include <stdio.h>

#include "Config.h"
#include "FanController.h"
#include "Sht30Sensor.h"
#include "WifiStation.h"

namespace {

const char WEB_PANEL_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="theme-color" content="#173a3a">
  <title>Fanmaster Control Deck</title>
  <style>
    :root {
      --ink: #173a3a;
      --ink-soft: #42605d;
      --paper: #f2eddf;
      --paper-deep: #ded6c2;
      --signal: #e35f32;
      --signal-dark: #ad3c1d;
      --mint: #87c8ad;
      --line: rgba(23, 58, 58, .2);
      --shadow: 0 20px 50px rgba(23, 58, 58, .14);
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      color: var(--ink);
      font-family: "Avenir Next", "Century Gothic", sans-serif;
      background:
        radial-gradient(circle at 78% 12%, rgba(227, 95, 50, .2), transparent 26rem),
        linear-gradient(rgba(23, 58, 58, .055) 1px, transparent 1px),
        linear-gradient(90deg, rgba(23, 58, 58, .055) 1px, transparent 1px),
        var(--paper);
      background-size: auto, 28px 28px, 28px 28px, auto;
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      background: linear-gradient(112deg, transparent 55%, rgba(255, 255, 255, .32));
    }

    .shell {
      width: min(1120px, calc(100% - 32px));
      margin: 0 auto;
      padding: 28px 0 42px;
    }

    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 20px;
      padding: 4px 2px 26px;
      animation: enter .55s ease-out both;
    }

    .brand { display: flex; align-items: center; gap: 14px; }

    .brand-mark {
      width: 46px;
      aspect-ratio: 1;
      display: grid;
      place-items: center;
      border: 2px solid var(--ink);
      border-radius: 50%;
      box-shadow: 5px 5px 0 var(--signal);
      font-family: Georgia, serif;
      font-weight: 700;
      font-size: 20px;
    }

    .eyebrow {
      margin: 0 0 2px;
      color: var(--signal-dark);
      font-size: 11px;
      font-weight: 800;
      letter-spacing: .19em;
      text-transform: uppercase;
    }

    h1, h2, p { margin-top: 0; }

    h1 {
      margin-bottom: 0;
      font-family: Georgia, serif;
      font-size: clamp(25px, 4vw, 38px);
      font-weight: 500;
      letter-spacing: -.035em;
    }

    .connection {
      display: inline-flex;
      align-items: center;
      gap: 9px;
      padding: 9px 13px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: rgba(255, 255, 255, .42);
      font-size: 12px;
      font-weight: 800;
      letter-spacing: .08em;
      text-transform: uppercase;
    }

    .connection-dot {
      width: 9px;
      height: 9px;
      border-radius: 50%;
      background: #a45a48;
      box-shadow: 0 0 0 4px rgba(164, 90, 72, .13);
    }

    .connection.online .connection-dot {
      background: #21875f;
      box-shadow: 0 0 0 4px rgba(33, 135, 95, .14);
      animation: breathe 2.2s ease-in-out infinite;
    }

    .grid {
      display: grid;
      grid-template-columns: minmax(0, 1.3fr) minmax(300px, .7fr);
      gap: 20px;
    }

    .card {
      position: relative;
      overflow: hidden;
      border: 1px solid var(--line);
      border-radius: 24px;
      background: rgba(255, 252, 243, .79);
      box-shadow: var(--shadow);
      backdrop-filter: blur(8px);
      animation: enter .55s ease-out both;
    }

    .telemetry { min-height: 420px; padding: clamp(25px, 5vw, 48px); }
    .control { padding: 30px; animation-delay: .08s; }

    .telemetry::after {
      content: "";
      position: absolute;
      width: 280px;
      height: 280px;
      right: -105px;
      bottom: -120px;
      border: 34px solid rgba(135, 200, 173, .2);
      border-radius: 50%;
    }

    .section-label {
      color: var(--ink-soft);
      font-size: 12px;
      font-weight: 800;
      letter-spacing: .16em;
      text-transform: uppercase;
    }

    .rpm-stage {
      display: flex;
      align-items: center;
      gap: clamp(22px, 5vw, 54px);
      margin: 48px 0 44px;
    }

    .rotor {
      flex: 0 0 auto;
      width: clamp(116px, 19vw, 172px);
      aspect-ratio: 1;
      position: relative;
      border: 3px solid var(--ink);
      border-radius: 50%;
      background: conic-gradient(
        from 25deg,
        var(--signal) 0 15%, transparent 15% 25%,
        var(--signal) 25% 40%, transparent 40% 50%,
        var(--signal) 50% 65%, transparent 65% 75%,
        var(--signal) 75% 90%, transparent 90%
      );
      box-shadow: inset 0 0 0 14px var(--paper), 7px 7px 0 var(--ink);
    }

    .rotor::after {
      content: "";
      position: absolute;
      width: 28%;
      aspect-ratio: 1;
      inset: 36%;
      border: 3px solid var(--ink);
      border-radius: 50%;
      background: var(--paper);
    }

    .rotor.spinning { animation: spin 1.2s linear infinite; }

    .rpm-value {
      display: block;
      font-family: Georgia, serif;
      font-size: clamp(62px, 12vw, 128px);
      font-variant-numeric: tabular-nums lining-nums;
      letter-spacing: -.075em;
      line-height: .82;
    }

    .rpm-unit {
      display: block;
      margin-top: 15px;
      color: var(--signal-dark);
      font-size: 13px;
      font-weight: 900;
      letter-spacing: .22em;
    }

    .sensor-strip {
      position: relative;
      z-index: 1;
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      border-top: 1px solid var(--line);
    }

    .sensor-item { padding: 20px 18px 0 0; }
    .sensor-item + .sensor-item { padding-left: 20px; border-left: 1px solid var(--line); }

    .sensor-name {
      display: block;
      margin-bottom: 8px;
      color: var(--ink-soft);
      font-size: 10px;
      font-weight: 800;
      letter-spacing: .13em;
      text-transform: uppercase;
    }

    .sensor-value {
      font-family: Georgia, serif;
      font-size: clamp(23px, 4vw, 34px);
      font-variant-numeric: tabular-nums;
    }

    .control h2 {
      margin: 15px 0 8px;
      font-family: Georgia, serif;
      font-size: 34px;
      font-weight: 500;
      letter-spacing: -.035em;
    }

    .control-copy { color: var(--ink-soft); line-height: 1.55; }

    form { margin-top: 34px; }

    label {
      display: block;
      margin-bottom: 9px;
      font-size: 12px;
      font-weight: 900;
      letter-spacing: .11em;
      text-transform: uppercase;
    }

    .input-wrap { position: relative; }

    input[type="number"] {
      width: 100%;
      padding: 18px 64px 18px 18px;
      border: 2px solid var(--ink);
      border-radius: 13px;
      color: var(--ink);
      background: rgba(255, 255, 255, .63);
      font: 700 29px/1 Georgia, serif;
      font-variant-numeric: tabular-nums;
      outline: none;
      transition: box-shadow .18s ease, transform .18s ease;
    }

    input[type="number"]:focus {
      box-shadow: 5px 5px 0 var(--mint);
      transform: translate(-2px, -2px);
    }

    .percent {
      position: absolute;
      top: 50%;
      right: 18px;
      transform: translateY(-50%);
      color: var(--ink-soft);
      font-size: 19px;
      font-weight: 900;
    }

    button {
      width: 100%;
      margin-top: 13px;
      padding: 17px 20px;
      border: 2px solid var(--ink);
      border-radius: 13px;
      color: #fffaf0;
      background: var(--signal);
      box-shadow: 5px 5px 0 var(--ink);
      font: 900 13px/1 "Avenir Next", "Century Gothic", sans-serif;
      letter-spacing: .12em;
      text-transform: uppercase;
      cursor: pointer;
      transition: transform .15s ease, box-shadow .15s ease, background .15s ease;
    }

    button:hover { background: var(--signal-dark); transform: translate(-1px, -1px); }
    button:active { box-shadow: 1px 1px 0 var(--ink); transform: translate(4px, 4px); }
    button:disabled { cursor: wait; opacity: .62; }

    .form-message {
      min-height: 42px;
      margin: 18px 0 0;
      padding: 11px 13px;
      border-left: 4px solid var(--mint);
      color: var(--ink-soft);
      background: rgba(135, 200, 173, .12);
      font-size: 13px;
      line-height: 1.45;
    }

    .form-message.error { border-color: var(--signal); background: rgba(227, 95, 50, .1); }

    footer {
      display: flex;
      justify-content: space-between;
      gap: 20px;
      padding: 22px 4px 0;
      color: var(--ink-soft);
      font-size: 11px;
      letter-spacing: .06em;
      animation: enter .55s .16s ease-out both;
    }

    code { color: var(--signal-dark); font-family: Consolas, monospace; }

    @keyframes enter {
      from { opacity: 0; transform: translateY(14px); }
      to { opacity: 1; transform: translateY(0); }
    }

    @keyframes spin { to { transform: rotate(360deg); } }

    @keyframes breathe {
      50% { box-shadow: 0 0 0 7px rgba(33, 135, 95, .08); }
    }

    @media (max-width: 760px) {
      .shell { width: min(100% - 20px, 600px); padding-top: 16px; }
      header { align-items: flex-start; }
      .brand-mark { display: none; }
      .connection { margin-top: 3px; font-size: 10px; }
      .grid { grid-template-columns: 1fr; }
      .telemetry { min-height: 0; padding: 26px 22px; }
      .rpm-stage { margin: 34px 0; }
      .rotor { width: 105px; }
      .sensor-strip { grid-template-columns: 1fr 1fr; }
      .sensor-item { padding-top: 17px; }
      .sensor-item:last-child { grid-column: 1 / -1; padding-left: 0; border-left: 0; }
      footer { display: block; line-height: 1.7; }
    }

    @media (max-width: 430px) {
      .rpm-stage { gap: 19px; }
      .rotor { width: 82px; box-shadow: inset 0 0 0 9px var(--paper), 5px 5px 0 var(--ink); }
      .rpm-value { font-size: 58px; }
      .sensor-strip { display: block; }
      .sensor-item, .sensor-item + .sensor-item {
        padding: 13px 0;
        border-left: 0;
        border-bottom: 1px solid var(--line);
      }
    }

    @media (prefers-reduced-motion: reduce) {
      *, *::before, *::after { animation-duration: .01ms !important; animation-iteration-count: 1 !important; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <header>
      <div class="brand">
        <div class="brand-mark" aria-hidden="true">F</div>
        <div>
          <p class="eyebrow">ESP32 / Control Deck</p>
          <h1>Fanmaster</h1>
        </div>
      </div>
      <div id="connection" class="connection" role="status">
        <span class="connection-dot"></span>
        <span id="connectionText">Connecting</span>
      </div>
    </header>

    <main class="grid">
      <section class="card telemetry" aria-labelledby="telemetryTitle">
        <div id="telemetryTitle" class="section-label">Live shaft telemetry</div>
        <div class="rpm-stage">
          <div id="rotor" class="rotor" aria-hidden="true"></div>
          <div>
            <strong id="rpm" class="rpm-value">--</strong>
            <span class="rpm-unit">REVOLUTIONS / MIN</span>
          </div>
        </div>

        <div class="sensor-strip">
          <div class="sensor-item">
            <span class="sensor-name">Applied output</span>
            <strong id="currentDuty" class="sensor-value">--%</strong>
          </div>
          <div class="sensor-item">
            <span class="sensor-name">Temperature</span>
            <strong id="temperature" class="sensor-value">--</strong>
          </div>
          <div class="sensor-item">
            <span class="sensor-name">Humidity</span>
            <strong id="humidity" class="sensor-value">--</strong>
          </div>
        </div>
      </section>

      <aside class="card control" aria-labelledby="controlTitle">
        <div class="section-label">Manual override</div>
        <h2 id="controlTitle">Set fan duty</h2>
        <p class="control-copy">Choose an exact PWM duty cycle. The new value is applied only after submission.</p>

        <form id="dutyForm" novalidate>
          <label for="duty">Duty cycle</label>
          <div class="input-wrap">
            <input id="duty" name="duty" type="number" min="0" max="100" step="1" inputmode="numeric" required>
            <span class="percent">%</span>
          </div>
          <button id="applyButton" type="submit">Apply fan setting</button>
        </form>
        <p id="formMessage" class="form-message" role="status" aria-live="polite">Waiting for controller status.</p>
      </aside>
    </main>

    <footer>
      <span id="lastUpdated">No telemetry received yet</span>
      <span>Entrypoint <code>/api/status</code> &middot; 1 second cadence</span>
    </footer>
  </div>

  <script>
    (() => {
      "use strict";

      const pollIntervalMs = __POLL_INTERVAL_MS__;
      const connection = document.getElementById("connection");
      const connectionText = document.getElementById("connectionText");
      const rpm = document.getElementById("rpm");
      const rotor = document.getElementById("rotor");
      const currentDuty = document.getElementById("currentDuty");
      const temperature = document.getElementById("temperature");
      const humidity = document.getElementById("humidity");
      const lastUpdated = document.getElementById("lastUpdated");
      const form = document.getElementById("dutyForm");
      const dutyInput = document.getElementById("duty");
      const applyButton = document.getElementById("applyButton");
      const formMessage = document.getElementById("formMessage");
      let dutyIsBeingEdited = false;
      let statusReceived = false;
      let statusErrorVisible = false;

      function setConnectionState(online) {
        connection.classList.toggle("online", online);
        connectionText.textContent = online ? "Controller online" : "Controller unavailable";
      }

      function showMessage(message, isError = false) {
        formMessage.textContent = message;
        formMessage.classList.toggle("error", isError);
      }

      function showOptionalReading(element, value, suffix) {
        element.textContent = value === null || value === undefined ? "--" : `${value}${suffix}`;
      }

      async function requestJson(url, options = {}) {
        const response = await fetch(url, { cache: "no-store", ...options });
        let body;

        try {
          body = await response.json();
        } catch (_) {
          throw new Error(`Controller returned HTTP ${response.status}`);
        }

        if (!response.ok) {
          throw new Error(body.error || `Controller returned HTTP ${response.status}`);
        }

        return body;
      }

      async function refreshStatus() {
        try {
          const state = await requestJson("/api/status");
          const roundedRpm = Math.max(0, Math.round(Number(state.rpm) || 0));

          rpm.textContent = String(roundedRpm);
          currentDuty.textContent = `${state.duty_percent}%`;
          showOptionalReading(temperature, state.temperature_c, "\u00b0C");
          showOptionalReading(humidity, state.humidity_percent, "%");

          rotor.classList.toggle("spinning", roundedRpm > 0);
          rotor.style.animationDuration = `${Math.max(.3, Math.min(2.4, 1800 / Math.max(roundedRpm, 1)))}s`;

          if (!dutyIsBeingEdited) {
            dutyInput.value = state.duty_percent;
          }

          setConnectionState(Boolean(state.wifi_connected));
          lastUpdated.textContent = `Telemetry updated ${new Date().toLocaleTimeString()}`;

          if (!statusReceived || statusErrorVisible) {
            showMessage("Controller ready. Choose a duty cycle when needed.");
          }
          statusReceived = true;
          statusErrorVisible = false;
        } catch (error) {
          setConnectionState(false);
          lastUpdated.textContent = "Telemetry request failed";
          showMessage(error.message, true);
          statusErrorVisible = true;
        }
      }

      async function pollStatus() {
        await refreshStatus();
        window.setTimeout(pollStatus, pollIntervalMs);
      }

      dutyInput.addEventListener("input", () => {
        dutyIsBeingEdited = true;
      });

      form.addEventListener("submit", async (event) => {
        event.preventDefault();
        const requestedDuty = Number(dutyInput.value);

        if (!Number.isInteger(requestedDuty) || requestedDuty < 0 || requestedDuty > 100) {
          showMessage("Enter a whole number from 0 through 100.", true);
          dutyInput.focus();
          return;
        }

        applyButton.disabled = true;
        showMessage("Applying new fan setting...");

        try {
          const body = new URLSearchParams({ duty: String(requestedDuty) });
          const result = await requestJson("/api/fan/duty", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
            body,
          });

          dutyIsBeingEdited = false;
          dutyInput.value = result.duty_percent;
          currentDuty.textContent = `${result.duty_percent}%`;
          showMessage(`Fan duty set to ${result.duty_percent}%.`);
          await refreshStatus();
        } catch (error) {
          showMessage(error.message, true);
        } finally {
          applyButton.disabled = false;
        }
      });

      pollStatus();
    })();
  </script>
</body>
</html>
)rawliteral";

}  // namespace

WebPanel::WebPanel()
    : server_(Config::Web::HTTP_PORT)
{
}

void WebPanel::begin(
    FanController &fan,
    WifiStation &wifiStation,
    Sht30Sensor &sht30Sensor,
    bool &hasClimateReading,
    float &temperatureC,
    float &humidityPercent
)
{
    fan_ = &fan;
    wifiStation_ = &wifiStation;
    sht30Sensor_ = &sht30Sensor;
    hasClimateReading_ = &hasClimateReading;
    temperatureC_ = &temperatureC;
    humidityPercent_ = &humidityPercent;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/fan/duty", HTTP_POST, [this]() { handleSetDuty(); });
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

    if (!started_ && wifiStation_->isConnected()) {
        server_.begin();
        started_ = true;
        lastDiagnosticMs_ = now;

        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.print("[WEB] listening at http://");
            Serial.print(wifiStation_->localIp());
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
    const uint32_t startedMs = millis();
    const uint32_t heapBefore = ESP.getFreeHeap();
    String page(FPSTR(WEB_PANEL_HTML));
    if (page.length() == 0) {
        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.println("[WEB] root page allocation failed");
        }
        server_.send(503, "text/plain; charset=utf-8", "Out of memory\n");
        return;
    }
    page.replace(
        "__POLL_INTERVAL_MS__",
        String(Config::Web::POLL_INTERVAL_MS)
    );

    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "text/html; charset=utf-8", page);

    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.print("[WEB] root bytes=");
        Serial.print(page.length());
        Serial.print(" handler_ms=");
        Serial.print(millis() - startedMs);
        Serial.print(" heap_delta=");
        Serial.println(
            static_cast<int32_t>(heapBefore) -
            static_cast<int32_t>(ESP.getFreeHeap())
        );
    }
}

void WebPanel::handleStatus()
{
    recordRequest("GET /api/status", false);
    const uint32_t roundedRpm = fan_->currentRpm() <= 0.0f
        ? 0
        : static_cast<uint32_t>(fan_->currentRpm() + 0.5f);
    const String temperatureField =
        *hasClimateReading_ ? String(*temperatureC_, 1) : "null";
    const String humidityField =
        *hasClimateReading_ ? String(*humidityPercent_, 1) : "null";

    char response[192];
    snprintf(
        response,
        sizeof(response),
        "{\"rpm\":%lu,\"duty_percent\":%u,\"wifi_connected\":%s,"
        "\"temperature_c\":%s,\"humidity_percent\":%s}",
        static_cast<unsigned long>(roundedRpm),
        fan_->dutyPercent(),
        wifiStation_->isConnected() ? "true" : "false",
        temperatureField.c_str(),
        humidityField.c_str()
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

    server_.sendHeader("Cache-Control", "no-store");
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
