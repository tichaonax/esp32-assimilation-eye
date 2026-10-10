// Small helpers shared by the WebApi*.cpp files. Not for use elsewhere.
#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

namespace webapi {

constexpr char JSON_TYPE[] = "application/json";

inline void sendJson(AsyncWebServerRequest* request, int status, const JsonDocument& doc,
                     const char* setCookie = nullptr) {
  AsyncResponseStream* response = request->beginResponseStream(JSON_TYPE);
  response->setCode(status);
  response->addHeader("Cache-Control", "no-store");
  if (setCookie != nullptr) {
    response->addHeader("Set-Cookie", setCookie);
  }
  serializeJson(doc, *response);
  request->send(response);
}

inline void sendError(AsyncWebServerRequest* request, int status, const char* message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(request, status, doc);
}

// The rule checkers return messages that complete "The ...".
inline void sendRuleError(AsyncWebServerRequest* request, const char* rule) {
  String message = F("The ");
  message += rule;
  sendError(request, 400, message.c_str());
}

inline void sendOk(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(request, 200, doc);
}

// The text at key, or nullptr if it is missing or not text.
inline const char* text(JsonVariantConst body, const char* key) {
  JsonVariantConst value = body[key];
  return value.is<const char*>() ? value.as<const char*>() : nullptr;
}

}  // namespace webapi
