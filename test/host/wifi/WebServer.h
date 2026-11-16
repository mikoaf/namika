#pragma once
#include "WiFi.h"
#include <functional>
#include <map>
constexpr int HTTP_GET = 0, HTTP_POST = 1;
class WebServer {
 public:
  static WebServer *active;
  std::map<std::string, std::function<void()>> handlers;
  std::map<std::string, String> args;
  std::string body;
  int code = 0;
  bool viaAp = true, running = false;
  explicit WebServer(int) { active = this; }
  void on(const char *path, int, std::function<void()> callback) { handlers[path] = callback; }
  void onNotFound(std::function<void()>) {}
  void sendHeader(const char *, const char *, bool = false) {}
  void send(int status, const char *, const char *text) { code = status; body = text; }
  void send_P(int status, const char *type, const char *text) { send(status, type, text); }
  void begin() { running = true; }
  void stop() { running = false; }
  void handleClient() {}
  String arg(const char *name) { return args[name]; }
  WebServer &client() { return *this; }
  IPAddress localIP() { return viaAp ? WiFi.softAPIP() : WiFi.localIP(); }
};
