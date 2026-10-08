# ARDU ESP8266 v2 — ARDU-DIRECT + twelve-scene HTTP API

Features: existing station HTTP/OTA; protected ARDU-DIRECT SoftAP 192.168.4.1 after station outage; new v2 /api/ambient/presets GET, /api/ambient/preset POST, /api/ambient/preset/settings POST. F01 manual remains; F02/F03 retired and reserved.

IMPORTANT: create local wifi_secrets.h using wifi_secrets.example.h, fill current SSID/password, OTA password and a NEW WPA2 direct-mode password (>=8 chars). CI placeholders are NOT functional on real hardware. Do not commit credentials.

OTA update requires working station OTA before disconnecting router. New ESP can run against old Nano: legacy endpoints remain, GET presets reports supported=false until full Nano twelve-scene v2 is installed. When Nano v2 is present new APIs read authoritative EEPROM state through opcode 115.

Do not declare physically final until Station OTA, protected ARDU-DIRECT fallback, power cycle, mobile reconnect, UART readback and LED functionality all pass hardware smoke.
