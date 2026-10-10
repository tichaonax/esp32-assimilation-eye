// Wi-Fi. The prop always runs its own access point, so there is always a way
// to reach it.
//
// Development only: if the firmware was built with a platformio.local.ini
// (see platformio.local.ini.example), it also joins that network so the PC
// can run the API tests without leaving its own Wi-Fi. Joining a network
// chosen in the interface arrives in Phase 6.
#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
 public:
  // Starts the access point, and the development connection if built in.
  bool begin();

  // Call from loop(): reports connection changes and retries the development
  // connection while it is down.
  void loop();

  IPAddress accessPointIp() const;
  bool stationConnected() const;
  IPAddress stationIp() const;

 private:
  bool _stationWanted = false;
  bool _stationWasConnected = false;
  uint32_t _lastAttemptMs = 0;
};
