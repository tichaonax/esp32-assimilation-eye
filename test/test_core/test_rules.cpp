// InputRules and FirmwareImage: sign-in details, network names and passwords,
// IPv4 parsing, static addressing, the sign-in lockout and the firmware check.

#include <string.h>

#include <string>

#include <unity.h>

#include "FirmwareImage.h"
#include "InputRules.h"
#include "tests.h"

using namespace eye::rules;

static uint32_t ip(const char* text) {
  uint32_t value = 0;
  TEST_ASSERT_TRUE_MESSAGE(parseIpv4(text, value), text);
  return value;
}

static StaticAddress address(const char* ipText, const char* mask, const char* gateway) {
  return StaticAddress{ip(ipText), ip(mask), ip(gateway), 0, 0};
}

// ---- Sign-in details --------------------------------------------------------

static void test_rules_username_length_and_shape() {
  TEST_ASSERT_NULL(checkUsername("admin"));
  TEST_ASSERT_NULL(checkUsername("abc"));
  TEST_ASSERT_NULL(checkUsername(std::string(32, 'u').c_str()));
  TEST_ASSERT_NULL(checkUsername("locutus of borg"));

  TEST_ASSERT_NOT_NULL(checkUsername(nullptr));
  TEST_ASSERT_NOT_NULL(checkUsername(""));
  TEST_ASSERT_NOT_NULL(checkUsername("ab"));
  TEST_ASSERT_NOT_NULL(checkUsername(std::string(33, 'u').c_str()));
  TEST_ASSERT_NOT_NULL(checkUsername(" admin"));
  TEST_ASSERT_NOT_NULL(checkUsername("admin "));
  TEST_ASSERT_NOT_NULL(checkUsername("ad\tmin"));
}

static void test_rules_password_length() {
  TEST_ASSERT_NULL(checkPassword("assimilate"));
  TEST_ASSERT_NULL(checkPassword("12345678"));
  TEST_ASSERT_NULL(checkPassword(std::string(64, 'p').c_str()));
  TEST_ASSERT_NULL(checkPassword("pass word with spaces"));

  TEST_ASSERT_NOT_NULL(checkPassword(nullptr));
  TEST_ASSERT_NOT_NULL(checkPassword("1234567"));
  TEST_ASSERT_NOT_NULL(checkPassword(std::string(65, 'p').c_str()));
  TEST_ASSERT_NOT_NULL(checkPassword("pass\nword1"));
}

// ---- Wi-Fi names and passwords ----------------------------------------------

static void test_rules_network_name() {
  TEST_ASSERT_NULL(checkNetworkName("AssimilationEye"));
  TEST_ASSERT_NULL(checkNetworkName("x"));
  TEST_ASSERT_NULL(checkNetworkName("A Network With Spaces"));
  TEST_ASSERT_NULL(checkNetworkName(std::string(32, 'n').c_str()));

  TEST_ASSERT_NOT_NULL(checkNetworkName(nullptr));
  TEST_ASSERT_NOT_NULL(checkNetworkName(""));
  TEST_ASSERT_NOT_NULL(checkNetworkName(std::string(33, 'n').c_str()));
}

static void test_rules_station_password_may_be_empty() {
  TEST_ASSERT_NULL(checkStationPassword(""));
  TEST_ASSERT_NULL(checkStationPassword(nullptr));
  TEST_ASSERT_NULL(checkStationPassword("12345678"));
  TEST_ASSERT_NULL(checkStationPassword(std::string(63, 'p').c_str()));

  TEST_ASSERT_NOT_NULL(checkStationPassword("1234567"));
  TEST_ASSERT_NOT_NULL(checkStationPassword(std::string(64, 'p').c_str()));
}

static void test_rules_access_point_password_is_required() {
  TEST_ASSERT_NULL(checkAccessPointPassword("resistanceisfutile"));
  TEST_ASSERT_NULL(checkAccessPointPassword(std::string(63, 'p').c_str()));

  TEST_ASSERT_NOT_NULL(checkAccessPointPassword(""));
  TEST_ASSERT_NOT_NULL(checkAccessPointPassword(nullptr));
  TEST_ASSERT_NOT_NULL(checkAccessPointPassword("1234567"));
  TEST_ASSERT_NOT_NULL(checkAccessPointPassword(std::string(64, 'p').c_str()));
}

// ---- IPv4 -------------------------------------------------------------------

static void test_rules_ipv4_parses_dotted_addresses() {
  uint32_t value = 0;
  TEST_ASSERT_TRUE(parseIpv4("192.168.1.50", value));
  TEST_ASSERT_EQUAL_HEX32(0xC0A80132, value);
  TEST_ASSERT_TRUE(parseIpv4("0.0.0.0", value));
  TEST_ASSERT_EQUAL_HEX32(0, value);
  TEST_ASSERT_TRUE(parseIpv4("255.255.255.255", value));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, value);
  TEST_ASSERT_TRUE(parseIpv4("10.0.0.1", value));
  TEST_ASSERT_EQUAL_HEX32(0x0A000001, value);
}

