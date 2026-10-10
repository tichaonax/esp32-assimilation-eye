// Sign-in routes: login, session, logout and changing the sign-in details.

#include "InputRules.h"
#include "WebApi.h"
#include "WebApiInternal.h"

using webapi::sendError;
using webapi::sendJson;
using webapi::sendRuleError;
using webapi::text;

namespace {
// HttpOnly: page scripts cannot read the token. SameSite=Strict: the browser
// does not send it on requests that another website triggers. No "Secure":
// the interface is served over plain HTTP.
constexpr char COOKIE_ATTRIBUTES[] = "; Path=/; Max-Age=86400; HttpOnly; SameSite=Strict";
constexpr char CLEAR_COOKIE[] = "eye_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict";
}  // namespace

void WebApi::sendSession(AsyncWebServerRequest* request, bool authenticated, const char* setCookie) {
  JsonDocument doc;
  doc["authenticated"] = authenticated;
  if (authenticated) {
    // Said only to someone already signed in, not advertised to strangers.
    doc["username"] = _auth.username();
    doc["defaultPassword"] = _auth.usingDefaultPassword();
  }
  sendJson(request, 200, doc, setCookie);
}

void WebApi::addAuthRoutes() {
  route("/api/session", HTTP_GET, Access::Public,
        [this](AsyncWebServerRequest* request) { sendSession(request, signedIn(request), nullptr); });

  routeJson("/api/login", HTTP_POST, Access::Public, [this](AsyncWebServerRequest* request, JsonVariantConst body) {
    const char* username = text(body, "username");
    const char* password = text(body, "password");
    if (username == nullptr || password == nullptr) {
      sendError(request, 400, "A user name and a password are required");
      return;
    }

    char token[Auth::TOKEN_SIZE];
    uint32_t retryAfterS = 0;
    switch (_auth.login(username, password, token, retryAfterS)) {
      case Auth::LoginResult::Ok: {
        String cookie = F("eye_session=");
        cookie += token;
        cookie += COOKIE_ATTRIBUTES;
        sendSession(request, true, cookie.c_str());
        return;
      }
      case Auth::LoginResult::WrongCredentials:
        sendError(request, 401, "Wrong user name or password");
        return;
      case Auth::LoginResult::LockedOut: {
        JsonDocument doc;
        String message = F("Too many wrong attempts. Try again in ");
        message += retryAfterS;
        message += retryAfterS == 1 ? F(" second.") : F(" seconds.");
        doc["error"] = message;
        doc["retryAfterS"] = retryAfterS;
        AsyncResponseStream* response = request->beginResponseStream(webapi::JSON_TYPE);
        response->setCode(429);
        response->addHeader("Cache-Control", "no-store");
        response->addHeader("Retry-After", String(retryAfterS));
        serializeJson(doc, *response);
        request->send(response);
        return;
      }
    }
  });

  route("/api/logout", HTTP_POST, Access::SignedIn, [this](AsyncWebServerRequest* request) {
    char token[Auth::TOKEN_SIZE];
    if (sessionToken(request, token)) {
      _auth.logout(token);
    }
    sendSession(request, false, CLEAR_COOKIE);
    // Open live connections were authorized by a session that may be this
    // one. Close them all; browsers still signed in reconnect by themselves.
    _socket.closeAll();
  });

  routeJson("/api/credentials", HTTP_POST, Access::SignedIn,
            [this](AsyncWebServerRequest* request, JsonVariantConst body) {
              const char* current = text(body, "currentPassword");
              const char* username = text(body, "username");
              const char* password = text(body, "password");
              if (current == nullptr || username == nullptr || password == nullptr) {
                sendError(request, 400, "The current password, a user name and a new password are required");
                return;
              }
              // Rules first: a typo in the new details should not cost a password attempt.
              const char* problem = eye::rules::checkUsername(username);
              if (problem == nullptr) {
                problem = eye::rules::checkPassword(password);
              }
              if (problem != nullptr) {
                sendRuleError(request, problem);
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

              char token[Auth::TOKEN_SIZE] = {};
              sessionToken(request, token);
              if (!_auth.changeCredentials(username, password, token)) {
                sendError(request, 500, "The new details are in use but could not be saved; they will be lost at the next restart");
                return;
              }
              Serial.println(F("[auth] Sign-in details changed from the interface."));
              sendSession(request, true, nullptr);
              // Every other session has just been ended; drop their live connections too.
              _socket.closeAll();
            });
}
