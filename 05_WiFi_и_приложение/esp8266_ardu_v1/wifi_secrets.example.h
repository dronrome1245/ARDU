#pragma once

// Copy this file to wifi_secrets.h in the same sketch folder.
// Do NOT commit wifi_secrets.h.
//
// The real values stay local so the tracked esp8266_ardu_v1.ino remains clean
// across git pull / branch switching.

#define ARDU_WIFI_SSID "YOUR_WIFI_SSID"
#define ARDU_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define ARDU_OTA_PASSWORD "YOUR_OTA_PASSWORD"

// Minimum 8 characters for ESP8266 SoftAP WPA2.
#define ARDU_SOFTAP_PASSWORD "YOUR_ARDU_DIRECT_PASSWORD"
