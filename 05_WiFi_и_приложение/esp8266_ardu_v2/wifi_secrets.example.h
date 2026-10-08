#pragma once

// Create wifi_secrets.h alongside esp8266_ardu_v2.ino, edit FOUR values,
// then set ARDU_CREDENTIALS_CONFIGURED=1 before an OTA upload.
// wifi_secrets.h is globally ignored by Git; never share it or its passwords.
#define ARDU_WIFI_SSID "PUT_YOUR_WIFI_SSID_HERE"
#define ARDU_WIFI_PASSWORD "PUT_YOUR_WIFI_PASSWORD_HERE"
#define ARDU_OTA_PASSWORD "PUT_A_STRONG_OTA_PASSWORD_HERE"
#define ARDU_SOFTAP_PASSWORD "PUT_A_STRONG_AP_PASSWORD_HERE"
#define ARDU_CREDENTIALS_CONFIGURED 0
