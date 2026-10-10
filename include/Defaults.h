// Factory defaults for everything that is not an LED setting.
// LED setting defaults are in lib/eye_core/EyeConfig.cpp.
//
// A factory reset (hold the BOOT button for ten seconds, or use the System
// screen) restores all of these.
#pragma once

namespace defaults {

// Sign-in to the interface. The interface warns while the password is unchanged.
constexpr char LOGIN_USERNAME[] = "admin";
constexpr char LOGIN_PASSWORD[] = "assimilate";

// The prop's own Wi-Fi network. Always on, so there is always a way in.
constexpr char AP_SSID[] = "AssimilationEye";
constexpr char AP_PASSWORD[] = "resistanceisfutile";  // WPA2 needs 8 to 63 characters

// Name the prop gives itself on a network it joins: http://assimilation-eye.local
constexpr char HOSTNAME[] = "assimilation-eye";

}  // namespace defaults
