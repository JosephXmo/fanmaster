#pragma once
#include "WiFi.h"
struct DNSServer {
    inline static bool works = true;
    void processNextRequest() {}
    bool start(int, const char *, IPAddress) { return works; }
    void stop() {}
};
