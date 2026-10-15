#pragma once
#include "WiFi.h"
#include <functional>
#include <map>

constexpr int HTTP_GET = 1, HTTP_POST = 2;
struct FakeClient {
    IPAddress bound{456};
    bool open = true, failWrite = false;
    std::string body;
    void setTimeout(uint32_t) {}
    IPAddress localIP() const { return bound; }
    size_t write(const uint8_t *data, size_t size) {
        if (failWrite) return 0;
        body.append(reinterpret_cast<const char *>(data), size); return size;
    }
    void stop() { open = false; }
};
class WebServer {
public:
    inline static WebServer *instance = nullptr;
    explicit WebServer(int) { instance = this; }
    void on(const char *path, int method, std::function<void()> handler) {
        routes[{path, method}] = handler;
    }
    void onNotFound(std::function<void()> handler) { notFound = handler; }
    void begin() {}
    void handleClient() {}
    FakeClient &client() { return socket; }
    bool hasArg(const char *key) const { return arguments.count(key); }
    String arg(const char *key) const { return arguments.at(key); }
    String uri() const { return path; }
    int method() const { return verb; }
    void sendHeader(const char *name, const String &value, bool = false) {
        headers[name] = value.c_str();
    }
    void setContentLength(size_t size) { contentLength = size; }
    void send(int code, const char *type, const char *body) {
        status = code; contentType = type; socket.body = body;
    }
    void send_P(int code, const char *type, const char *body) { send(code, type, body); }
    void request(const char *url, int method, std::map<std::string, String> args = {}, bool ap = true) {
        socket = FakeClient{};
        socket.bound = ap ? IPAddress{456} : IPAddress{123};
        path = url; verb = method; arguments = std::move(args);
        headers.clear(); contentLength = 0; status = 0; contentType.clear();
        auto route = routes.find({url, method});
        if (route == routes.end()) notFound(); else route->second();
    }
    FakeClient socket;
    int status = 0;
    size_t contentLength = 0;
    std::string contentType;
    std::map<std::string, std::string> headers;
private:
    std::map<std::pair<std::string, int>, std::function<void()>> routes;
    std::map<std::string, String> arguments;
    std::function<void()> notFound;
    String path;
    int verb = 0;
};
