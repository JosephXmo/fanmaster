#include "WifiStation.h"

#include <stddef.h>
#include <string.h>

#include "AssetFormat.h"
#include "Config.h"

namespace {
// One NVS blob prevents torn SSID/password pairs. No password is logged or echoed.
struct Credentials {
    char ssid[33];
    char password[65];
    uint8_t reserved[2];
    uint32_t crc;
};
static_assert(sizeof(Credentials) == 104, "Credential layout changed");

bool validPassword(const String &password)
{
    // Empty passwords allow open networks. WPA passphrases are 8..63 bytes;
    // a 64-byte key must be hexadecimal.
    if (!password.length()) return true;
    if (password.length() >= 8 && password.length() <= 63) return true;
    if (password.length() != 64) return false;
    for (size_t i = 0; i < password.length(); ++i) {
        const char c = password[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
}  // namespace

void WifiStation::begin()
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    macAddress_ = WiFi.macAddress();
    nvsReady_ = preferences_.begin("fanmaster", false);
    failureStartedMs_ = millis();
    if (loadCredentials()) {
        startConnectionAttempt(millis());
    } else {
        startSetup();
    }
}

bool WifiStation::loadCredentials()
{
    ssid_ = password_ = "";
    Credentials credentials{};
    if (!nvsReady_ || preferences_.getBytesLength("wifi") != sizeof(credentials) ||
        preferences_.getBytes("wifi", &credentials, sizeof(credentials)) != sizeof(credentials) ||
        !memchr(credentials.ssid, 0, sizeof(credentials.ssid)) ||
        !memchr(credentials.password, 0, sizeof(credentials.password)) ||
        (AssetFormat::crcUpdate(0xFFFFFFFF, &credentials, offsetof(Credentials, crc)) ^
         0xFFFFFFFF) != credentials.crc) return false;
    ssid_ = credentials.ssid;
    password_ = credentials.password;
    if (!ssid_.length() || !validPassword(password_)) {
        ssid_ = password_ = "";
        return false;
    }
    return true;
}

bool WifiStation::saveCredentials()
{
    if (!nvsReady_) return false;
    Credentials credentials{};
    ssid_.toCharArray(credentials.ssid, sizeof(credentials.ssid));
    password_.toCharArray(credentials.password, sizeof(credentials.password));
    credentials.crc = AssetFormat::crcUpdate(
        0xFFFFFFFF, &credentials, offsetof(Credentials, crc)) ^ 0xFFFFFFFF;
    return preferences_.putBytes("wifi", &credentials, sizeof(credentials)) == sizeof(credentials);
}

bool WifiStation::provision(const String &ssid, const String &password)
{
    if (!setupActive_ || candidate_ || !ssid.length() || ssid.length() > 32 ||
        strlen(ssid.c_str()) != ssid.length() ||
        strlen(password.c_str()) != password.length() || !validPassword(password)) return false;
    ssid_ = ssid;
    password_ = password;
    candidate_ = true;
    connected_ = false;
    connecting_ = false;
    attemptPending_ = true;
    provisionState_ = "connecting";
    // Let WebServer return HTTP 202 before a channel change can disrupt the AP.
    return true;
}

void WifiStation::update()
{
    if (setupActive_) dns_.processNextRequest();
    const uint32_t now = millis();
    if (attemptPending_) {
        attemptPending_ = false;
        WiFi.disconnect(false, false);
        startConnectionAttempt(now);
        return;
    }
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid_) {
        connecting_ = false;
        if (!connected_) {
            connected_ = true;
            connectedAtMs_ = now;
            if (candidate_) {
                provisionState_ = saveCredentials() ? "saved" : "storage_failed";
                candidate_ = false;
            }
            if (!Config::SerialConsole::PLOTTER_MODE) {
                Serial.print("[WIFI] connected ip=");
                Serial.println(WiFi.localIP());
            }
        }
        if (setupActive_ &&
            (!strcmp(provisionState_, "idle") || !strcmp(provisionState_, "saved")) &&
            now - connectedAtMs_ >= Config::Wifi::SETUP_CLOSE_GRACE_MS) stopSetup();
        return;
    }

    if (connected_) {
        connected_ = false;
        connecting_ = false;
        if (!strcmp(provisionState_, "saved")) provisionState_ = "idle";
        failureStartedMs_ = now;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
    }
    const bool needsSetup = !ssid_.length() ||
        now - failureStartedMs_ >= Config::Wifi::SETUP_AFTER_FAILURE_MS;
    if (!setupActive_ && needsSetup && (!setupAttempted_ ||
        now - lastSetupAttemptMs_ >= Config::Wifi::SETUP_RETRY_INTERVAL_MS)) {
        startSetup();
    }
    if (connecting_) {
        if (now - attemptStartedMs_ < Config::Wifi::CONNECT_TIMEOUT_MS) return;
        WiFi.disconnect(false, false);
        connecting_ = false;
        nextAttemptMs_ = now + Config::Wifi::RETRY_INTERVAL_MS;
        if (candidate_) {
            candidate_ = false;
            provisionState_ = "connection_failed";
            loadCredentials();  // Preserve and retry the previous working record.
        }
        return;
    }
    if (ssid_.length() && static_cast<int32_t>(now - nextAttemptMs_) >= 0) {
        startConnectionAttempt(now);
    }
}

void WifiStation::startConnectionAttempt(uint32_t now)
{
    WiFi.begin(ssid_.c_str(), password_.c_str());
    attemptStartedMs_ = now;
    connecting_ = true;
}

void WifiStation::startSetup()
{
    setupAttempted_ = true;
    lastSetupAttemptMs_ = millis();
    WiFi.mode(WIFI_AP_STA);
    String suffix = macAddress_;
    suffix.replace(":", "");
    suffix = suffix.substring(suffix.length() > 6 ? suffix.length() - 6 : 0);
    const String name = String(Config::Wifi::SETUP_AP_PREFIX) + suffix;
    if (!WiFi.softAP(name.c_str())) return;
    if (!dns_.start(53, "*", WiFi.softAPIP())) {
        WiFi.softAPdisconnect(false);
        WiFi.mode(WIFI_STA);
        return;
    }
    setupActive_ = true;
    if (!Config::SerialConsole::PLOTTER_MODE) {
        Serial.print("[WIFI] setup AP: ");
        Serial.print(name);
        Serial.print(" http://");
        Serial.println(WiFi.softAPIP());
    }
}

void WifiStation::stopSetup()
{
    dns_.stop();
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_STA);
    setupActive_ = false;
    provisionState_ = "idle";
}

bool WifiStation::isConnected() const { return connected_; }
bool WifiStation::isSetupActive() const { return setupActive_; }
bool WifiStation::isConnecting() const { return connecting_ || attemptPending_; }
bool WifiStation::isProvisioning() const { return candidate_; }
IPAddress WifiStation::localIp() const { return WiFi.localIP(); }
IPAddress WifiStation::setupIp() const { return WiFi.softAPIP(); }
const String &WifiStation::macAddress() const { return macAddress_; }
const char *WifiStation::provisionState() const { return provisionState_; }
