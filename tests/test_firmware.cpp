#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include "WifiStation.h"
#include "Config.h"
#include <SPIMemory.h>
#include "AssetStore.h"

void reset(bool clearNvs = true)
{
    fakeMillis = 100;
    WiFi = FakeWiFi{};
    Preferences::writable = Preferences::opens = true;
    DNSServer::works = true;
    if (clearNvs) Preferences::data.clear();
}

void wifiTests()
{
    reset();
    WifiStation fresh;
    fresh.begin();
    assert(fresh.isSetupActive() && WiFi.currentMode == WIFI_AP_STA);
    assert(!fresh.provision("", "password"));
    assert(!fresh.provision("home", "short"));
    assert(!fresh.provision("home", String(std::string(64, 'z'))));
    assert(!fresh.provision(String(std::string("home\0evil", 9)), "password"));
    assert(fresh.provision("home", "password"));
    assert(Preferences::data.empty());  // Submission never commits unverified credentials.
    assert(!fresh.provision("other", "password"));
    assert(fresh.isProvisioning());
    assert(WiFi.requestedSsid.length() == 0);
    fresh.update();
    WiFi.connect();
    fresh.update();
    assert(std::string(fresh.provisionState()) == "saved");
    assert(!Preferences::data.empty());
    auto workingRecord = Preferences::data;
    assert(fresh.isSetupActive());
    fakeMillis += Config::Wifi::SETUP_CLOSE_GRACE_MS;
    fresh.update();
    assert(!fresh.isSetupActive() && WiFi.currentMode == WIFI_STA);
    assert(!fresh.provision("other", "password"));

    reset(false);
    WifiStation reboot;
    reboot.begin();
    assert(!reboot.isSetupActive() && WiFi.requestedSsid == "home");
    fakeMillis += Config::Wifi::SETUP_AFTER_FAILURE_MS;
    reboot.update();
    assert(reboot.isSetupActive());
    fakeMillis += Config::Wifi::RETRY_INTERVAL_MS;
    reboot.update();
    assert(reboot.isConnecting() && !reboot.isProvisioning());
    assert(reboot.provision("wrong", "badpassword"));
    reboot.update();
    fakeMillis += Config::Wifi::CONNECT_TIMEOUT_MS;
    reboot.update();
    assert(std::string(reboot.provisionState()) == "connection_failed");
    assert(Preferences::data == workingRecord);
    fakeMillis += Config::Wifi::RETRY_INTERVAL_MS;
    reboot.update();
    assert(WiFi.requestedSsid == "home");
    WiFi.connect();
    reboot.update();
    fakeMillis += Config::Wifi::SETUP_CLOSE_GRACE_MS;
    reboot.update();
    assert(reboot.isSetupActive());  // Failed replacement remains actionable.
    assert(reboot.provision("new", "newpassword"));
    Preferences::writable = false;
    reboot.update();
    WiFi.connect();
    reboot.update();
    assert(std::string(reboot.provisionState()) == "storage_failed");
    fakeMillis += Config::Wifi::SETUP_CLOSE_GRACE_MS;
    reboot.update();
    assert(reboot.isSetupActive() && Preferences::data == workingRecord);
    Preferences::writable = true;
    assert(reboot.provision("new", "newpassword"));
    reboot.update();
    WiFi.connect();
    reboot.update();
    assert(std::string(reboot.provisionState()) == "saved");
    assert(Preferences::data != workingRecord);

    reset(false);
    fakeMillis = UINT32_MAX - 1000;
    WifiStation rollover;
    rollover.begin();
    fakeMillis += Config::Wifi::SETUP_AFTER_FAILURE_MS;
    rollover.update();
    assert(rollover.isSetupActive());
    WiFi.connect();
    rollover.update();
    fakeMillis += Config::Wifi::SETUP_CLOSE_GRACE_MS;
    rollover.update();
    assert(!rollover.isSetupActive());
    assert(std::string(rollover.provisionState()) == "idle");
    WiFi.state = WL_DISCONNECTED;
    rollover.update();
    fakeMillis += Config::Wifi::SETUP_AFTER_FAILURE_MS;
    rollover.update();
    assert(rollover.isSetupActive());

    reset();
    Preferences::opens = false;
    WifiStation noNvs;
    noNvs.begin();
    assert(noNvs.isSetupActive());
    assert(noNvs.provision("open", ""));
    noNvs.update();
    WiFi.connect();
    noNvs.update();
    assert(std::string(noNvs.provisionState()) == "storage_failed");

    reset();
    DNSServer::works = false;
    WifiStation failedDns;
    failedDns.begin();
    assert(!failedDns.isSetupActive());
    DNSServer::works = true;
    failedDns.update();
    assert(!failedDns.isSetupActive());
    fakeMillis += Config::Wifi::SETUP_RETRY_INTERVAL_MS;
    failedDns.update();
    assert(failedDns.isSetupActive());

    reset(false);
    Preferences::data = workingRecord;
    Preferences::data[0] ^= 1;
    WifiStation corrupt;
    corrupt.begin();
    assert(corrupt.isSetupActive() && !corrupt.isConnecting());
}

void assetTests(const char *imagePath)
{
    std::ifstream file(imagePath, std::ios::binary);
    assert(file);
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), {});
    std::copy(image.begin(), image.end(), fakeFlash.begin());
    AssetStore store;
    assert(store.begin());
    AssetFormat::Entry entry;
    for (const char *path : {"/index.html", "/setup.html", "/assets/panel.js", "/assets/setup.css"}) {
        assert(store.find(path, entry));
        uint8_t buffer[5];
        assert(store.read(entry, 0, buffer, sizeof(buffer)));
        assert(!store.read(entry, entry.length, buffer, 1));
    }
    assert(!store.find("/../../Config.h", entry));
    assert(!store.find("/missing", entry));
    assert(store.find("/index.html", entry));
    fakeFlash[entry.offset] ^= 1;
    AssetStore badContent;
    assert(badContent.begin());
    assert(!badContent.find("/index.html", entry));
    assert(badContent.find("/setup.html", entry));
    std::copy(image.begin(), image.end(), fakeFlash.begin());
    fakeFlash[40] ^= 1;
    AssetStore badIndex;
    assert(!badIndex.begin());
    std::copy(image.begin(), image.end(), fakeFlash.begin());
    fakeFlash[0] ^= 1;
    AssetStore badHeader;
    assert(!badHeader.begin());
    std::fill(fakeFlash.begin(), fakeFlash.end(), 0xff);
    AssetStore interruptedUpload;
    assert(!interruptedUpload.begin());
    W25q64 flash;
    fakeFlashConnected = false;
    assert(!flash.begin());
    fakeFlashConnected = true;
    assert(flash.begin());
    uint8_t page[256] = {};
    fakeFlashConnected = false;
    assert(!flash.read(0, page, 1));
    fakeFlashConnected = true;
    assert(flash.programPage(0, page, 256));
    assert(!flash.programPage(255, page, 2));
    assert(!flash.programPage(AssetFormat::CAPACITY, page, 1));
    assert(!flash.read(AssetFormat::CAPACITY, page, 1));
    assert(!flash.eraseSector(1));
    assert(flash.eraseSector(0));
    assert(fakeFlash[0] == 0xff);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    wifiTests();
    assetTests(argv[1]);
    std::cout << "Firmware state and resource integrity tests passed\n";
}
