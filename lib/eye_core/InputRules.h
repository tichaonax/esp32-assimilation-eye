// Rules for everything a person types into the system screens: sign-in
// details, network names and passwords, and static network addresses. Also the
// counter that locks sign-in after repeated failures.
//
// Unlike LED settings, these are never clamped: a value that breaks a rule is
// refused with a message saying why.
//
// The check functions return nullptr when the value is acceptable, otherwise a
// plain-language message that completes the sentence "The <thing> ...".
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace eye {
namespace rules {

constexpr size_t USERNAME_MIN = 3;
constexpr size_t USERNAME_MAX = 32;
constexpr size_t PASSWORD_MIN = 8;
constexpr size_t PASSWORD_MAX = 64;
constexpr size_t SSID_MAX = 32;
constexpr size_t WIFI_PASSWORD_MIN = 8;  // WPA2
constexpr size_t WIFI_PASSWORD_MAX = 63;

// The prop's own access point network. A static address on a joined network
// must not fall in it, or the board could not tell the two networks apart.
constexpr uint32_t ACCESS_POINT_NETWORK = (192u << 24) | (168u << 16) | (4u << 8);
constexpr uint32_t ACCESS_POINT_MASK = 0xFFFFFF00u;

const char* checkUsername(const char* username);
const char* checkPassword(const char* password);
const char* checkNetworkName(const char* ssid);
// A network joined as a client: no password (open network) or 8 to 63 characters.
const char* checkStationPassword(const char* password);
// The prop's own network always has a password.
const char* checkAccessPointPassword(const char* password);

// ---- IPv4 -------------------------------------------------------------------

// Addresses are held as one number, first part in the top byte:
// 192.168.1.50 = 0xC0A80132.

// Strict dotted form: four parts of 0 to 255, no spaces, signs or extra zeros.
bool parseIpv4(const char* text, uint32_t& address);

constexpr size_t IPV4_TEXT_SIZE = 16;
void formatIpv4(uint32_t address, char out[IPV4_TEXT_SIZE]);

struct StaticAddress {
  uint32_t ip;
  uint32_t mask;
  uint32_t gateway;
  uint32_t dns1;  // 0 = not given; the gateway is used
  uint32_t dns2;  // 0 = not given
};

// The mask must be contiguous and leave room for hosts; the address and
// gateway must be usable hosts in the same subnet and differ from each other;
// the subnet must not overlap the prop's access point network.
const char* checkStaticAddress(const StaticAddress& address);

// ---- Sign-in lockout --------------------------------------------------------

// After maxFailures wrong attempts in a row, sign-in is refused for lockMs.
// Times are milliseconds from any steady clock; wrap-around is handled.
class LockoutCounter {
 public:
  explicit LockoutCounter(uint32_t maxFailures = 5, uint32_t lockMs = 30000)
      : _maxFailures(maxFailures), _lockMs(lockMs) {}

  // Milliseconds until sign-in is allowed again; 0 if it is allowed now.
  uint32_t lockedForMs(uint32_t nowMs);

  void recordFailure(uint32_t nowMs);
  void recordSuccess();

  uint32_t failures() const { return _failures; }

 private:
  uint32_t _maxFailures;
  uint32_t _lockMs;
  uint32_t _failures = 0;
  bool _locked = false;
  uint32_t _lockedAtMs = 0;
};

}  // namespace rules
}  // namespace eye
