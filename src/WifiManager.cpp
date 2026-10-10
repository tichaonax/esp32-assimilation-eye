#include "WifiManager.h"

#include <ESPmDNS.h>
#include <Preferences.h>

#include "Defaults.h"

namespace {

constexpr char NVS_NAMESPACE[] = "wifi";
constexpr char KEY_SSID[] = "ssid";
constexpr char KEY_PASSWORD[] = "pass";
constexpr char KEY_DHCP[] = "dhcp";
constexpr char KEY_IP[] = "ip";
constexpr char KEY_MASK[] = "mask";
constexpr char KEY_GATEWAY[] = "gw";
constexpr char KEY_DNS1[] = "dns1";
constexpr char KEY_DNS2[] = "dns2";
constexpr char KEY_AP_SSID[] = "apssid";
constexpr char KEY_AP_PASSWORD[] = "appass";

constexpr uint32_t ACCESS_POINT_APPLY_DELAY_MS = 1000;
constexpr uint32_t SCAN_TIMEOUT_MS = 15000;

// Why the last attempt to join failed; written by the Wi-Fi event task.
volatile uint8_t lastDisconnectReason = 0;

// Plain-language cause for the reason codes that matter in practice.
const char* describeReason(uint8_t reason) {
  switch (reason) {
    case 0:
      return "no answer from the network yet";
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

IPAddress toIp(uint32_t address) {
  return IPAddress(static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
                   static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address));
}

class Lock {
 public:
  explicit Lock(SemaphoreHandle_t mutex) : _mutex(mutex) { xSemaphoreTake(_mutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(_mutex); }
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

 private:
  SemaphoreHandle_t _mutex;
};

}  // namespace

// ---- Saved settings -----------------------------------------------------------

void WifiManager::loadSaved() {
  _station = {};
  _station.dhcp = true;
  strlcpy(_ap.ssid, defaults::AP_SSID, sizeof(_ap.ssid));
  strlcpy(_ap.password, defaults::AP_PASSWORD, sizeof(_ap.password));

  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, true)) {
    const String ssid = prefs.isKey(KEY_SSID) ? prefs.getString(KEY_SSID) : String();
    const String password = prefs.isKey(KEY_PASSWORD) ? prefs.getString(KEY_PASSWORD) : String();
    if (eye::rules::checkNetworkName(ssid.c_str()) == nullptr &&
        eye::rules::checkStationPassword(password.c_str()) == nullptr) {
      _station.configured = true;
      strlcpy(_station.ssid, ssid.c_str(), sizeof(_station.ssid));
      strlcpy(_station.password, password.c_str(), sizeof(_station.password));
      _station.dhcp = prefs.getBool(KEY_DHCP, true);
      _station.fixed.ip = prefs.getUInt(KEY_IP, 0);
      _station.fixed.mask = prefs.getUInt(KEY_MASK, 0);
      _station.fixed.gateway = prefs.getUInt(KEY_GATEWAY, 0);
      _station.fixed.dns1 = prefs.getUInt(KEY_DNS1, 0);
      _station.fixed.dns2 = prefs.getUInt(KEY_DNS2, 0);
      // A static address that no longer passes the rules is not worth trying.
      if (!_station.dhcp && eye::rules::checkStaticAddress(_station.fixed) != nullptr) {
        _station.dhcp = true;
      }
    }
    const String apSsid = prefs.isKey(KEY_AP_SSID) ? prefs.getString(KEY_AP_SSID) : String();
    const String apPassword = prefs.isKey(KEY_AP_PASSWORD) ? prefs.getString(KEY_AP_PASSWORD) : String();
    if (eye::rules::checkNetworkName(apSsid.c_str()) == nullptr &&
        eye::rules::checkAccessPointPassword(apPassword.c_str()) == nullptr) {
      strlcpy(_ap.ssid, apSsid.c_str(), sizeof(_ap.ssid));
      strlcpy(_ap.password, apPassword.c_str(), sizeof(_ap.password));
    }
    prefs.end();
  }

#ifdef DEV_WIFI_SSID
  if (!_station.configured && eye::rules::checkNetworkName(DEV_WIFI_SSID) == nullptr) {
    _station.configured = true;
    _station.fromBuild = true;
    _station.dhcp = true;
    strlcpy(_station.ssid, DEV_WIFI_SSID, sizeof(_station.ssid));
    strlcpy(_station.password, DEV_WIFI_PASSWORD, sizeof(_station.password));
  }
#endif
}

// ---- Start-up -------------------------------------------------------------------

