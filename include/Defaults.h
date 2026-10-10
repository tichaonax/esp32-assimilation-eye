// Factory defaults for everything that is not an LED setting.
// LED setting defaults are in lib/eye_core/EyeConfig.cpp.
#pragma once

namespace defaults {

// The prop's own Wi-Fi network. Always on, so there is always a way in.
constexpr char AP_SSID[] = "AssimilationEye";
constexpr char AP_PASSWORD[] = "resistanceisfutile";  // WPA2 needs 8 to 63 characters

// Name the prop gives itself on a network it joins.
constexpr char HOSTNAME[] = "assimilation-eye";

}  // namespace defaults
