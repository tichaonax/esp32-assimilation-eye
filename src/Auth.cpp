#include "Auth.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include "Defaults.h"

namespace {

constexpr char NVS_NAMESPACE[] = "auth";
constexpr char KEY_USER[] = "user";
constexpr char KEY_SALT[] = "salt";
constexpr char KEY_HASH[] = "hash";
constexpr char KEY_ITERATIONS[] = "iter";
constexpr char KEY_DEFAULT[] = "dflt";

// Compares without stopping at the first difference, so the time taken says
// nothing about how much of a guess was right.
bool sameBytes(const uint8_t* a, const uint8_t* b, size_t length) {
  uint8_t difference = 0;
  for (size_t i = 0; i < length; i++) {
    difference |= a[i] ^ b[i];
  }
  return difference == 0;
}

bool sameText(const char* a, const char* b) {
  const size_t lengthA = strlen(a);
  const size_t lengthB = strlen(b);
  uint8_t difference = lengthA == lengthB ? 0 : 1;
  for (size_t i = 0; i < lengthA; i++) {
    difference |= static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i % (lengthB + 1)]);
  }
  return difference == 0;
}

bool derive(const char* password, const uint8_t* salt, size_t saltBytes, uint32_t iterations, uint8_t* out,
            size_t outBytes) {
  return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char*>(password),
                                       strlen(password), salt, saltBytes, iterations, outBytes, out) == 0;
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

void Auth::begin() {
  _mutex = xSemaphoreCreateMutex();
  if (!load()) {
    store(defaults::LOGIN_USERNAME, defaults::LOGIN_PASSWORD);
    Serial.println(F("[auth] Created the factory default sign-in."));
  }
  const uint32_t started = millis();
  passwordMatches("");
  Serial.printf("[auth] Sign-in user \"%s\"%s. A password check takes %u ms.\n", _username,
                _defaultPassword ? ", DEFAULT PASSWORD still in use" : "", static_cast<unsigned>(millis() - started));
}

bool Auth::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    return false;
  }
  const String user = prefs.isKey(KEY_USER) ? prefs.getString(KEY_USER) : String();
  const bool complete = prefs.getBytesLength(KEY_SALT) == SALT_BYTES && prefs.getBytesLength(KEY_HASH) == HASH_BYTES;
  bool ok = false;
  if (complete && eye::rules::checkUsername(user.c_str()) == nullptr) {
    prefs.getBytes(KEY_SALT, _salt, SALT_BYTES);
    prefs.getBytes(KEY_HASH, _hash, HASH_BYTES);
    _iterations = prefs.getUInt(KEY_ITERATIONS, HASH_ITERATIONS);
    _defaultPassword = prefs.getBool(KEY_DEFAULT, false);
    strlcpy(_username, user.c_str(), sizeof(_username));
    ok = _iterations > 0;
  }
  prefs.end();
  return ok;
}

bool Auth::store(const char* username, const char* password) {
  uint8_t salt[SALT_BYTES];
  uint8_t hash[HASH_BYTES];
  esp_fill_random(salt, sizeof(salt));
  if (!derive(password, salt, sizeof(salt), HASH_ITERATIONS, hash, sizeof(hash))) {
    return false;
  }
  const bool isDefault = strcmp(password, defaults::LOGIN_PASSWORD) == 0;

  Preferences prefs;
  bool saved = false;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    saved = prefs.putString(KEY_USER, username) > 0 && prefs.putBytes(KEY_SALT, salt, sizeof(salt)) == sizeof(salt) &&
            prefs.putBytes(KEY_HASH, hash, sizeof(hash)) == sizeof(hash) &&
            prefs.putUInt(KEY_ITERATIONS, HASH_ITERATIONS) > 0 && prefs.putBool(KEY_DEFAULT, isDefault) > 0;
    prefs.end();
  }
  if (!saved) {
    Serial.println(F("[auth] Could not write the sign-in details to flash."));
  }

  // Use them even if the write failed, so the account works until a restart.
  strlcpy(_username, username, sizeof(_username));
  memcpy(_salt, salt, sizeof(_salt));
  memcpy(_hash, hash, sizeof(_hash));
  _iterations = HASH_ITERATIONS;
  _defaultPassword = isDefault;
  return saved;
}

