// Wi-Fi routes: status, joining a network with DHCP or a static address,
// forgetting it, searching for networks, and the access point's own details.
//
// Passwords are accepted here but never sent back.

#include <WiFi.h>

#include "Defaults.h"
#include "InputRules.h"
#include "WebApi.h"
#include "WebApiInternal.h"

using webapi::sendError;
using webapi::sendJson;
using webapi::sendRuleError;
using webapi::text;

namespace {

const char* stateName(WifiManager::State state) {
  switch (state) {
    case WifiManager::State::NotConfigured:
      return "notConfigured";
    case WifiManager::State::Connecting:
      return "connecting";
    case WifiManager::State::Connected:
      return "connected";
    case WifiManager::State::Failed:
      return "failed";
  }
  return "notConfigured";
}

void putAddress(JsonObject target, const char* key, uint32_t address) {
  char buffer[eye::rules::IPV4_TEXT_SIZE] = "";
  if (address != 0) {
    eye::rules::formatIpv4(address, buffer);
  }
  target[key] = static_cast<const char*>(buffer);
}

// Reads one address field. Absent or empty leaves value at 0. Returns false,
// having answered the request, if the text is not an address.
bool readAddress(AsyncWebServerRequest* request, JsonVariantConst body, const char* key, const char* label,
                 uint32_t& value) {
  value = 0;
  const char* given = text(body, key);
  if (given == nullptr || given[0] == '\0') {
    return true;
  }
  if (!eye::rules::parseIpv4(given, value)) {
    String message = F("The ");
    message += label;
    message += F(" is not a valid address (for example 192.168.1.50)");
    sendError(request, 400, message.c_str());
    return false;
  }
  return true;
}

}  // namespace

void WebApi::fillWifi(JsonObject target) {
  const WifiManager::Station saved = _wifi.station();
  const WifiManager::State state = _wifi.state();
  const bool connected = state == WifiManager::State::Connected;

  JsonObject station = target["station"].to<JsonObject>();
  station["configured"] = saved.configured;
  station["fromBuild"] = saved.fromBuild;
  station["ssid"] = static_cast<const char*>(saved.ssid);
  station["status"] = stateName(state);
  station["problem"] = _wifi.problem();
  station["addressing"] = saved.dhcp ? "dhcp" : "static";
  // What is in force now, whichever way it was obtained.
  station["ip"] = connected ? WiFi.localIP().toString() : String();
  station["mask"] = connected ? WiFi.subnetMask().toString() : String();
  station["gateway"] = connected ? WiFi.gatewayIP().toString() : String();
  station["dns1"] = connected ? WiFi.dnsIP(0).toString() : String();
  station["dns2"] = connected && WiFi.dnsIP(1) != IPAddress(static_cast<uint32_t>(0)) ? WiFi.dnsIP(1).toString()
                                                                                    : String();
  if (connected) {
    station["rssi"] = WiFi.RSSI();
    station["channel"] = WiFi.channel();
  }
  // What was typed in for static addressing, to fill the form in again.
  JsonObject fixed = station["static"].to<JsonObject>();
  putAddress(fixed, "ip", saved.fixed.ip);
  putAddress(fixed, "mask", saved.fixed.mask);
  putAddress(fixed, "gateway", saved.fixed.gateway);
  putAddress(fixed, "dns1", saved.fixed.dns1);
  putAddress(fixed, "dns2", saved.fixed.dns2);

  const WifiManager::AccessPoint ap = _wifi.accessPoint();
  JsonObject accessPoint = target["accessPoint"].to<JsonObject>();
  accessPoint["ssid"] = static_cast<const char*>(ap.ssid);
  accessPoint["ip"] = WiFi.softAPIP().toString();
  accessPoint["clients"] = WiFi.softAPgetStationNum();
  accessPoint["defaultPassword"] = _wifi.accessPointPasswordIsDefault();

  target["hostname"] = defaults::HOSTNAME;
  target["mac"] = WiFi.macAddress();
}

void WebApi::sendWifi(AsyncWebServerRequest* request) {
  JsonDocument doc;
  fillWifi(doc.to<JsonObject>());
  sendJson(request, 200, doc);
}

