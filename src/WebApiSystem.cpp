// System routes: information, firmware upload, restart and factory reset.

#include <stdlib.h>

#include <WiFi.h>

#include "BuildInfo.h"
#include "SystemControl.h"
#include "WebApi.h"
#include "WebApiInternal.h"

using webapi::sendError;
using webapi::sendJson;
using webapi::text;

namespace {

constexpr uint32_t RESTART_DELAY_MS = 1000;

// Progress of one firmware upload, kept on the request. The server frees
// _tempObject with free(), so this is plain data from calloc().
struct UploadState {
  bool authorized;
  bool finished;
  char error[96];
};

}  // namespace

void WebApi::addSystemRoutes() {
  route("/api/system", HTTP_GET, Access::SignedIn, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;

    JsonObject firmware = doc["firmware"].to<JsonObject>();
    firmware["version"] = buildinfo::version();
    firmware["built"] = buildinfo::builtAt();
    firmware["developmentBuild"] = buildinfo::developmentBuild();

    JsonObject chip = doc["chip"].to<JsonObject>();
    chip["model"] = ESP.getChipModel();
    // Core 3.x reports the revision as major * 100 + minor.
    chip["revision"] = String(ESP.getChipRevision() / 100) + "." + String(ESP.getChipRevision() % 100);
    chip["cores"] = ESP.getChipCores();
    chip["mac"] = WiFi.macAddress();

    doc["uptimeS"] = static_cast<uint32_t>(millis() / 1000);
    doc["resetReason"] = systemcontrol::resetReason();

    JsonObject memory = doc["memory"].to<JsonObject>();
    memory["freeBytes"] = ESP.getFreeHeap();
    memory["lowestFreeBytes"] = ESP.getMinFreeHeap();

    JsonObject flash = doc["flash"].to<JsonObject>();
    flash["sizeBytes"] = ESP.getFlashChipSize();
    flash["firmwareBytes"] = ESP.getSketchSize();
    flash["slotBytes"] = OtaUpdate::slotBytes();

    // For checking that the interface does not disturb the LEDs: the count of
    // frames drawn, and the longest wait between two since this was last read.
    JsonObject animation = doc["animation"].to<JsonObject>();
    animation["frames"] = _animation.frameCount();
    animation["tickMs"] = AnimationTask::TICK_MS;
    animation["worstGapMs"] = _animation.takeWorstGapMs();

    sendJson(request, 200, doc);
  });

  route("/api/restart", HTTP_POST, Access::SignedIn, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["restarting"] = true;
    sendJson(request, 200, doc);
    Serial.println(F("[system] Restart requested from the interface."));
    systemcontrol::scheduleRestart(RESTART_DELAY_MS);
  });

  routeJson("/api/factory-reset", HTTP_POST, Access::SignedIn,
            [this](AsyncWebServerRequest* request, JsonVariantConst body) {
              const char* current = text(body, "currentPassword");
              if (current == nullptr) {
                sendError(request, 400, "The current password is required");
                return;
              }
              if (!_auth.confirmPassword(current)) {
                if (_auth.lockedForS() > 0) {
                  sendError(request, 429, "Too many wrong attempts. Wait half a minute and try again.");
                } else {
                  sendError(request, 403, "The current password is wrong");
                }
                return;
              }
              JsonDocument doc;
              doc["restarting"] = true;
              sendJson(request, 200, doc);
              _factoryReset.request();
            });

  // Firmware upload. The file arrives in pieces through the second function
  // while the request is still coming in; the first runs once it has all
  // arrived and sends the verdict. Sign-in is checked on the first piece, so
  // nothing is written for a caller who is not signed in.
  _server.on(
      "/api/update", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        const UploadState* state = static_cast<const UploadState*>(request->_tempObject);
        if (state == nullptr) {
          if (!signedIn(request)) {
            sendError(request, 401, "Sign in first");
          } else {
            sendError(request, 400, "No firmware file was received");
          }
          return;
        }
        if (!state->authorized) {
          sendError(request, 401, "Sign in first");
          return;
        }
        if (state->error[0] != '\0') {
          sendError(request, 400, state->error);
          return;
        }
        if (!state->finished) {
          _ota.abort();
          sendError(request, 400, "The upload ended before the whole file arrived");
          return;
        }
        JsonDocument doc;
        doc["ok"] = true;
        doc["restarting"] = true;
        sendJson(request, 200, doc);
        systemcontrol::scheduleRestart(RESTART_DELAY_MS);
      },
      [this](AsyncWebServerRequest* request, const String&, size_t index, uint8_t* data, size_t len, bool final) {
        UploadState* state = static_cast<UploadState*>(request->_tempObject);
        if (index == 0 && state == nullptr) {
          state = static_cast<UploadState*>(calloc(1, sizeof(UploadState)));
          request->_tempObject = state;
          if (state == nullptr) {
            return;
          }
          state->authorized = signedIn(request);
          if (state->authorized) {
            const char* problem = _ota.start(data, len, request->contentLength());
            if (problem != nullptr) {
              strlcpy(state->error, problem, sizeof(state->error));
            } else {
              // A browser that goes away mid-upload must not leave the update open.
              request->onDisconnect([this]() {
                if (_ota.active()) {
                  _ota.abort();
                }
              });
            }
          }
        }
        if (state == nullptr || !state->authorized || state->error[0] != '\0' || state->finished) {
          return;  // the rest of a refused upload is received and dropped
        }

        const char* problem = _ota.write(data, len);
        if (problem == nullptr && final) {
          problem = _ota.finish();
          state->finished = problem == nullptr;
        }
        if (problem != nullptr) {
          strlcpy(state->error, problem, sizeof(state->error));
          _ota.abort();
        }
      });
}
