// The device's web server: the browser interface and the HTTP and WebSocket
// API for LED settings.
//
//   GET  /             the interface (one compressed page, built from ui/)
//   GET  /api/state   everything: settings, available patterns, onboard LED availability
//   POST /api/config   partial settings as JSON; replies with the resulting state
//   POST /api/reset    back to the default LED settings; replies with the state
//   WS   /ws           live two-way state (messages below)
//
// Out-of-range numbers are clamped. A wrong type, an unknown pattern,
// direction or mode, or a body that is not JSON gets HTTP 400 and
// {"error": "..."}; nothing is changed.
//
// WebSocket, client to device:
//   {"type":"set","values":{...}}   same as POST /api/config
//   {"type":"reset"}                same as POST /api/reset
//   {"type":"get"}                  ask for the state
// WebSocket, device to client:
//   {"type":"state","state":{...}}  on connect, and to everyone after any change
//   {"type":"error","error":"..."}  to the sender only
//
// There is no login yet: until Phase 6 the access point password is the only
// protection.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "AnimationTask.h"
#include "ConfigJson.h"
#include "ConfigStore.h"
#include "EyeConfig.h"
#include "OnboardLed.h"

class WebApi {
 public:
  static constexpr uint16_t PORT = 80;
  static constexpr size_t MAX_BODY_BYTES = 2048;

  WebApi(AnimationTask& animation, ConfigStore& store, OnboardLed& onboard);

  void begin();

  // Call from loop(): tidies up closed WebSocket connections.
  void loop();

  // Applies settings from anywhere in the firmware the same way the API does:
  // sanitize, hand to the animation, schedule a save, tell every open
  // interface. Returns false if the pattern name is not registered.
  bool updateConfig(const eye::EyeConfig& config);

 private:
  void fillState(JsonObject target) const;
  String stateMessage() const;
  void broadcastState();

  // Applies a partial settings object. On failure, error says why.
  bool applyValues(JsonVariantConst values, char error[eye::json::ERROR_SIZE]);

  void sendState(AsyncWebServerRequest* request) const;
  static void sendError(AsyncWebServerRequest* request, int status, const char* message);

  void handleConfig(AsyncWebServerRequest* request);
  static void collectBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

  void handleSocketEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleSocketText(AsyncWebSocketClient* client, const uint8_t* data, size_t len);
  static void sendSocketError(AsyncWebSocketClient* client, const char* message);

  AnimationTask& _animation;
  ConfigStore& _store;
  OnboardLed& _onboard;
  AsyncWebServer _server;
  AsyncWebSocket _socket;
  uint32_t _lastCleanupMs = 0;
};
