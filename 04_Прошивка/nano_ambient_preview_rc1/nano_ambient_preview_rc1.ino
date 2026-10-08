/*
 ARDU Nano Ambient preview RC1 — COMPLETE Nano v1 firmware.
 Special, user-visible hardware demo of P01 Aurora / P04 Cosmos / P05 Fireplace.
 Unlike an isolated test sketch, ALL ordinary light, music, clap, RTC,
 alarm/dawn, UART and the existing EEPROM layout come from nano_ardu_v1.ino.

 Preview is NOT the production v1 release and does NOT add new UART IDs:
 when Ambient/F01 is selected, it displays the three scenes (~8.2 s each)
 followed by the original F01 (~8.2 s), then repeats. Selecting F02/F03
 uses the original effects. Cold power-on still starts L01.

 Do not confuse this file with nano_ardu_v1/nano_ardu_v1.ino:
 that default build remains the previously accepted v1 behavior.
 See 04_Прошивка/AMBIENT_PRESETS_PREVIEW_RC1_TEST.md before uploading.
*/

#define ARDU_AMBIENT_PRESET_PROBE 1
#include "../nano_ardu_v1/nano_ardu_v1.ino"
