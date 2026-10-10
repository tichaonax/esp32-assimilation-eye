// The device's web server: the browser interface, and the HTTP and WebSocket
// API behind it.
//
// Public (no sign-in needed):
//   GET    /                    the interface (one compressed page, built from ui/)
//   POST   /api/login           {username, password}; sets the session cookie
//   GET    /api/session         whether the caller is signed in
//
// Everything else needs the session cookie and answers 401 without it:
//   POST   /api/logout
//   POST   /api/credentials     {currentPassword, username, password}
//   GET    /api/state           LED settings, available patterns, onboard LED availability
//   POST   /api/config          partial LED settings; replies with the resulting state
//   POST   /api/reset           default LED settings
//   WS     /ws                  live two-way LED state (messages below)
//   GET    /api/wifi            joined network and access point status
//   POST   /api/wifi            join a network: {ssid, password, addressing, ip, mask, gateway, dns1, dns2}
//   DELETE /api/wifi            forget the saved network
//   GET    /api/wifi/scan       nearby networks; starts a search and is polled until done
//   POST   /api/wifi/ap         access point {ssid, password}
//   GET    /api/system          firmware, chip, memory, uptime
//   POST   /api/update          firmware.bin as a multipart upload; restarts on success
//   POST   /api/restart
//   POST   /api/factory-reset   {currentPassword}
//
// Errors are {"error": "..."} with 400 (bad input), 401 (not signed in),
// 403 (wrong current password), 404, 413 (too large) or 429 (locked out).
// LED numbers out of range are clamped; everything else that breaks a rule is
// refused and nothing is changed.
//
// WebSocket, client to device:
//   {"type":"set","values":{...}}   same as POST /api/config
//   {"type":"reset"}                same as POST /api/reset
//   {"type":"get"}                  ask for the state
// WebSocket, device to client:
//   {"type":"state","state":{...}}  on connect, and to everyone after any change
//   {"type":"error","error":"..."}  to the sender only
#pragma once

#include <functional>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "AnimationTask.h"
#include "Auth.h"
#include "ConfigJson.h"
#include "ConfigStore.h"
#include "EyeConfig.h"
#include "FactoryReset.h"
#include "OnboardLed.h"
#include "OtaUpdate.h"
#include "WifiManager.h"

class WebApi {
 public:
  static constexpr uint16_t PORT = 80;
  static constexpr size_t MAX_BODY_BYTES = 2048;

  WebApi(AnimationTask& animation, ConfigStore& store, OnboardLed& onboard, Auth& auth, WifiManager& wifi,
         OtaUpdate& ota, FactoryReset& factoryReset);

  void begin();

  // Call from loop(): tidies up closed WebSocket connections.
  void loop();

  // Applies settings from anywhere in the firmware the same way the API does:
  // sanitize, hand to the animation, schedule a save, tell every open
  // interface. Returns false if the pattern name is not registered.
  bool updateConfig(const eye::EyeConfig& config);

 private:
  enum class Access { Public, SignedIn };
  using Handler = std::function<void(AsyncWebServerRequest*)>;
  using JsonHandler = std::function<void(AsyncWebServerRequest*, JsonVariantConst)>;

  // Registers a route. The JSON form reads and parses the body first and
  // answers 400 itself if there is none or it is not JSON.
  void route(const char* path, WebRequestMethodComposite method, Access access, Handler handler);
  void routeJson(const char* path, WebRequestMethodComposite method, Access access, JsonHandler handler);

  // Copies the caller's session token out of its cookie; false if there is none.
  static bool sessionToken(AsyncWebServerRequest* request, char token[Auth::TOKEN_SIZE]);
  bool signedIn(AsyncWebServerRequest* request);

  // Each adds one area's routes. Defined in WebApi*.cpp.
  void addAuthRoutes();
  void addLedRoutes();
  void addWifiRoutes();
  void addSystemRoutes();

  // LED state
  void fillState(JsonObject target) const;
  String stateMessage() const;
  void broadcastState();
  bool applyValues(JsonVariantConst values, char error[eye::json::ERROR_SIZE]);
  void sendState(AsyncWebServerRequest* request) const;

  // Sign-in
  void sendSession(AsyncWebServerRequest* request, bool authenticated, const char* setCookie);

  // Wi-Fi
  void fillWifi(JsonObject target);
  void sendWifi(AsyncWebServerRequest* request);

  // WebSocket
  void handleSocketEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleSocketText(AsyncWebSocketClient* client, const uint8_t* data, size_t len);

  AnimationTask& _animation;
  ConfigStore& _store;
  OnboardLed& _onboard;
  Auth& _auth;
  WifiManager& _wifi;
  OtaUpdate& _ota;
  FactoryReset& _factoryReset;
  AsyncWebServer _server;
  AsyncWebSocket _socket;
  uint32_t _lastCleanupMs = 0;
};
