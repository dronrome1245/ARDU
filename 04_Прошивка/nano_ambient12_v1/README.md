# ARDU Nano 12 atmospheric scenes — development full-image candidate

This one-file Nano ATmega328P sketch preserves full Nano v1 audio/RTC/Light/Night/Alarm/UART but adds 12 scene renderers. F01 stays manual; F02/F03 retired (numeric identifiers not reused). New commands: 111 select scene 1..12, 112 brightness, 113 dynamics, 114 persist, 115 read all scenes. EEPROM: independent 29-byte versioned block at address 512; existing v1 blocks unchanged. Cold boot remains L01.

Do not upload until Nano+ESP compile gates pass, and use complete integrated OTA/UART hardware smoke before declaring final firmware.

## 2026-10-08 fresh compilation

[Arduino Verify 37820974644](https://github.com/dronrome1245/ARDU/actions/runs/37820974644) PASS after removing legacy F02/F03/auto write handlers. Complete Nano v2 uses **29306 / 30720 bytes Flash**, **1282 / 2048 global SRAM**; [HEX artifact](https://github.com/dronrome1245/ARDU/actions/runs/37820974644/artifacts/11568284647). Other v1 modes remain. A full hardware upload and physical ring/stack/current/EEPROM test have NOT occurred; see [central guide](../../ARDU_AMBIENT12_RELEASE_CANDIDATE.md).
