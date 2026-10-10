// Server set-up, routing, the interface page, and the LED routes and WebSocket.
// Sign-in, Wi-Fi and system routes are in WebApiAuth.cpp, WebApiWifi.cpp and
// WebApiSystem.cpp.

#include "WebApi.h"

#include <stdlib.h>
#include <string.h>

#include "FactoryConfig.h"
#include "WebApiInternal.h"
#include "generated/ui_bundle.h"

using webapi::sendError;
using webapi::sendJson;

namespace {
constexpr uint32_t SOCKET_CLEANUP_MS = 1000;
constexpr char HTML_TYPE[] = "text/html; charset=utf-8";
constexpr char COOKIE_NAME[] = "eye_session=";

// The interface is one gzip-compressed page built into the firmware. The
// browser may keep a copy but must check it is still current each time
// ("no-cache"), so a firmware update is picked up at once; while it is
// current the check costs a few bytes instead of the whole page.
//
// The page is public: it holds no data, and shows the sign-in form.
void sendInterface(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* response;
  if (request->hasHeader("If-None-Match") && request->header("If-None-Match") == UI_BUNDLE_ETAG) {
    response = request->beginResponse(304);
  } else {
    response = request->beginResponse(200, HTML_TYPE, UI_BUNDLE_GZ, UI_BUNDLE_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
  }
  response->addHeader("ETag", UI_BUNDLE_ETAG);
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

// Gathers the request body into one buffer. The server frees _tempObject with
// free() when the request ends, so it must come from malloc().
void collectBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0 && request->_tempObject == nullptr && total <= WebApi::MAX_BODY_BYTES) {
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

void sendSocketError(AsyncWebSocketClient* client, const char* message) {
  JsonDocument doc;
  doc["type"] = "error";
  doc["error"] = message;
  String text;
  serializeJson(doc, text);
  client->text(text);
}
}  // namespace

WebApi::WebApi(AnimationTask& animation, ConfigStore& store, OnboardLed& onboard, Auth& auth, WifiManager& wifi,
               OtaUpdate& ota, FactoryReset& factoryReset)
    : _animation(animation),
      _store(store),
      _onboard(onboard),
      _auth(auth),
      _wifi(wifi),
      _ota(ota),
      _factoryReset(factoryReset),
      _server(PORT),
      _socket("/ws") {}

void WebApi::begin() {
  _server.on("/", HTTP_GET, sendInterface);
  _server.on("/index.html", HTTP_GET, sendInterface);

  addAuthRoutes();
  addLedRoutes();
  addWifiRoutes();
  addSystemRoutes();

  // The browser sends the session cookie when it opens the WebSocket; without
  // a valid one the connection is refused before it is established.
  _socket.handleHandshake([this](AsyncWebServerRequest* request) { return signedIn(request); });
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

// ---- Routing ------------------------------------------------------------------

bool WebApi::sessionToken(AsyncWebServerRequest* request, char token[Auth::TOKEN_SIZE]) {
  if (!request->hasHeader("Cookie")) {
    return false;
  }
  const String& cookies = request->header("Cookie");
  int at = 0;
  // Find the cookie by name at the start of the header or after "; ".
  while ((at = cookies.indexOf(COOKIE_NAME, at)) >= 0) {
    if (at == 0 || cookies[at - 1] == ' ' || cookies[at - 1] == ';') {
      break;
    }
    at++;
  }
  if (at < 0) {
    return false;
  }
  at += sizeof(COOKIE_NAME) - 1;
  int end = cookies.indexOf(';', at);
  if (end < 0) {
    end = cookies.length();
  }
  if (end - at != static_cast<int>(Auth::TOKEN_SIZE - 1)) {
    return false;
  }
  memcpy(token, cookies.c_str() + at, Auth::TOKEN_SIZE - 1);
  token[Auth::TOKEN_SIZE - 1] = '\0';
  return true;
}

bool WebApi::signedIn(AsyncWebServerRequest* request) {
  char token[Auth::TOKEN_SIZE];
  return sessionToken(request, token) && _auth.sessionValid(token);
}

void WebApi::route(const char* path, WebRequestMethodComposite method, Access access, Handler handler) {
  _server.on(path, method, [this, access, handler](AsyncWebServerRequest* request) {
    if (access == Access::SignedIn && !signedIn(request)) {
      sendError(request, 401, "Sign in first");
      return;
    }
    handler(request);
  });
}

void WebApi::routeJson(const char* path, WebRequestMethodComposite method, Access access, JsonHandler handler) {
  _server.on(
      path, method,
      [this, access, handler](AsyncWebServerRequest* request) {
        if (access == Access::SignedIn && !signedIn(request)) {
          sendError(request, 401, "Sign in first");
          return;
        }
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
        handler(request, doc.as<JsonVariantConst>());
      },
      nullptr, collectBody);
}

// ---- LED settings ---------------------------------------------------------------

void WebApi::addLedRoutes() {
  route("/api/state", HTTP_GET, Access::SignedIn, [this](AsyncWebServerRequest* request) { sendState(request); });

  routeJson("/api/config", HTTP_POST, Access::SignedIn, [this](AsyncWebServerRequest* request, JsonVariantConst body) {
    char error[eye::json::ERROR_SIZE];
    if (!applyValues(body, error)) {
      sendError(request, 400, error);
      return;
    }
    sendState(request);
  });

  route("/api/reset", HTTP_POST, Access::SignedIn, [this](AsyncWebServerRequest* request) {
    updateConfig(eye::factoryConfig());
    sendState(request);
  });
}

bool WebApi::updateConfig(const eye::EyeConfig& config) {
  if (!_animation.apply(config)) {
    return false;
  }
  _store.requestSave();
  broadcastState();
  return true;
}

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

void WebApi::sendState(AsyncWebServerRequest* request) const {
  JsonDocument doc;
  fillState(doc.to<JsonObject>());
  sendJson(request, 200, doc);
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
    updateConfig(eye::factoryConfig());
    return;
  }
  if (strcmp(type, "get") == 0) {
    client->text(stateMessage());
    return;
  }
  sendSocketError(client, "type must be set, reset or get");
}
