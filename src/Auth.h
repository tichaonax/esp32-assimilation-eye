// Sign-in for the interface: one administrator account.
//
//   - The password is never stored. Flash (NVS) holds a random salt and the
//     PBKDF2-HMAC-SHA256 result, so reading the flash does not reveal it.
//   - Signing in gives the browser a random session token in a cookie.
//     Sessions live in memory: a restart signs everyone out.
//   - Five wrong attempts in a row lock sign-in for 30 seconds.
//
// The login runs over plain HTTP, as specified: on a shared network the
// password and the session cookie can be read by anyone able to capture the
// traffic. On the prop's own WPA2 access point the radio link is encrypted.
//
// Everything here is safe to call from any task after begin().
#pragma once

#include <Arduino.h>

#include "InputRules.h"

class Auth {
 public:
  static constexpr size_t TOKEN_BYTES = 32;
  static constexpr size_t TOKEN_SIZE = TOKEN_BYTES * 2 + 1;  // hex, with terminator
  static constexpr size_t MAX_SESSIONS = 8;
  static constexpr uint32_t SESSION_IDLE_MS = 12UL * 60 * 60 * 1000;
  // Each sign-in costs this many rounds, during which the web server answers
  // nothing else. 2,000 takes about a fifth of a second on this chip; 10,000
  // took over a second. The count is stored with the hash, and a stored
  // account is brought up to the current count at its next sign-in.
  static constexpr uint32_t HASH_ITERATIONS = 2000;

  enum class LoginResult { Ok, WrongCredentials, LockedOut };

  // Loads the stored account, or creates the factory default one.
  void begin();

  // On Ok, token receives the new session's token. On LockedOut, retryAfterS
  // says how long to wait.
  LoginResult login(const char* username, const char* password, char token[TOKEN_SIZE], uint32_t& retryAfterS);

  // True if the token belongs to a live session; also keeps it alive.
  bool sessionValid(const char* token);

  void logout(const char* token);

  // Checks the current password, for actions that ask for it again. A wrong
  // answer counts towards the lockout; returns false while locked out.
  bool confirmPassword(const char* password);

  // Stores new sign-in details (already checked against the rules) and ends
  // every session except keepToken's. Returns false if flash could not be written.
  bool changeCredentials(const char* username, const char* password, const char* keepToken);

  // Back to the factory default account, with every session ended.
  void factoryReset();

  String username();
  bool usingDefaultPassword();
  uint32_t lockedForS();

 private:
  static constexpr size_t SALT_BYTES = 16;
  static constexpr size_t HASH_BYTES = 32;

  struct Session {
    bool used;
    char token[TOKEN_SIZE];
    uint32_t lastUsedMs;
  };

  bool load();
  bool store(const char* username, const char* password);
  bool passwordMatches(const char* password) const;
  Session* find(const char* token);
  void endAllSessionsExcept(const char* keepToken);

  SemaphoreHandle_t _mutex = nullptr;
  char _username[eye::rules::USERNAME_MAX + 1] = {};
  uint8_t _salt[SALT_BYTES] = {};
  uint8_t _hash[HASH_BYTES] = {};
  uint32_t _iterations = HASH_ITERATIONS;
  bool _defaultPassword = true;
  Session _sessions[MAX_SESSIONS] = {};
  eye::rules::LockoutCounter _lockout;
};