static void test_rules_ipv4_rejects_malformed_addresses() {
  const char* bad[] = {
      "",          "192.168.1",   "192.168.1.",   "192.168.1.1.1", "192.168.1.256", "192.168.1.-1", "192.168.01.1",
      "1.2.3.4 ",  " 1.2.3.4",    "1..2.3",       "a.b.c.d",       "1.2.3.4x",      "1234.1.1.1",   "192,168,1,1",
      "192.168.1", "0x10.1.1.1",  "1.2.3.+4",     "....",          "1.2.3.0004",
  };
  for (const char* text : bad) {
    uint32_t value = 0xDEADBEEF;
    TEST_ASSERT_FALSE_MESSAGE(parseIpv4(text, value), text);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xDEADBEEF, value, "a failed parse must not change the output");
  }
  uint32_t value = 0;
  TEST_ASSERT_FALSE(parseIpv4(nullptr, value));
}

static void test_rules_ipv4_formats_back_to_text() {
  char text[IPV4_TEXT_SIZE];
  formatIpv4(0xC0A80132, text);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", text);
  formatIpv4(0xFFFFFFFF, text);
  TEST_ASSERT_EQUAL_STRING("255.255.255.255", text);
  formatIpv4(0, text);
  TEST_ASSERT_EQUAL_STRING("0.0.0.0", text);
}

// ---- Static addressing ------------------------------------------------------

static void test_rules_static_accepts_ordinary_home_addresses() {
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "192.168.1.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.8.240", "255.255.255.0", "192.168.8.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("10.0.5.20", "255.0.0.0", "10.0.0.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("172.16.3.9", "255.255.240.0", "172.16.0.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.1.2", "255.255.255.252", "192.168.1.1")));
}

static void test_rules_static_rejects_bad_masks() {
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "0.0.0.0", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.0.255.0", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.1", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "0.255.255.255", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.255", "192.168.1.50")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.254", "192.168.1.51")));
}

static void test_rules_static_rejects_unusable_addresses() {
  // Network and broadcast addresses.
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.0", "255.255.255.0", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.255", "255.255.255.0", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "192.168.1.0")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "192.168.1.255")));
  // Loopback, multicast, nothing.
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("127.0.0.5", "255.0.0.0", "127.0.0.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("224.0.0.5", "255.255.255.0", "224.0.0.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("0.0.0.0", "255.255.255.0", "0.0.0.1")));
}

static void test_rules_static_gateway_must_be_a_different_host_in_the_subnet() {
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "192.168.2.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "192.168.1.50")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.255.0", "0.0.0.0")));
  TEST_ASSERT_EQUAL_STRING("gateway is not in the same subnet as the IP address",
                           checkStaticAddress(address("192.168.1.50", "255.255.255.0", "10.0.0.1")));
}

static void test_rules_static_must_not_overlap_the_access_point_network() {
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.4.50", "255.255.255.0", "192.168.4.1")));
  // A wider subnet that contains 192.168.4.x.
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.1.50", "255.255.0.0", "192.168.1.1")));
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.5.50", "255.255.252.0", "192.168.5.1")));
  // A narrower subnet inside it.
  TEST_ASSERT_NOT_NULL(checkStaticAddress(address("192.168.4.130", "255.255.255.128", "192.168.4.129")));
  // Neighbours that do not overlap.
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.5.50", "255.255.255.0", "192.168.5.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.3.50", "255.255.255.0", "192.168.3.1")));
  TEST_ASSERT_NULL(checkStaticAddress(address("192.168.8.50", "255.255.252.0", "192.168.8.1")));
}

// ---- Lockout ----------------------------------------------------------------

static void test_lockout_allows_four_failures() {
  LockoutCounter lock;
  for (int i = 0; i < 4; i++) {
    lock.recordFailure(1000 + i);
    TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(1000 + i));
  }
}

static void test_lockout_locks_on_the_fifth_failure_for_thirty_seconds() {
  LockoutCounter lock;
  for (int i = 0; i < 5; i++) {
    lock.recordFailure(1000);
  }
  TEST_ASSERT_EQUAL_UINT32(30000, lock.lockedForMs(1000));
  TEST_ASSERT_EQUAL_UINT32(20000, lock.lockedForMs(11000));
  TEST_ASSERT_EQUAL_UINT32(1, lock.lockedForMs(30999));
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(31000));
}

static void test_lockout_success_clears_the_count() {
  LockoutCounter lock;
  for (int i = 0; i < 4; i++) {
    lock.recordFailure(1000);
  }
  lock.recordSuccess();
  for (int i = 0; i < 4; i++) {
    lock.recordFailure(2000);
  }
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(2000));
}