bool WifiManager::begin() {
  _mutex = xSemaphoreCreateMutex();
  loadSaved();

  WiFi.onEvent(
      [](arduino_event_id_t, arduino_event_info_t info) {
        lastDisconnectReason = info.wifi_sta_disconnected.reason;
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Network details are kept by this firmware, not by the Wi-Fi library.
  WiFi.persistent(false);
  WiFi.setHostname(defaults::HOSTNAME);
  // Station mode stays on even with nothing to join: searching for networks needs it.
  WiFi.mode(WIFI_AP_STA);
  // Power saving delays replies by up to a few hundred milliseconds.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  const bool apOk = WiFi.softAP(_ap.ssid, _ap.password);
  if (apOk) {
    Serial.printf("[wifi] Access point \"%s\" is up. Interface address: http://%s\n", _ap.ssid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println(F("[wifi] FAILED to start the access point."));
  }

  if (MDNS.begin(defaults::HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }

  if (_station.configured) {
    // The network name is the user's own business on a shared serial log; say only where it came from.
    Serial.println(_station.fromBuild ? F("[wifi] DEVELOPMENT BUILD: joining the network from platformio.local.ini.")
                                      : F("[wifi] Joining the saved network."));
    _applyStation = true;
  } else {
    Serial.println(F("[wifi] No network saved. Use the Wi-Fi screen to join one."));
  }
  return apOk;
}

void WifiManager::startAccessPoint() {
  const AccessPoint ap = accessPoint();
  if (WiFi.softAP(ap.ssid, ap.password)) {
    Serial.printf("[wifi] Access point is now \"%s\".\n", ap.ssid);
  } else {
    Serial.println(F("[wifi] FAILED to apply the new access point details."));
  }
}

void WifiManager::startStation() {
  const Station target = station();
  WiFi.disconnect();
  if (!target.configured) {
    _state = State::NotConfigured;
    return;
  }

  if (target.dhcp) {
    // All zeros hands addressing back to DHCP.
    WiFi.config(IPAddress(static_cast<uint32_t>(0)), IPAddress(static_cast<uint32_t>(0)),
                IPAddress(static_cast<uint32_t>(0)));
  } else {
    const uint32_t dns1 = target.fixed.dns1 != 0 ? target.fixed.dns1 : target.fixed.gateway;
    WiFi.config(toIp(target.fixed.ip), toIp(target.fixed.gateway), toIp(target.fixed.mask), toIp(dns1),
                toIp(target.fixed.dns2));
  }

  lastDisconnectReason = 0;
  WiFi.begin(target.ssid, target.password[0] != '\0' ? target.password : nullptr);
  _attemptStartedMs = millis();
  _state = State::Connecting;
}

// ---- Main-task work -------------------------------------------------------------

void WifiManager::loop() {
  const uint32_t now = millis();

  if (_applyAccessPoint && static_cast<int32_t>(now - _applyAccessPointAtMs) >= 0) {
    _applyAccessPoint = false;
    startAccessPoint();
  }

  // Searching and joining cannot share the radio, so a search goes first and
  // joining resumes when it is done.
  if (_scanWanted && !_scanRunning) {
    _scanWanted = false;
    if (_state == State::Connecting || _state == State::Failed) {
      WiFi.disconnect();
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true /* in the background */, false /* skip hidden networks */);
    _scanRunning = true;
    _scanStartedMs = now;
  }
  if (_scanRunning) {
    const int16_t found = WiFi.scanComplete();
    if (found >= 0) {
      collectScan(found);
    } else if (found == WIFI_SCAN_FAILED || now - _scanStartedMs > SCAN_TIMEOUT_MS) {
      collectScan(0);
    } else {
      return;
    }
    if (_state == State::Connecting || _state == State::Failed) {
      _applyStation = true;
    }
  }

  if (_applyStation) {
    _applyStation = false;
    _everConnected = false;
    startStation();
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  switch (_state) {
    case State::NotConfigured:
      break;

    case State::Connecting:
      if (connected) {
        _state = State::Connected;
        _everConnected = true;
        Serial.printf("[wifi] Joined the network. Interface address: http://%s  (also http://%s.local)\n",
                      WiFi.localIP().toString().c_str(), defaults::HOSTNAME);
      } else if (now - _attemptStartedMs > JOIN_TIMEOUT_MS) {
        _state = State::Failed;
        _retryAtMs = now + RETRY_MS;
        Serial.printf("[wifi] Not joined: %s (code %u). Trying again in %u s. The access point is still up.\n",
                      describeReason(lastDisconnectReason), lastDisconnectReason,
                      static_cast<unsigned>(RETRY_MS / 1000));
      }
      break;

    case State::Connected:
      if (!connected) {
        // The Wi-Fi library reconnects by itself; this only tracks it.
        _state = State::Connecting;
        _attemptStartedMs = now;
        Serial.println(F("[wifi] Lost the network; reconnecting. The access point is still up."));
      }
      break;

    case State::Failed:
      if (connected) {
        _state = State::Connected;
        Serial.printf("[wifi] Joined the network. Interface address: http://%s\n",
                      WiFi.localIP().toString().c_str());
      } else if (static_cast<int32_t>(now - _retryAtMs) >= 0) {
        startStation();
      }
      break;
  }
}

void WifiManager::collectScan(int16_t found) {
  Network list[MAX_NETWORKS];
  size_t count = 0;

  for (int16_t i = 0; i < found; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || ssid.length() > eye::rules::SSID_MAX) {
      continue;
    }
    const int8_t rssi = static_cast<int8_t>(constrain(WiFi.RSSI(i), -127, 0));

    // A network with several transmitters is listed once, at its strongest.
    size_t slot = count;
    for (size_t j = 0; j < count; j++) {
      if (ssid == list[j].ssid) {
        slot = j;
        break;
      }
    }
    if (slot == count) {
      if (count == MAX_NETWORKS) {
        continue;
      }
      count++;
    } else if (rssi <= list[slot].rssi) {
      continue;
    }
    strlcpy(list[slot].ssid, ssid.c_str(), sizeof(list[slot].ssid));
    list[slot].rssi = rssi;
    list[slot].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    list[slot].channel = static_cast<uint8_t>(WiFi.channel(i));
  }
  WiFi.scanDelete();

  // Strongest first; the list is short, so a simple sort will do.
  for (size_t i = 1; i < count; i++) {
    const Network moving = list[i];
    size_t j = i;
    while (j > 0 && list[j - 1].rssi < moving.rssi) {
      list[j] = list[j - 1];
      j--;
    }
    list[j] = moving;
  }

  {
    Lock lock(_mutex);
    memcpy(_networks, list, sizeof(list));
    _networkCount = count;
  }
  _scanRunning = false;
  _scanned = true;
}

// ---- Any-task interface ---------------------------------------------------------

WifiManager::Station WifiManager::station() const {
  Lock lock(_mutex);
  return _station;
}

WifiManager::AccessPoint WifiManager::accessPoint() const {
  Lock lock(_mutex);
  return _ap;
}

bool WifiManager::accessPointPasswordIsDefault() const {
  Lock lock(_mutex);
  return strcmp(_ap.password, defaults::AP_PASSWORD) == 0;
}

const char* WifiManager::problem() const {
  if (_state == State::Failed || (_state == State::Connecting && lastDisconnectReason != 0)) {
    return describeReason(lastDisconnectReason);
  }
  return "";
}

void WifiManager::join(const Station& station) {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.putString(KEY_SSID, station.ssid);
    prefs.putString(KEY_PASSWORD, station.password);
    prefs.putBool(KEY_DHCP, station.dhcp);
    prefs.putUInt(KEY_IP, station.fixed.ip);
    prefs.putUInt(KEY_MASK, station.fixed.mask);
    prefs.putUInt(KEY_GATEWAY, station.fixed.gateway);
    prefs.putUInt(KEY_DNS1, station.fixed.dns1);
    prefs.putUInt(KEY_DNS2, station.fixed.dns2);
    prefs.end();
  } else {
    Serial.println(F("[wifi] Could not save the network to flash; it will be used until the next restart."));
  }
  {
    Lock lock(_mutex);
    _station = station;
    _station.configured = true;
    _station.fromBuild = false;
  }
  Serial.println(F("[wifi] Network saved from the interface; joining."));
  // Shown as "connecting" from this moment, not from when loop() gets to it.
  _state = State::Connecting;
  _attemptStartedMs = millis();
  lastDisconnectReason = 0;
  _applyStation = true;
}

void WifiManager::forget() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    for (const char* key : {KEY_SSID, KEY_PASSWORD, KEY_DHCP, KEY_IP, KEY_MASK, KEY_GATEWAY, KEY_DNS1, KEY_DNS2}) {
      prefs.remove(key);
    }
    prefs.end();
  }
  {
    Lock lock(_mutex);
    _station = {};
    _station.dhcp = true;
  }
  Serial.println(F("[wifi] Saved network forgotten."));
  _state = State::NotConfigured;
  _applyStation = true;
}

void WifiManager::setAccessPoint(const AccessPoint& accessPoint) {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.putString(KEY_AP_SSID, accessPoint.ssid);
    prefs.putString(KEY_AP_PASSWORD, accessPoint.password);
    prefs.end();
  }
  {
    Lock lock(_mutex);
    _ap = accessPoint;
  }
  _applyAccessPointAtMs = millis() + ACCESS_POINT_APPLY_DELAY_MS;
  _applyAccessPoint = true;
}

void WifiManager::eraseSaved() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.clear();
    prefs.end();
  }
}

void WifiManager::startScan() {
  if (!_scanRunning) {
    _scanWanted = true;
  }
}

size_t WifiManager::networks(Network out[MAX_NETWORKS]) const {
  Lock lock(_mutex);
  memcpy(out, _networks, sizeof(Network) * _networkCount);
  return _networkCount;
}
