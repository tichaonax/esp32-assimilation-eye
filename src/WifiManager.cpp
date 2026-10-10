#include "WifiManager.h"

#include "Defaults.h"

namespace {
constexpr uint32_t RETRY_MS = 15000;

// Why the last attempt to join failed; written by the Wi-Fi event task.
volatile uint8_t lastDisconnectReason = 0;

// Plain-language cause for the reason codes that matter in practice.
const char* describeReason(uint8_t reason) {
  switch (reason) {
    case 0:
      return "no answer yet";
    case WIFI_REASON_NO_AP_FOUND:
#ifdef WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
#endif
#ifdef WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
#endif
#ifdef WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
#endif
      return "network not found (check the name; the ESP32 only sees 2.4 GHz networks)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return "the network refused the password";
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_BEACON_TIMEOUT:
      return "the network stopped answering (weak signal?)";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
      return "the network would not accept another device";
    default:
      return "the network dropped the connection";
  }
}
}  // namespace

bool WifiManager::begin() {
#ifdef DEV_WIFI_SSID
  _stationWanted = true;
#endif

  // Network details are kept by this firmware, not by the Wi-Fi library.
  WiFi.persistent(false);
  WiFi.setHostname(defaults::HOSTNAME);
  WiFi.mode(_stationWanted ? WIFI_AP_STA : WIFI_AP);
  // Power saving delays replies by up to a few hundred milliseconds.
  WiFi.setSleep(false);

  const bool apOk = WiFi.softAP(defaults::AP_SSID, defaults::AP_PASSWORD);
  if (apOk) {
    Serial.printf("[wifi] Access point \"%s\" is up. Interface address: http://%s\n", defaults::AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println(F("[wifi] FAILED to start the access point."));
  }

#ifdef DEV_WIFI_SSID
  // The network name and password are deliberately not printed.
  Serial.println(F("[wifi] DEVELOPMENT BUILD: joining the network from platformio.local.ini."));
  WiFi.onEvent(
      [](arduino_event_id_t, arduino_event_info_t info) {
        lastDisconnectReason = info.wifi_sta_disconnected.reason;
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.setAutoReconnect(true);
  WiFi.begin(DEV_WIFI_SSID, DEV_WIFI_PASSWORD);
  _lastAttemptMs = millis();
#endif

  return apOk;
}

void WifiManager::loop() {
  if (!_stationWanted) {
    return;
  }
  const bool connected = stationConnected();
  if (connected != _stationWasConnected) {
    _stationWasConnected = connected;
    if (connected) {
      Serial.printf("[wifi] Joined the network. Interface address: http://%s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println(F("[wifi] Lost the network; retrying. The access point is still up."));
      _lastAttemptMs = millis();
    }
  }
#ifdef DEV_WIFI_SSID
  if (!connected && millis() - _lastAttemptMs >= RETRY_MS) {
    _lastAttemptMs = millis();
    const uint8_t reason = lastDisconnectReason;
    Serial.printf("[wifi] Not joined yet: %s (code %u). Retrying.\n", describeReason(reason), reason);
    WiFi.disconnect();
    WiFi.begin(DEV_WIFI_SSID, DEV_WIFI_PASSWORD);
  }
#endif
}

IPAddress WifiManager::accessPointIp() const {
  return WiFi.softAPIP();
}

bool WifiManager::stationConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress WifiManager::stationIp() const {
  return WiFi.localIP();
}
