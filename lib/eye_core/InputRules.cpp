#include "InputRules.h"

#include <stdio.h>
#include <string.h>

namespace eye {
namespace rules {

namespace {

bool hasControlCharacter(const char* text) {
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0'; p++) {
    if (*p < 0x20 || *p == 0x7F) {
      return true;
    }
  }
  return false;
}

// True if the mask is a run of ones followed by a run of zeros.
bool contiguous(uint32_t mask) {
  const uint32_t inverted = ~mask;
  return (inverted & (inverted + 1)) == 0;
}

}  // namespace

const char* checkUsername(const char* username) {
  if (username == nullptr) {
    return "user name is required";
  }
  const size_t length = strlen(username);
  if (length < USERNAME_MIN || length > USERNAME_MAX) {
    return "user name must be 3 to 32 characters";
  }
  if (hasControlCharacter(username)) {
    return "user name must not contain control characters";
  }
  if (username[0] == ' ' || username[length - 1] == ' ') {
    return "user name must not start or end with a space";
  }
  return nullptr;
}

const char* checkPassword(const char* password) {
  if (password == nullptr) {
    return "password is required";
  }
  const size_t length = strlen(password);
  if (length < PASSWORD_MIN || length > PASSWORD_MAX) {
    return "password must be 8 to 64 characters";
  }
  if (hasControlCharacter(password)) {
    return "password must not contain control characters";
  }
  return nullptr;
}

const char* checkNetworkName(const char* ssid) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return "network name is required";
  }
  if (strlen(ssid) > SSID_MAX) {
    return "network name must be at most 32 characters";
  }
  if (hasControlCharacter(ssid)) {
    return "network name must not contain control characters";
  }
  return nullptr;
}

const char* checkStationPassword(const char* password) {
  if (password == nullptr || password[0] == '\0') {
    return nullptr;  // open network
  }
  const size_t length = strlen(password);
  if (length < WIFI_PASSWORD_MIN || length > WIFI_PASSWORD_MAX) {
    return "network password must be 8 to 63 characters, or empty for an open network";
  }
  if (hasControlCharacter(password)) {
    return "network password must not contain control characters";
  }
  return nullptr;
}

const char* checkAccessPointPassword(const char* password) {
  if (password == nullptr) {
    return "access point password is required";
  }
  const size_t length = strlen(password);
  if (length < WIFI_PASSWORD_MIN || length > WIFI_PASSWORD_MAX) {
    return "access point password must be 8 to 63 characters";
  }
  if (hasControlCharacter(password)) {
    return "access point password must not contain control characters";
  }
  return nullptr;
}

bool parseIpv4(const char* text, uint32_t& address) {
  if (text == nullptr) {
    return false;
  }
  uint32_t result = 0;
  const char* p = text;
  for (int part = 0; part < 4; part++) {
    if (*p < '0' || *p > '9') {
      return false;
    }
    if (*p == '0' && p[1] >= '0' && p[1] <= '9') {
      return false;  // "01" is ambiguous: some systems read it as octal
    }
    uint32_t value = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + static_cast<uint32_t>(*p - '0');
      if (++digits > 3 || value > 255) {
        return false;
      }
      p++;
    }
    result = (result << 8) | value;
    if (part < 3) {
      if (*p != '.') {
        return false;
      }
      p++;
    }
  }
  if (*p != '\0') {
    return false;
  }
  address = result;
  return true;
}

void formatIpv4(uint32_t address, char out[IPV4_TEXT_SIZE]) {
  snprintf(out, IPV4_TEXT_SIZE, "%u.%u.%u.%u", static_cast<unsigned>((address >> 24) & 0xFF),
           static_cast<unsigned>((address >> 16) & 0xFF), static_cast<unsigned>((address >> 8) & 0xFF),
           static_cast<unsigned>(address & 0xFF));
}

const char* checkStaticAddress(const StaticAddress& a) {
  if (a.mask == 0 || !contiguous(a.mask)) {
    return "subnet mask is not valid (for example 255.255.255.0)";
  }
  if (a.mask > 0xFFFFFFFCu) {
    return "subnet mask leaves no room for a device and a gateway";
  }
  const uint32_t network = a.ip & a.mask;
  const uint32_t broadcast = network | ~a.mask;

  if (a.ip == 0 || (a.ip >> 24) == 127 || (a.ip >> 24) >= 224) {
    return "IP address is not one a device can use";
  }
  if (a.ip == network || a.ip == broadcast) {
    return "IP address is the network or broadcast address of that subnet";
  }
  if (a.gateway == 0) {
    return "gateway is required";
  }
  if ((a.gateway & a.mask) != network) {
    return "gateway is not in the same subnet as the IP address";
  }
  if (a.gateway == network || a.gateway == broadcast) {
    return "gateway is the network or broadcast address of that subnet";
  }
  if (a.gateway == a.ip) {
    return "IP address and gateway must be different";
  }

  // Two subnets overlap if they agree on every bit that both masks cover.
  const uint32_t common = a.mask & ACCESS_POINT_MASK;
  if ((network & common) == (ACCESS_POINT_NETWORK & common)) {
    return "address range overlaps 192.168.4.x, which the eye's own access point uses";
  }
  return nullptr;
}

uint32_t LockoutCounter::lockedForMs(uint32_t nowMs) {
  if (!_locked) {
    return 0;
  }
  const uint32_t elapsed = nowMs - _lockedAtMs;  // unsigned: correct across wrap-around
  if (elapsed >= _lockMs) {
    _locked = false;
    _failures = 0;
    return 0;
  }
  return _lockMs - elapsed;
}

void LockoutCounter::recordFailure(uint32_t nowMs) {
  if (lockedForMs(nowMs) > 0) {
    return;
  }
  if (++_failures >= _maxFailures) {
    _locked = true;
    _lockedAtMs = nowMs;
  }
}

void LockoutCounter::recordSuccess() {
  _failures = 0;
  _locked = false;
}

}  // namespace rules
}  // namespace eye