bool Auth::passwordMatches(const char* password) const {
  // A far longer input than any allowed password is not worth hashing.
  if (strlen(password) > eye::rules::PASSWORD_MAX * 2) {
    password = "";
  }
  uint8_t candidate[HASH_BYTES];
  if (!derive(password, _salt, SALT_BYTES, _iterations, candidate, sizeof(candidate))) {
    return false;
  }
  return sameBytes(candidate, _hash, HASH_BYTES);
}

Auth::Session* Auth::find(const char* token) {
  if (token == nullptr || strlen(token) != TOKEN_SIZE - 1) {
    return nullptr;
  }
  const uint32_t now = millis();
  Session* match = nullptr;
  for (Session& session : _sessions) {
    if (!session.used) {
      continue;
    }
    if (now - session.lastUsedMs > SESSION_IDLE_MS) {
      session.used = false;
      continue;
    }
    // Every session is compared, and compared in full.
    if (sameBytes(reinterpret_cast<const uint8_t*>(session.token), reinterpret_cast<const uint8_t*>(token),
                  TOKEN_SIZE - 1)) {
      match = &session;
    }
  }
  return match;
}

void Auth::endAllSessionsExcept(const char* keepToken) {
  Session* keep = find(keepToken);
  for (Session& session : _sessions) {
    if (&session != keep) {
      session.used = false;
    }
  }
}

Auth::LoginResult Auth::login(const char* username, const char* password, char token[TOKEN_SIZE],
                              uint32_t& retryAfterS) {
  Lock lock(_mutex);
  retryAfterS = 0;

  const uint32_t lockedMs = _lockout.lockedForMs(millis());
  if (lockedMs > 0) {
    retryAfterS = (lockedMs + 999) / 1000;
    return LoginResult::LockedOut;
  }

  // Both checks always run, so a wrong user name takes as long as a wrong password.
  const bool passwordOk = passwordMatches(password);
  const bool usernameOk = sameText(username, _username);
  if (!(passwordOk && usernameOk)) {
    _lockout.recordFailure(millis());
    return LoginResult::WrongCredentials;
  }
  _lockout.recordSuccess();

  // The one moment the password itself is in hand: re-hash it if it was
  // stored with a different number of rounds than this firmware uses.
  if (_iterations != HASH_ITERATIONS) {
    store(_username, password);
  }

  // Take a free slot, or the one that has been idle longest.
  const uint32_t now = millis();
  Session* slot = &_sessions[0];
  for (Session& session : _sessions) {
    if (!session.used) {
      slot = &session;
      break;
    }
    if (now - session.lastUsedMs > now - slot->lastUsedMs) {
      slot = &session;
    }
  }

  uint8_t raw[TOKEN_BYTES];
  esp_fill_random(raw, sizeof(raw));
  for (size_t i = 0; i < TOKEN_BYTES; i++) {
    snprintf(slot->token + i * 2, 3, "%02x", raw[i]);
  }
  slot->used = true;
  slot->lastUsedMs = now;
  memcpy(token, slot->token, TOKEN_SIZE);
  return LoginResult::Ok;
}

bool Auth::sessionValid(const char* token) {
  Lock lock(_mutex);
  Session* session = find(token);
  if (session == nullptr) {
    return false;
  }
  session->lastUsedMs = millis();
  return true;
}

void Auth::logout(const char* token) {
  Lock lock(_mutex);
  Session* session = find(token);
  if (session != nullptr) {
    session->used = false;
  }
}

bool Auth::confirmPassword(const char* password) {
  Lock lock(_mutex);
  if (_lockout.lockedForMs(millis()) > 0) {
    return false;
  }
  if (!passwordMatches(password)) {
    _lockout.recordFailure(millis());
    return false;
  }
  _lockout.recordSuccess();
  return true;
}

bool Auth::changeCredentials(const char* username, const char* password, const char* keepToken) {
  Lock lock(_mutex);
  const bool saved = store(username, password);
  endAllSessionsExcept(keepToken);
  return saved;
}

void Auth::factoryReset() {
  Lock lock(_mutex);
  store(defaults::LOGIN_USERNAME, defaults::LOGIN_PASSWORD);
  endAllSessionsExcept(nullptr);
  _lockout.recordSuccess();
}

String Auth::username() {
  Lock lock(_mutex);
  return String(_username);
}

bool Auth::usingDefaultPassword() {
  Lock lock(_mutex);
  return _defaultPassword;
}

uint32_t Auth::lockedForS() {
  Lock lock(_mutex);
  return (_lockout.lockedForMs(millis()) + 999) / 1000;
}
