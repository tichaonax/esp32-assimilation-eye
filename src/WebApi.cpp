#include "WebApi.h"

#include <stdlib.h>
#include <string.h>

#include "ConfigJson.h"

namespace {
constexpr uint32_t SOCKET_CLEANUP_MS = 1000;
constexpr char JSON_TYPE[] = "application/json";

constexpr char PLACEHOLDER_PAGE[] =
    "Assimilation Eye\n\n"
    "The device API is running. The browser interface arrives in a later firmware version.\n"
    "Current state: /api/state\n";

void sendJson(AsyncWebServerRequest* request, int status, const JsonDocument& doc) {
  AsyncResponseStream* response = request->beginResponseStream(JSON_TYPE);
  response->setCode(status);
  response->addHeader("Cache-Control", "no-store");
  serializeJson(doc, *response);
  request->send(response);
}
}  // namespace

WebApi::WebApi(AnimationTask& animation, ConfigStore& store, OnboardLed& onboard)
    : _animation(animation), _store(store), _onboard(onboard), _server(PORT), _socket("/ws") {}

void WebApi::begin() {
  _server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* request) { sendState(request); });

  _server.on(
      "/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) { handleConfig(request); }, nullptr,
      collectBody);

  _server.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    updateConfig(eye::defaultConfig());
    sendState(request);
  });

  _server.on("/", HTTP_GET,
             [](AsyncWebServerRequest* request) { request->send(200, "text/plain; charset=utf-8", PLACEHOLDER_PAGE); });

  _socket.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
                         size_t len) { handleSocketEvent(client, type, arg, data, len); });
  _server.addHandler(&_socket);

  _server.onNotFound([](AsyncWebServerRequest* request) {
    // A known path with the wrong method lands here too.
    sendError(request, 404, "Not found");
  });

  _server.begin();
  Serial.printf("[api] Listening on port %u.\n", PORT);
}

void WebApi::loop() {
  const uint32_t now = millis();
  if (now - _lastCleanupMs >= SOCKET_CLEANUP_MS) {
    _lastCleanupMs = now;
    _socket.cleanupClients();
  }
}

bool WebApi::updateConfig(const eye::EyeConfig& config) {
  if (!_animation.apply(config)) {
    return false;
  }
  _store.requestSave();
  broadcastState();
  return true;
}

// ---- State ------------------------------------------------------------------

void WebApi::fillState(JsonObject target) const {
  eye::json::write(_animation.config(), target);

  JsonArray patterns = target["patterns"].to<JsonArray>();
  const eye::PatternRegistry& registry = _animation.patterns();
  for (size_t i = 0; i < registry.count(); i++) {
    JsonObject entry = patterns.add<JsonObject>();
    entry["name"] = registry.at(i)->name();
    entry["label"] = registry.at(i)->label();
  }
  target["onboardAvailable"] = _onboard.available();
}

String WebApi::stateMessage() const {
  JsonDocument doc;
  doc["type"] = "state";
  fillState(doc["state"].to<JsonObject>());
  String text;
  serializeJson(doc, text);
  return text;
}

void WebApi::broadcastState() {
  if (_socket.count() > 0) {
    _socket.textAll(stateMessage());
  }
}

bool WebApi::applyValues(JsonVariantConst values, char error[eye::json::ERROR_SIZE]) {
  eye::EyeConfig config = _animation.config();
  if (!eye::json::read(values, config, error)) {
    return false;
  }
  if (!updateConfig(config)) {
    snprintf(error, eye::json::ERROR_SIZE, "pattern is not a known pattern");
    return false;
  }
  return true;
}

// ---- HTTP -------------------------------------------------------------------

void WebApi::sendState(AsyncWebServerRequest* request) const {
  JsonDocument doc;
  fillState(doc.to<JsonObject>());
  sendJson(request, 200, doc);
}

void WebApi::sendError(AsyncWebServerRequest* request, int status, const char* message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(request, status, doc);
}

// Gathers the request body into one buffer. The server frees _tempObject with
// free() when the request ends, so it must come from malloc().
void WebApi::collectBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0 && request->_tempObject == nullptr && total <= MAX_BODY_BYTES) {
    request->_tempObject = malloc(total + 1);
    if (request->_tempObject != nullptr) {
      static_cast<char*>(request->_tempObject)[total] = '\0';
    }
  }
  if (request->_tempObject == nullptr || index + len > total) {
    return;
  }
  memcpy(static_cast<char*>(request->_tempObject) + index, data, len);
}

void WebApi::handleConfig(AsyncWebServerRequest* request) {
  if (request->contentLength() > MAX_BODY_BYTES) {
    sendError(request, 413, "Request body is too large");
    return;
  }
  if (request->_tempObject == nullptr) {
    sendError(request, 400, "A JSON body is required");
    return;
  }

  JsonDocument doc;
  const char* body = static_cast<const char*>(request->_tempObject);
  if (deserializeJson(doc, body, request->contentLength()) != DeserializationError::Ok) {
    sendError(request, 400, "The body is not valid JSON");
    return;
  }

  char error[eye::json::ERROR_SIZE];
  if (!applyValues(doc.as<JsonVariantConst>(), error)) {
    sendError(request, 400, error);
    return;
  }
  sendState(request);
}

// ---- WebSocket --------------------------------------------------------------

void WebApi::handleSocketEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
                               size_t len) {
  if (type == WS_EVT_CONNECT) {
    // A slow or sleeping browser misses updates instead of being cut off;
    // every state message is complete, so the next one catches it up.
    client->setCloseClientOnQueueFull(false);
    client->text(stateMessage());
    return;
  }
  if (type != WS_EVT_DATA) {
    return;
  }
  const AwsFrameInfo* info = static_cast<const AwsFrameInfo*>(arg);
  // Every message this API accepts fits in one frame.
  const bool whole = info->final && info->index == 0 && info->len == len;
  if (!whole || info->opcode != WS_TEXT) {
    sendSocketError(client, "Messages must be a single JSON text frame");
    return;
  }
  if (len > MAX_BODY_BYTES) {
    sendSocketError(client, "Message is too large");
    return;
  }
  handleSocketText(client, data, len);
}

void WebApi::handleSocketText(AsyncWebSocketClient* client, const uint8_t* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, reinterpret_cast<const char*>(data), len) != DeserializationError::Ok) {
    sendSocketError(client, "The message is not valid JSON");
    return;
  }
  const char* type = doc["type"] | "";

  if (strcmp(type, "set") == 0) {
    char error[eye::json::ERROR_SIZE];
    if (!applyValues(doc["values"], error)) {
      sendSocketError(client, error);
    }
    return;
  }
  if (strcmp(type, "reset") == 0) {
    updateConfig(eye::defaultConfig());
    return;
  }
  if (strcmp(type, "get") == 0) {
    client->text(stateMessage());
    return;
  }
  sendSocketError(client, "type must be set, reset or get");
}

void WebApi::sendSocketError(AsyncWebSocketClient* client, const char* message) {
  JsonDocument doc;
  doc["type"] = "error";
  doc["error"] = message;
  String text;
  serializeJson(doc, text);
  client->text(text);
}