static void test_lockout_attempts_while_locked_do_not_extend_it() {
  LockoutCounter lock;
  for (int i = 0; i < 5; i++) {
    lock.recordFailure(1000);
  }
  for (int i = 0; i < 20; i++) {
    lock.recordFailure(15000);
  }
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(31000));
}

static void test_lockout_starts_a_fresh_count_after_it_expires() {
  LockoutCounter lock;
  for (int i = 0; i < 5; i++) {
    lock.recordFailure(1000);
  }
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(40000));
  for (int i = 0; i < 4; i++) {
    lock.recordFailure(40000);
  }
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(40000));
  lock.recordFailure(40000);
  TEST_ASSERT_EQUAL_UINT32(30000, lock.lockedForMs(40000));
}

static void test_lockout_survives_the_clock_wrapping_round() {
  LockoutCounter lock;
  const uint32_t nearWrap = 0xFFFFFFFFu - 10000;  // 10 s before the 49-day wrap
  for (int i = 0; i < 5; i++) {
    lock.recordFailure(nearWrap);
  }
  TEST_ASSERT_EQUAL_UINT32(20000, lock.lockedForMs(nearWrap + 10000));  // just before the wrap
  TEST_ASSERT_EQUAL_UINT32(15000, lock.lockedForMs(4999));              // 15 s in, after it
  TEST_ASSERT_EQUAL_UINT32(0, lock.lockedForMs(20000));
}

// ---- Firmware image ---------------------------------------------------------

static const size_t SLOT = 1966080;

static void validHeader(uint8_t out[64]) {
  memset(out, 0, 64);
  out[0] = eye::firmware::IMAGE_MAGIC;
  out[32] = 0x32;
  out[33] = 0x54;
  out[34] = 0xCD;
  out[35] = 0xAB;
}

static void test_firmware_accepts_an_application_image() {
  uint8_t head[64];
  validHeader(head);
  TEST_ASSERT_NULL(eye::firmware::check(head, sizeof(head), 1000000, SLOT));
  TEST_ASSERT_NULL(eye::firmware::check(head, eye::firmware::HEADER_BYTES, SLOT, SLOT));
}

static void test_firmware_rejects_the_wrong_kind_of_file() {
  uint8_t head[64];

  validHeader(head);
  head[0] = 0x50;  // not an ESP image at all
  TEST_ASSERT_NOT_NULL(eye::firmware::check(head, sizeof(head), 1000000, SLOT));

  validHeader(head);
  head[12] = 0x09;  // ESP32-S3
  TEST_ASSERT_NOT_NULL(eye::firmware::check(head, sizeof(head), 1000000, SLOT));

  validHeader(head);
  head[32] = 0x50;  // a bootloader, as at the start of firmware.factory.bin
  TEST_ASSERT_NOT_NULL(eye::firmware::check(head, sizeof(head), 1000000, SLOT));
}

static void test_firmware_rejects_files_that_are_too_big_or_too_small() {
  uint8_t head[64];
  validHeader(head);
  TEST_ASSERT_NOT_NULL(eye::firmware::check(head, sizeof(head), SLOT + 1, SLOT));
  TEST_ASSERT_NOT_NULL(eye::firmware::check(head, eye::firmware::HEADER_BYTES - 1, 20, SLOT));
  TEST_ASSERT_NOT_NULL(eye::firmware::check(nullptr, 0, 0, SLOT));
}

void runRulesTests() {
  RUN_TEST(test_rules_username_length_and_shape);
  RUN_TEST(test_rules_password_length);
  RUN_TEST(test_rules_network_name);
  RUN_TEST(test_rules_station_password_may_be_empty);
  RUN_TEST(test_rules_access_point_password_is_required);
  RUN_TEST(test_rules_ipv4_parses_dotted_addresses);
  RUN_TEST(test_rules_ipv4_rejects_malformed_addresses);
  RUN_TEST(test_rules_ipv4_formats_back_to_text);
  RUN_TEST(test_rules_static_accepts_ordinary_home_addresses);
  RUN_TEST(test_rules_static_rejects_bad_masks);
  RUN_TEST(test_rules_static_rejects_unusable_addresses);
  RUN_TEST(test_rules_static_gateway_must_be_a_different_host_in_the_subnet);
  RUN_TEST(test_rules_static_must_not_overlap_the_access_point_network);
  RUN_TEST(test_lockout_allows_four_failures);
  RUN_TEST(test_lockout_locks_on_the_fifth_failure_for_thirty_seconds);
  RUN_TEST(test_lockout_success_clears_the_count);
  RUN_TEST(test_lockout_attempts_while_locked_do_not_extend_it);
  RUN_TEST(test_lockout_starts_a_fresh_count_after_it_expires);
  RUN_TEST(test_lockout_survives_the_clock_wrapping_round);
  RUN_TEST(test_firmware_accepts_an_application_image);
  RUN_TEST(test_firmware_rejects_the_wrong_kind_of_file);
  RUN_TEST(test_firmware_rejects_files_that_are_too_big_or_too_small);
}
