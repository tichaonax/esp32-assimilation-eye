// Wi-Fi: the prop's own access point, which is always on, and an optional
// connection to an existing network ("station"), set up from the interface.
//
//   - The access point means there is always a way in, whatever happens to
//     the joined network or its settings.
//   - The joined network can use DHCP or a static address.
//   - A saved network that is missing, or that refuses the password, is
//     retried in the background.
//   - When joined, the board also answers at http://assimilation-eye.local
//
// Development only: a firmware built with a platformio.local.ini uses that
// network when none has been saved from the interface.
//
// Radio operations happen in loop(), on the main task. Everything else is safe
// to call from any task: requests are noted and carried out by loop().
#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "InputRules.h"

class WifiManager {
 public:
  static constexpr size_t SSID_SIZE = eye::rules::SSID_MAX + 1;
  static constexpr size_t PASSWORD_SIZE = eye::rules::WIFI_PASSWORD_MAX + 1;
  static constexpr size_t MAX_NETWORKS = 20;
  static constexpr uint32_t JOIN_TIMEOUT_MS = 20000;
  static constexpr uint32_t RETRY_MS = 30000;

  struct Station {
    bool configured;
    bool fromBuild;  // came from platformio.local.ini, not from the interface
    char ssid[SSID_SIZE];
    char password[PASSWORD_SIZE];  // empty for an open network
    bool dhcp;
    eye::rules::StaticAddress fixed;  // used when dhcp is false
  };

  struct AccessPoint {
    char ssid[SSID_SIZE];
    char password[PASSWORD_SIZE];
  };

  enum class State : uint8_t { NotConfigured, Connecting, Connected, Failed };

  struct Network {
    char ssid[SSID_SIZE];
    int8_t rssi;
    bool secure;
    uint8_t channel;
  };

  // Starts the access point and, if a network is saved, starts joining it.
  bool begin();

  void loop();

  Station station() const;
  AccessPoint accessPoint() const;
  bool accessPointPasswordIsDefault() const;
  State state() const { return _state; }
  // Why the last attempt to join failed, in plain words; "" if there is nothing to say.
  const char* problem() const;

  // Saves the network (already checked against the rules) and starts joining it.
  void join(const Station& station);

  // Forgets the saved network and leaves it.
  void forget();

  // Saves new access point details (already checked) and applies them a
  // moment later, so the reply reaches a browser that is connected through it.
  void setAccessPoint(const AccessPoint& accessPoint);

  // Wipes everything saved. Takes effect at the restart that follows.
  void eraseSaved();

  // Starts looking for nearby networks unless a search is already running.
  void startScan();
  bool scanning() const { return _scanWanted || _scanRunning; }
  // Networks found by the last search, strongest first. Returns how many.
  size_t networks(Network out[MAX_NETWORKS]) const;
  // False until the first search has finished.
  bool scanned() const { return _scanned; }

 private:
  void loadSaved();
  void startAccessPoint();
  void startStation();
  void collectScan(int16_t found);

  SemaphoreHandle_t _mutex = nullptr;
  Station _station = {};      // guarded by _mutex
  AccessPoint _ap = {};       // guarded by _mutex
  Network _networks[MAX_NETWORKS] = {};  // guarded by _mutex
  size_t _networkCount = 0;   // guarded by _mutex

  volatile State _state = State::NotConfigured;
  volatile bool _applyStation = false;
  volatile bool _applyAccessPoint = false;
  volatile uint32_t _applyAccessPointAtMs = 0;
  volatile bool _scanWanted = false;
  volatile bool _scanRunning = false;
  volatile bool _scanned = false;

  uint32_t _attemptStartedMs = 0;
  uint32_t _retryAtMs = 0;
  uint32_t _scanStartedMs = 0;
  bool _everConnected = false;
};