void WebApi::addWifiRoutes() {
  // The server matches "/api/wifi" against "/api/wifi/..." as well, so the
  // longer paths must be registered first.

  // ?start=1 begins a new search. Without it, a search is only started if
  // there has never been one. Poll until "scanning" is false.
  route("/api/wifi/scan", HTTP_GET, Access::SignedIn, [this](AsyncWebServerRequest* request) {
    if (request->hasParam("start") || (!_wifi.scanned() && !_wifi.scanning())) {
      _wifi.startScan();
    }
    JsonDocument doc;
    doc["scanning"] = _wifi.scanning();
    JsonArray list = doc["networks"].to<JsonArray>();
    WifiManager::Network found[WifiManager::MAX_NETWORKS];
    const size_t count = _wifi.networks(found);
    for (size_t i = 0; i < count; i++) {
      JsonObject entry = list.add<JsonObject>();
      entry["ssid"] = static_cast<const char*>(found[i].ssid);
      entry["rssi"] = found[i].rssi;
      entry["secure"] = found[i].secure;
      entry["channel"] = found[i].channel;
    }
    sendJson(request, 200, doc);
  });

  routeJson("/api/wifi/ap", HTTP_POST, Access::SignedIn, [this](AsyncWebServerRequest* request, JsonVariantConst body) {
    const char* ssid = text(body, "ssid");
    const char* password = text(body, "password");
    const char* problem = eye::rules::checkNetworkName(ssid);
    if (problem == nullptr) {
      problem = eye::rules::checkAccessPointPassword(password);
    }
    if (problem != nullptr) {
      sendRuleError(request, problem);
      return;
    }
    WifiManager::AccessPoint ap = {};
    strlcpy(ap.ssid, ssid, sizeof(ap.ssid));
    strlcpy(ap.password, password, sizeof(ap.password));
    _wifi.setAccessPoint(ap);
    Serial.println(F("[wifi] Access point details changed from the interface."));
    sendWifi(request);
  });

  route("/api/wifi", HTTP_GET, Access::SignedIn, [this](AsyncWebServerRequest* request) { sendWifi(request); });

  routeJson("/api/wifi", HTTP_POST, Access::SignedIn, [this](AsyncWebServerRequest* request, JsonVariantConst body) {
    const char* ssid = text(body, "ssid");
    const char* problem = eye::rules::checkNetworkName(ssid);
    if (problem != nullptr) {
      sendRuleError(request, problem);
      return;
    }

    const WifiManager::Station saved = _wifi.station();
    WifiManager::Station next = {};
    strlcpy(next.ssid, ssid, sizeof(next.ssid));

    // Leaving the password out keeps the saved one, so the addressing of the
    // current network can be changed without typing the password again.
    const char* password = text(body, "password");
    if (password == nullptr) {
      password = (saved.configured && strcmp(saved.ssid, ssid) == 0) ? saved.password : "";
    }
    problem = eye::rules::checkStationPassword(password);
    if (problem != nullptr) {
      sendRuleError(request, problem);
      return;
    }
    strlcpy(next.password, password, sizeof(next.password));

    const char* addressing = text(body, "addressing");
    if (addressing == nullptr || strcmp(addressing, "dhcp") == 0) {
      next.dhcp = true;
      // Keep the last static details so switching back does not mean retyping.
      next.fixed = saved.fixed;
    } else if (strcmp(addressing, "static") == 0) {
      next.dhcp = false;
      if (!readAddress(request, body, "ip", "IP address", next.fixed.ip) ||
          !readAddress(request, body, "mask", "subnet mask", next.fixed.mask) ||
          !readAddress(request, body, "gateway", "gateway", next.fixed.gateway) ||
          !readAddress(request, body, "dns1", "first DNS server", next.fixed.dns1) ||
          !readAddress(request, body, "dns2", "second DNS server", next.fixed.dns2)) {
        return;
      }
      if (next.fixed.ip == 0 || next.fixed.mask == 0 || next.fixed.gateway == 0) {
        sendError(request, 400, "A static address needs an IP address, a subnet mask and a gateway");
        return;
      }
      problem = eye::rules::checkStaticAddress(next.fixed);
      if (problem != nullptr) {
        sendRuleError(request, problem);
        return;
      }
    } else {
      sendError(request, 400, "addressing must be dhcp or static");
      return;
    }

    _wifi.join(next);
    sendWifi(request);
  });

  route("/api/wifi", HTTP_DELETE, Access::SignedIn, [this](AsyncWebServerRequest* request) {
    _wifi.forget();
    sendWifi(request);
  });
}
