# ARDU Nano 12 atmospheric scenes — development full-image candidate

This one-file Nano ATmega328P sketch preserves full Nano v1 audio/RTC/Light/Night/Alarm/UART but adds 12 scene renderers. F01 stays manual; F02/F03 retired (numeric identifiers not reused). New commands: 111 select scene 1..12, 112 brightness, 113 dynamics, 114 persist, 115 read all scenes. EEPROM: independent 29-byte versioned block at address 512; existing v1 blocks unchanged. Cold boot remains L01.

Do not upload until Nano+ESP compile gates pass, and use complete integrated OTA/UART hardware smoke before declaring final firmware.
