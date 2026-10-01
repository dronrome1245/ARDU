/*
  ARDU v1 release-candidate Nano firmware.
  Built from hardware-tested ARDU subsystems.

  v1 scope:
  - 2 x 44 WS2812B, mirrored frame: D6 + D7
  - L01 + persisted Kelvin/RGB profile
  - local double-clap + family CLAPCAL
  - DS3231 + Night schedule + Alarm/Dawn recovery
  - Ambient F01/F02/F03
  - Music M01/M02/M03/M04/M05/M08/M09
  - shared MAX9814/A0 audio frontend + shared FHT_N=64
  - extended EEPROM persistence + service current limit

  Do not upload until full compile/memory gate passes.
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <avr/io.h>
#define FHT_N 64
#define LOG_OUT 1
#include <FHT.h>
#ifdef SCALE
#undef SCALE
#endif

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

namespace Pins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
}

namespace Cfg {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;

constexpr uint16_t LED_COUNT = 44;
constexpr uint16_t DEFAULT_CURRENT_LIMIT_MA = 3000;
constexpr uint16_t MIN_CURRENT_LIMIT_MA = 500;
constexpr uint16_t HARD_CURRENT_LIMIT_MA = 4500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;

constexpr uint8_t DS3231_ADDR = 0x68;
constexpr unsigned long RTC_POLL_MS = 250UL;
constexpr unsigned long DAWN_FRAME_MS = 50UL;

// Existing compatible EEPROM blocks.
constexpr int ALARM_BASE = 0;       // 0..9
constexpr int DAWN_BASE = 16;       // 16..23
constexpr int DAWN_RUNTIME_BASE = 32; // 32..45
constexpr int NIGHT_BASE = 64;      // 64..76
constexpr int LIGHT_BASE = 128;     // L01 R2/R3 compatible
constexpr int CLAP_BASE = 192;      // threshold/timeout
constexpr int CORE_BASE = 224;      // current mode + clap enable
constexpr int EXT_BASE = 256;       // v1 Music/Ambient/audio/service block

constexpr uint16_t EXT_MAGIC = 0xA1D1;
constexpr uint8_t EXT_VER = 1;

constexpr uint16_t ALARM_MAGIC = 0xA8D1;
constexpr uint8_t ALARM_VER = 1;
constexpr uint16_t DAWN_MAGIC = 0xDA01;
constexpr uint8_t DAWN_VER = 1;
constexpr uint16_t DAWN_RUNTIME_MAGIC = 0xDA02;
constexpr uint8_t DAWN_RUNTIME_VER = 1;
constexpr uint16_t NIGHT_MAGIC = 0x4E32;
constexpr uint8_t NIGHT_VER = 2;
constexpr uint16_t LIGHT_MAGIC = 0x4C32;
constexpr uint8_t LIGHT_VER = 2;
constexpr uint16_t CLAP_MAGIC = 0x4343;
constexpr uint8_t CLAP_VER = 1;
constexpr uint16_t CORE_MAGIC = 0xC010;
constexpr uint8_t CORE_VER = 1;

constexpr uint8_t DEFAULT_ALARM_HOUR = 7;
constexpr uint8_t DEFAULT_ALARM_MINUTE = 0;

constexpr uint8_t DEFAULT_FADE_MIN = 30;
constexpr uint8_t DEFAULT_DAWN_MAX_BR = 120;
constexpr uint8_t DEFAULT_DAWN_START_HUE = 8;
constexpr uint8_t DEFAULT_DAWN_END_HUE = 32;

constexpr uint8_t DEFAULT_NIGHT_HUE = 24;
constexpr uint8_t DEFAULT_NIGHT_SAT = 180;
constexpr uint8_t DEFAULT_NIGHT_BR = 18;

constexpr uint8_t DEFAULT_LIGHT_BR = 64;
constexpr uint16_t DEFAULT_LIGHT_KELVIN = 4000;
constexpr uint16_t MIN_KELVIN = 1800;
constexpr uint16_t MAX_KELVIN = 6500;

constexpr int DEFAULT_CLAP_TRSH = 70;
constexpr uint16_t DEFAULT_CLAP_TIMEOUT_MS = 500;
constexpr unsigned long CLAP_COMMAND_GUARD_MS = 700UL;
constexpr uint16_t CLAP_VOL_DT_US = 700;
constexpr uint16_t CLAP_VOL_PERIOD_MS = 5;
constexpr uint8_t CLAP_VOL_WINDOW = 20;
constexpr uint16_t CLAP_AMPLI_DT_MS = 150;

constexpr unsigned long CLAPCAL_PRE_DELAY_MS = 1000UL;
constexpr unsigned long CLAPCAL_QUIET_WINDOW_MS = 3000UL;
constexpr unsigned long CLAPCAL_SAMPLE_WINDOW_MS = 2200UL;
constexpr unsigned long CLAPCAL_QUIET_SAMPLE_MS = 20UL;
constexpr uint8_t CLAPCAL_DER_FLOOR = 15;
constexpr uint8_t CLAPCAL_MIN_PAIRS = 3;
constexpr uint8_t CLAPCAL_MAX_PAIRS = 12;

constexpr uint32_t RAM_COOKIE_MAGIC = 0x41524455UL; // "ARDU"
}

// -------------------- common types --------------------

struct RtcTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

enum class SystemMode : uint8_t {
  OFF = 0,
  LIGHT = 1,
  NIGHT = 2,
  DAWN = 3,
  AMBIENT = 4,
  MUSIC = 5
};

enum class ColorMode : uint8_t {
  KELVIN = 0,
  RGB = 1
};

enum class DawnPhase : uint8_t {
  IDLE = 0,
  RUNNING = 1,
  HOLD = 2
};

enum class DawnSource : uint8_t {
  NONE = 0,
  ALARM = 1,
  MANUAL = 2,
  TEST = 3
};

struct AlarmSettings {
  bool enabled = false;
  uint8_t hour = Cfg::DEFAULT_ALARM_HOUR;
  uint8_t minute = Cfg::DEFAULT_ALARM_MINUTE;
  uint16_t lastTriggerYear = 0;
  uint8_t lastTriggerMonth = 0;
  uint8_t lastTriggerDay = 0;
  bool storageValid = false;
};

struct DawnSettings {
  uint8_t fadeMinutes = Cfg::DEFAULT_FADE_MIN;
  uint8_t maxBrightness = Cfg::DEFAULT_DAWN_MAX_BR;
  uint8_t startHue = Cfg::DEFAULT_DAWN_START_HUE;
  uint8_t endHue = Cfg::DEFAULT_DAWN_END_HUE;
  bool storageValid = false;
};

struct DawnRuntime {
  DawnPhase phase = DawnPhase::IDLE;
  DawnSource source = DawnSource::NONE;
  uint32_t startRtcSeconds = 0;
  uint32_t durationSeconds = 0;
  bool storageValid = false;
};

struct NightSettings {
  bool manualEnabled = false;
  uint8_t hue = Cfg::DEFAULT_NIGHT_HUE;
  uint8_t saturation = Cfg::DEFAULT_NIGHT_SAT;
  uint8_t brightness = Cfg::DEFAULT_NIGHT_BR;
  bool scheduleEnabled = false;
  uint8_t onHour = 22;
  uint8_t onMinute = 0;
  uint8_t offHour = 7;
  uint8_t offMinute = 0;
  bool storageValid = false;
};

struct LightSettings {
  ColorMode colorMode = ColorMode::KELVIN;
  uint8_t brightness = Cfg::DEFAULT_LIGHT_BR;
  uint16_t kelvin = Cfg::DEFAULT_LIGHT_KELVIN;
  CRGB customRgb = CRGB::White;
  bool storageValid = false;
  bool dirty = false;
};

struct ClapSettings {
  uint16_t threshold = Cfg::DEFAULT_CLAP_TRSH;
  uint16_t timeoutMs = Cfg::DEFAULT_CLAP_TIMEOUT_MS;
  bool storageValid = false;
  bool dirty = false;
};

struct LightPersist {
  uint16_t magic;
  uint8_t version;
  uint8_t colorMode;
  uint8_t brightness;
  uint16_t kelvin;
  uint8_t r,g,b;
  uint8_t sum;
};

struct ClapPersist {
  uint16_t magic;
  uint8_t version;
  uint16_t threshold;
  uint16_t timeoutMs;
  uint8_t sum;
};

struct CoreState {
  SystemMode lastMode = SystemMode::LIGHT;
  bool clapEnabled = true;
  bool storageValid = false;
};

struct TempAnchor {
  uint16_t k;
  uint8_t r, g, b;
};

enum class MusicMode : uint8_t {
  M01 = 0, M02 = 1, M03 = 2, M04 = 3, M05 = 4, M08 = 5, M09 = 6, COUNT = 7
};

enum class BandSubmode : uint8_t {
  THREE = 0, LOW_ONLY = 1, MID_ONLY = 2, HIGH_ONLY = 3
};

enum class AmbientEffect : uint8_t {
  F01 = 0, F02 = 1, F03 = 2
};

enum BandIndex : uint8_t {
  BAND_LOW = 0, BAND_MID = 1, BAND_HIGH = 2, BAND_COUNT = 3
};

struct Bands {
  uint8_t low, mid, high, peak;
};

struct MusicModeSettings {
  uint8_t brightness;
  uint8_t background;
  uint8_t smooth;
  uint8_t sensitivity;
  uint8_t speed;
  uint8_t aux;
  uint8_t submode;
};

struct AmbientSettingsV1 {
  uint8_t effect;
  uint8_t f01Hue, f01Sat, f01Brightness;
  uint8_t f02Hue, f02Sat, f02Brightness, f02Speed;
  uint8_t f03Hue, f03Brightness, f03Speed, f03Step10;
  uint8_t autoCycle, autoPeriodSec;
};

struct ExtendedSettings {
  uint16_t currentLimitMa = Cfg::DEFAULT_CURRENT_LIMIT_MA;
  uint16_t vuLowPass = 300;
  int16_t micDcOffset = 250;
  uint8_t spectrumLowPass = 40;
  uint8_t audioCalibrated = 1;
  MusicMode selectedMusic = MusicMode::M01;
  MusicModeSettings music[static_cast<uint8_t>(MusicMode::COUNT)];
  AmbientSettingsV1 ambient;
  bool storageValid = false;
  bool dirty = false;
};

struct __attribute__((packed)) ExtendedPersist {
  uint16_t magic;
  uint8_t version;
  uint16_t currentLimitMa;
  uint16_t vuLowPass;
  int16_t micDcOffset;
  uint8_t spectrumLowPass;
  uint8_t audioCalibrated;
  uint8_t selectedMusic;
  MusicModeSettings music[static_cast<uint8_t>(MusicMode::COUNT)];
  AmbientSettingsV1 ambient;
  uint8_t sum;
};

struct VuRuntime {
  uint16_t filtered;
  uint16_t averageLevel;
  uint16_t maxLevel;
  uint16_t lastPeak;
  uint16_t rainbowHue10;
  uint8_t pairs;
  unsigned long lastRainbowMs;
};

struct BandRuntime {
  uint16_t filtered[BAND_COUNT];
  uint16_t average[BAND_COUNT];
  uint8_t brightness[BAND_COUNT];
  uint8_t flashMask;
  uint8_t runningMask;
  Bands last;
  unsigned long lastShiftMs;
};

struct SpectrumRuntime {
  uint8_t trail[20];
  uint16_t maxFiltered256;
};

union MusicRuntime {
  VuRuntime vu;
  BandRuntime band;
  SpectrumRuntime spectrum;
};

// -------------------- globals --------------------

AlarmSettings alarmCfg;
DawnSettings dawnCfg;
DawnRuntime dawnRuntime;
NightSettings nightCfg;
LightSettings lightCfg;
ClapSettings clapCfg;
CoreState coreCfg;
ExtendedSettings extCfg;
MusicRuntime musicRt;

SystemMode currentMode = SystemMode::OFF;
DawnPhase dawnPhase = DawnPhase::IDLE;
bool dawnRecoveredAtBoot = false;

CRGB leds[Cfg::LED_COUNT];

char rxBuffer[Cfg::RX_BUFFER_SIZE];
size_t rxLength = 0;
unsigned long lastSerialRxMs = 0;
bool frameDirty = false;

bool rtcPresent = false;
bool rtcLostPower = true;
bool rtcReadOk = false;
unsigned long lastRtcPollMs = 0;

bool nightEffectivePower = false;
bool nightWindowActive = false;

unsigned long lastDawnFrameMs = 0;
unsigned long dawnStartMs = 0;
unsigned long dawnDurationMs = 0;

unsigned long lastMusicFrameMs = 0;
unsigned long lastAmbientFrameMs = 0;
unsigned long lastAmbientAutoMs = 0;
uint8_t ambientRuntimeHue = 0;

uint8_t rawResetFlags = 0;
bool coldBoot = true;
uint32_t ramResetCookie __attribute__((section(".noinit")));

// -------------------- small helpers --------------------

uint8_t checksum(const uint8_t* bytes, uint8_t len) {
  uint8_t c = 0x5A;
  for (uint8_t i = 0; i < len; ++i) {
    c = static_cast<uint8_t>((c << 1) | (c >> 7));
    c ^= bytes[i];
  }
  return c;
}

uint8_t bcdToDec(uint8_t v) {
  return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F));
}

uint8_t decToBcd(uint8_t v) {
  return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
}

bool isLeapYear(uint16_t y) {
  if ((y % 400) == 0) return true;
  if ((y % 100) == 0) return false;
  return (y % 4) == 0;
}

uint8_t daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t days[] = {
    31,28,31,30,31,30,31,31,30,31,30,31
  };
  if (m < 1 || m > 12) return 0;
  if (m == 2 && isLeapYear(y)) return 29;
  return days[m - 1];
}

bool validRtc(const RtcTime& t) {
  return t.year >= 2000 && t.year <= 2099 &&
         t.month >= 1 && t.month <= 12 &&
         t.day >= 1 && t.day <= daysInMonth(t.year, t.month) &&
         t.hour <= 23 && t.minute <= 59 && t.second <= 59;
}

void print2(uint8_t v) {
  if (v < 10) Serial.print('0');
  Serial.print(v);
}

void print4(uint16_t v) {
  if (v < 1000) Serial.print('0');
  if (v < 100) Serial.print('0');
  if (v < 10) Serial.print('0');
  Serial.print(v);
}

void printRtc(const RtcTime& t) {
  print4(t.year); Serial.print('-');
  print2(t.month); Serial.print('-');
  print2(t.day); Serial.print(' ');
  print2(t.hour); Serial.print(':');
  print2(t.minute); Serial.print(':');
  print2(t.second);
}

const __FlashStringHelper* modeName(SystemMode m) {
  switch (m) {
    case SystemMode::OFF: return F("OFF");
    case SystemMode::LIGHT: return F("L01");
    case SystemMode::NIGHT: return F("NIGHT");
    case SystemMode::DAWN: return F("DAWN");
    case SystemMode::AMBIENT: return F("AMBIENT");
    case SystemMode::MUSIC: return F("MUSIC");
  }
  return F("?");
}

const __FlashStringHelper* dawnPhaseName() {
  switch (dawnPhase) {
    case DawnPhase::IDLE: return F("IDLE");
    case DawnPhase::RUNNING: return F("RUNNING");
    case DawnPhase::HOLD: return F("HOLD");
  }
  return F("?");
}

const __FlashStringHelper* dawnSourceName(DawnSource s) {
  switch (s) {
    case DawnSource::NONE: return F("NONE");
    case DawnSource::ALARM: return F("ALARM");
    case DawnSource::MANUAL: return F("MANUAL");
    case DawnSource::TEST: return F("TEST");
  }
  return F("?");
}

bool serialGuard() {
  return (millis() - lastSerialRxMs) < Cfg::SERIAL_RX_GUARD_MS;
}

bool parseLongRange(const char* text, long lo, long hi, long& value) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  const long v = strtol(text, &end, 10);
  if (*end != '\0' || v < lo || v > hi) return false;
  value = v;
  return true;
}

bool parseByte(const char* text, uint8_t& value) {
  long v = 0;
  if (!parseLongRange(text, 0, 255, v)) return false;
  value = static_cast<uint8_t>(v);
  return true;
}

bool parseClock(const char* text, uint8_t& h, uint8_t& m) {
  if (text == nullptr || strlen(text) != 5 || text[2] != ':') return false;
  if (text[0] < '0' || text[0] > '9' ||
      text[1] < '0' || text[1] > '9' ||
      text[3] < '0' || text[3] > '9' ||
      text[4] < '0' || text[4] > '9') return false;
  h = static_cast<uint8_t>((text[0]-'0')*10 + (text[1]-'0'));
  m = static_cast<uint8_t>((text[3]-'0')*10 + (text[4]-'0'));
  return h <= 23 && m <= 59;
}

bool parseFixed(const char* text, uint8_t len, uint16_t& value) {
  value = 0;
  for (uint8_t i = 0; i < len; ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = static_cast<uint16_t>(value * 10 + (text[i]-'0'));
  }
  return true;
}

bool parseSetTime(const char* text, RtcTime& t) {
  if (strlen(text) != 19 ||
      text[4] != '-' || text[7] != '-' || text[10] != ' ' ||
      text[13] != ':' || text[16] != ':') return false;

  uint16_t y,mo,d,h,mi,s;
  if (!parseFixed(text+0,4,y) ||
      !parseFixed(text+5,2,mo) ||
      !parseFixed(text+8,2,d) ||
      !parseFixed(text+11,2,h) ||
      !parseFixed(text+14,2,mi) ||
      !parseFixed(text+17,2,s)) return false;

  t.year=y; t.month=mo; t.day=d; t.hour=h; t.minute=mi; t.second=s;
  return validRtc(t);
}

// -------------------- RTC --------------------

bool rtcProbe() {
  Wire.beginTransmission(Cfg::DS3231_ADDR);
  return Wire.endTransmission() == 0;
}

bool rtcReadRegs(uint8_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(Cfg::DS3231_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  const uint8_t got = Wire.requestFrom(Cfg::DS3231_ADDR, len);
  if (got != len) {
    while (Wire.available()) (void)Wire.read();
    return false;
  }
  for (uint8_t i=0;i<len;++i) {
    if (!Wire.available()) return false;
    data[i]=Wire.read();
  }
  return true;
}

bool rtcWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(Cfg::DS3231_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool rtcReadLostPower(bool& lost) {
  uint8_t s=0;
  if (!rtcReadRegs(0x0F,&s,1)) return false;
  lost = (s & 0x80) != 0;
  return true;
}

bool rtcClearLostPower() {
  uint8_t s=0;
  if (!rtcReadRegs(0x0F,&s,1)) return false;
  s &= static_cast<uint8_t>(~0x80);
  return rtcWriteReg(0x0F,s);
}

bool rtcRead(RtcTime& t) {
  uint8_t d[7]={0};
  if (!rtcReadRegs(0x00,d,7)) return false;

  t.second=bcdToDec(d[0]&0x7F);
  t.minute=bcdToDec(d[1]&0x7F);

  const uint8_t hr=d[2];
  if (hr & 0x40) {
    const uint8_t h12=bcdToDec(hr&0x1F);
    const bool pm=(hr&0x20)!=0;
    t.hour=(h12==12)?(pm?12:0):static_cast<uint8_t>(h12+(pm?12:0));
  } else {
    t.hour=bcdToDec(hr&0x3F);
  }

  t.day=bcdToDec(d[4]&0x3F);
  t.month=bcdToDec(d[5]&0x1F);
  t.year=static_cast<uint16_t>(2000+bcdToDec(d[6]));
  return validRtc(t);
}

bool rtcSet(const RtcTime& t) {
  if (!validRtc(t)) return false;
  Wire.beginTransmission(Cfg::DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(t.second));
  Wire.write(decToBcd(t.minute));
  Wire.write(decToBcd(t.hour));
  Wire.write(decToBcd(1));
  Wire.write(decToBcd(t.day));
  Wire.write(decToBcd(t.month));
  Wire.write(decToBcd(static_cast<uint8_t>(t.year-2000)));
  if (Wire.endTransmission()!=0) return false;
  return rtcClearLostPower();
}

bool rtcValidNow(RtcTime* out=nullptr) {
  rtcPresent=rtcProbe();
  if (!rtcPresent) {
    rtcReadOk=false; rtcLostPower=true; return false;
  }

  bool lost=true;
  if (!rtcReadLostPower(lost)) {
    rtcReadOk=false; rtcLostPower=true; return false;
  }
  rtcLostPower=lost;

  RtcTime now;
  if (!rtcRead(now)) {
    rtcReadOk=false; return false;
  }
  rtcReadOk=true;
  if (out) *out=now;
  return !rtcLostPower;
}

uint32_t rtcSeconds2000(const RtcTime& t) {
  uint32_t days=0;
  for (uint16_t y=2000;y<t.year;++y) days += isLeapYear(y)?366UL:365UL;
  for (uint8_t m=1;m<t.month;++m) days += daysInMonth(t.year,m);
  days += static_cast<uint32_t>(t.day-1);
  return days*86400UL +
         static_cast<uint32_t>(t.hour)*3600UL +
         static_cast<uint32_t>(t.minute)*60UL + t.second;
}

// -------------------- EEPROM: Alarm/Dawn --------------------

void saveAlarm() {
  uint8_t b[9];
  b[0]=Cfg::ALARM_MAGIC&0xFF; b[1]=Cfg::ALARM_MAGIC>>8;
  b[2]=Cfg::ALARM_VER; b[3]=alarmCfg.enabled?1:0;
  b[4]=alarmCfg.hour; b[5]=alarmCfg.minute;
  b[6]=(alarmCfg.lastTriggerYear>=2000 && alarmCfg.lastTriggerYear<=2099)
      ? static_cast<uint8_t>(alarmCfg.lastTriggerYear-2000) : 0xFF;
  b[7]=alarmCfg.lastTriggerMonth; b[8]=alarmCfg.lastTriggerDay;
  for(uint8_t i=0;i<9;++i) EEPROM.update(Cfg::ALARM_BASE+i,b[i]);
  EEPROM.update(Cfg::ALARM_BASE+9,checksum(b,9));
  alarmCfg.storageValid=true;
}

void loadAlarm() {
  uint8_t b[9];
  for(uint8_t i=0;i<9;++i) b[i]=EEPROM.read(Cfg::ALARM_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::ALARM_MAGIC && b[2]==Cfg::ALARM_VER &&
      EEPROM.read(Cfg::ALARM_BASE+9)==checksum(b,9) &&
      b[3]<=1 && b[4]<=23 && b[5]<=59 && b[7]<=12 && b[8]<=31;
  if(!ok){ alarmCfg=AlarmSettings(); return; }
  alarmCfg.enabled=b[3]!=0; alarmCfg.hour=b[4]; alarmCfg.minute=b[5];
  if(b[6]==0xFF || b[7]==0 || b[8]==0) {
    alarmCfg.lastTriggerYear=0; alarmCfg.lastTriggerMonth=0; alarmCfg.lastTriggerDay=0;
  } else {
    alarmCfg.lastTriggerYear=2000+b[6]; alarmCfg.lastTriggerMonth=b[7]; alarmCfg.lastTriggerDay=b[8];
  }
  alarmCfg.storageValid=true;
}

void saveDawnSettings() {
  uint8_t b[7];
  b[0]=Cfg::DAWN_MAGIC&0xFF; b[1]=Cfg::DAWN_MAGIC>>8; b[2]=Cfg::DAWN_VER;
  b[3]=dawnCfg.fadeMinutes; b[4]=dawnCfg.maxBrightness;
  b[5]=dawnCfg.startHue; b[6]=dawnCfg.endHue;
  for(uint8_t i=0;i<7;++i) EEPROM.update(Cfg::DAWN_BASE+i,b[i]);
  EEPROM.update(Cfg::DAWN_BASE+7,checksum(b,7));
  dawnCfg.storageValid=true;
}

void loadDawnSettings() {
  uint8_t b[7];
  for(uint8_t i=0;i<7;++i) b[i]=EEPROM.read(Cfg::DAWN_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::DAWN_MAGIC && b[2]==Cfg::DAWN_VER &&
      EEPROM.read(Cfg::DAWN_BASE+7)==checksum(b,7) &&
      b[3]>=1 && b[3]<=120 && b[4]>=1;
  if(!ok){ dawnCfg=DawnSettings(); return; }
  dawnCfg.fadeMinutes=b[3]; dawnCfg.maxBrightness=b[4];
  dawnCfg.startHue=b[5]; dawnCfg.endHue=b[6]; dawnCfg.storageValid=true;
}

void writeU32(uint8_t* p,uint32_t v){
  p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF;
}
uint32_t readU32(const uint8_t* p){
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1])<<8) |
         (static_cast<uint32_t>(p[2])<<16) |
         (static_cast<uint32_t>(p[3])<<24);
}

void saveDawnRuntime() {
  uint8_t b[13];
  b[0]=Cfg::DAWN_RUNTIME_MAGIC&0xFF; b[1]=Cfg::DAWN_RUNTIME_MAGIC>>8;
  b[2]=Cfg::DAWN_RUNTIME_VER;
  b[3]=static_cast<uint8_t>(dawnRuntime.phase);
  b[4]=static_cast<uint8_t>(dawnRuntime.source);
  writeU32(b+5,dawnRuntime.startRtcSeconds);
  writeU32(b+9,dawnRuntime.durationSeconds);
  for(uint8_t i=0;i<13;++i) EEPROM.update(Cfg::DAWN_RUNTIME_BASE+i,b[i]);
  EEPROM.update(Cfg::DAWN_RUNTIME_BASE+13,checksum(b,13));
  dawnRuntime.storageValid=true;
}

void loadDawnRuntime() {
  uint8_t b[13];
  for(uint8_t i=0;i<13;++i) b[i]=EEPROM.read(Cfg::DAWN_RUNTIME_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::DAWN_RUNTIME_MAGIC && b[2]==Cfg::DAWN_RUNTIME_VER &&
      EEPROM.read(Cfg::DAWN_RUNTIME_BASE+13)==checksum(b,13) &&
      b[3]<=static_cast<uint8_t>(DawnPhase::HOLD) &&
      b[4]<=static_cast<uint8_t>(DawnSource::TEST);
  if(!ok){ dawnRuntime=DawnRuntime(); return; }
  dawnRuntime.phase=static_cast<DawnPhase>(b[3]);
  dawnRuntime.source=static_cast<DawnSource>(b[4]);
  dawnRuntime.startRtcSeconds=readU32(b+5);
  dawnRuntime.durationSeconds=readU32(b+9);
  dawnRuntime.storageValid=true;
}

void setDawnRuntimeIdle() {
  dawnRuntime.phase=DawnPhase::IDLE;
  dawnRuntime.source=DawnSource::NONE;
  dawnRuntime.startRtcSeconds=0;
  dawnRuntime.durationSeconds=0;
  saveDawnRuntime();
}

// -------------------- EEPROM: Night --------------------

void saveNight() {
  uint8_t b[12];
  b[0]=Cfg::NIGHT_MAGIC&0xFF; b[1]=Cfg::NIGHT_MAGIC>>8; b[2]=Cfg::NIGHT_VER;
  b[3]=nightCfg.manualEnabled?1:0;
  b[4]=nightCfg.hue; b[5]=nightCfg.saturation; b[6]=nightCfg.brightness;
  b[7]=nightCfg.scheduleEnabled?1:0;
  b[8]=nightCfg.onHour; b[9]=nightCfg.onMinute;
  b[10]=nightCfg.offHour; b[11]=nightCfg.offMinute;
  for(uint8_t i=0;i<12;++i) EEPROM.update(Cfg::NIGHT_BASE+i,b[i]);
  EEPROM.update(Cfg::NIGHT_BASE+12,checksum(b,12));
  nightCfg.storageValid=true;
}

void loadNight() {
  uint8_t b[12];
  for(uint8_t i=0;i<12;++i) b[i]=EEPROM.read(Cfg::NIGHT_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::NIGHT_MAGIC && b[2]==Cfg::NIGHT_VER &&
      EEPROM.read(Cfg::NIGHT_BASE+12)==checksum(b,12) &&
      b[3]<=1 && b[7]<=1 && b[8]<=23 && b[9]<=59 &&
      b[10]<=23 && b[11]<=59 && !(b[8]==b[10] && b[9]==b[11]);
  if(!ok){ nightCfg=NightSettings(); return; }
  nightCfg.manualEnabled=b[3]!=0; nightCfg.hue=b[4]; nightCfg.saturation=b[5];
  nightCfg.brightness=b[6]; nightCfg.scheduleEnabled=b[7]!=0;
  nightCfg.onHour=b[8]; nightCfg.onMinute=b[9];
  nightCfg.offHour=b[10]; nightCfg.offMinute=b[11];
  nightCfg.storageValid=true;
}

// -------------------- EEPROM: L01 --------------------

uint8_t lightChecksum(const LightPersist& d) {
  const uint8_t* p=reinterpret_cast<const uint8_t*>(&d);
  uint8_t s=0x5A;
  for(size_t i=0;i<sizeof(LightPersist)-1;++i){
    s=static_cast<uint8_t>((s<<1)|(s>>7)); s^=p[i];
  }
  return s;
}

void loadLight() {
  LightPersist d;
  EEPROM.get(Cfg::LIGHT_BASE,d);
  const bool ok=d.magic==Cfg::LIGHT_MAGIC && d.version==Cfg::LIGHT_VER &&
      d.colorMode<=static_cast<uint8_t>(ColorMode::RGB) &&
      d.kelvin>=Cfg::MIN_KELVIN && d.kelvin<=Cfg::MAX_KELVIN &&
      d.sum==lightChecksum(d);
  if(!ok){ lightCfg=LightSettings(); return; }
  lightCfg.colorMode=static_cast<ColorMode>(d.colorMode);
  lightCfg.brightness=d.brightness; lightCfg.kelvin=d.kelvin;
  lightCfg.customRgb=CRGB(d.r,d.g,d.b);
  lightCfg.storageValid=true; lightCfg.dirty=false;
}

void saveLight() {
  LightPersist d;
  d.magic=Cfg::LIGHT_MAGIC; d.version=Cfg::LIGHT_VER;
  d.colorMode=static_cast<uint8_t>(lightCfg.colorMode);
  d.brightness=lightCfg.brightness; d.kelvin=lightCfg.kelvin;
  d.r=lightCfg.customRgb.r; d.g=lightCfg.customRgb.g; d.b=lightCfg.customRgb.b;
  d.sum=0; d.sum=lightChecksum(d);
  EEPROM.put(Cfg::LIGHT_BASE,d);
  lightCfg.storageValid=true; lightCfg.dirty=false;
}

// -------------------- EEPROM: Clap/Core --------------------

uint8_t clapChecksum(const ClapPersist& d) {
  const uint8_t* p=reinterpret_cast<const uint8_t*>(&d);
  uint8_t s=0xA7;
  for(size_t i=0;i<sizeof(ClapPersist)-1;++i){
    s=static_cast<uint8_t>((s<<1)|(s>>7)); s^=p[i];
  }
  return s;
}

void loadClap() {
  ClapPersist d;
  EEPROM.get(Cfg::CLAP_BASE,d);
  const bool ok=d.magic==Cfg::CLAP_MAGIC && d.version==Cfg::CLAP_VER &&
      d.threshold>=20 && d.threshold<=300 &&
      d.timeoutMs>=250 && d.timeoutMs<=1200 &&
      d.sum==clapChecksum(d);
  if(!ok){ clapCfg=ClapSettings(); return; }
  clapCfg.threshold=d.threshold;
  clapCfg.timeoutMs=d.timeoutMs;
  clapCfg.storageValid=true;
  clapCfg.dirty=false;
}

void saveClap() {
  ClapPersist d;
  d.magic=Cfg::CLAP_MAGIC;
  d.version=Cfg::CLAP_VER;
  d.threshold=clapCfg.threshold;
  d.timeoutMs=clapCfg.timeoutMs;
  d.sum=0;
  d.sum=clapChecksum(d);
  EEPROM.put(Cfg::CLAP_BASE,d);
  clapCfg.storageValid=true;
  clapCfg.dirty=false;
}

void loadCore() {
  uint8_t b[5];
  for(uint8_t i=0;i<5;++i) b[i]=EEPROM.read(Cfg::CORE_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::CORE_MAGIC && b[2]==Cfg::CORE_VER &&
      b[3]<=static_cast<uint8_t>(SystemMode::MUSIC) && b[4]<=1 &&
      EEPROM.read(Cfg::CORE_BASE+5)==checksum(b,5);
  if(!ok){ coreCfg=CoreState(); return; }
  coreCfg.lastMode=static_cast<SystemMode>(b[3]);
  coreCfg.clapEnabled=b[4]!=0;
  coreCfg.storageValid=true;
}

void saveCore() {
  SystemMode persistMode=currentMode;
  if(persistMode==SystemMode::DAWN) persistMode=SystemMode::OFF;
  uint8_t b[5];
  b[0]=Cfg::CORE_MAGIC&0xFF; b[1]=Cfg::CORE_MAGIC>>8; b[2]=Cfg::CORE_VER;
  b[3]=static_cast<uint8_t>(persistMode); b[4]=coreCfg.clapEnabled?1:0;
  for(uint8_t i=0;i<5;++i) EEPROM.update(Cfg::CORE_BASE+i,b[i]);
  EEPROM.update(Cfg::CORE_BASE+5,checksum(b,5));
  coreCfg.lastMode=persistMode; coreCfg.storageValid=true;
}



// -------------------- v1 extended persistence --------------------

void setMusicDefaults(MusicModeSettings& m, MusicMode id){
  m=MusicModeSettings();
  m.brightness=64;
  m.background=0;
  m.sensitivity=100;
  switch(id){
    case MusicMode::M01: m.smooth=30; break;
    case MusicMode::M02: m.smooth=30; m.aux=50; break; // rainbow step x10
    case MusicMode::M03:
    case MusicMode::M04: m.smooth=80; break;
    case MusicMode::M05: m.smooth=80; m.submode=0; break;
    case MusicMode::M08: m.smooth=80; m.speed=11; m.submode=0; break;
    case MusicMode::M09: m.speed=0; m.aux=5; break; // HUE_START / HUE_STEP
    default: break;
  }
}

void setExtendedDefaults(){
  extCfg=ExtendedSettings();
  extCfg.currentLimitMa=Cfg::DEFAULT_CURRENT_LIMIT_MA;
  extCfg.vuLowPass=300;
  extCfg.micDcOffset=250;
  extCfg.spectrumLowPass=40;
  extCfg.audioCalibrated=1;
  extCfg.selectedMusic=MusicMode::M01;
  for(uint8_t i=0;i<static_cast<uint8_t>(MusicMode::COUNT);++i)
    setMusicDefaults(extCfg.music[i],static_cast<MusicMode>(i));
  extCfg.ambient.effect=0;
  extCfg.ambient.f01Hue=0; extCfg.ambient.f01Sat=255; extCfg.ambient.f01Brightness=64;
  extCfg.ambient.f02Hue=0; extCfg.ambient.f02Sat=255; extCfg.ambient.f02Brightness=64; extCfg.ambient.f02Speed=100;
  extCfg.ambient.f03Hue=0; extCfg.ambient.f03Brightness=64; extCfg.ambient.f03Speed=1; extCfg.ambient.f03Step10=5;
  extCfg.ambient.autoCycle=0; extCfg.ambient.autoPeriodSec=10;
  extCfg.storageValid=false;
  extCfg.dirty=false;
}

uint8_t extendedChecksum(const ExtendedPersist& d){
  return checksum(reinterpret_cast<const uint8_t*>(&d),static_cast<uint8_t>(sizeof(ExtendedPersist)-1));
}

void saveExtended(){
  ExtendedPersist d{};
  d.magic=Cfg::EXT_MAGIC; d.version=Cfg::EXT_VER;
  d.currentLimitMa=extCfg.currentLimitMa;
  d.vuLowPass=extCfg.vuLowPass;
  d.micDcOffset=extCfg.micDcOffset;
  d.spectrumLowPass=extCfg.spectrumLowPass;
  d.audioCalibrated=extCfg.audioCalibrated;
  d.selectedMusic=static_cast<uint8_t>(extCfg.selectedMusic);
  memcpy(d.music,extCfg.music,sizeof(d.music));
  d.ambient=extCfg.ambient;
  d.sum=0; d.sum=extendedChecksum(d);
  EEPROM.put(Cfg::EXT_BASE,d);
  extCfg.storageValid=true; extCfg.dirty=false;
}

void loadExtended(){
  ExtendedPersist d{};
  EEPROM.get(Cfg::EXT_BASE,d);
  const bool ok=d.magic==Cfg::EXT_MAGIC && d.version==Cfg::EXT_VER &&
      d.sum==extendedChecksum(d) &&
      d.currentLimitMa>=Cfg::MIN_CURRENT_LIMIT_MA &&
      d.currentLimitMa<=Cfg::HARD_CURRENT_LIMIT_MA &&
      d.selectedMusic<static_cast<uint8_t>(MusicMode::COUNT) &&
      d.ambient.effect<=static_cast<uint8_t>(AmbientEffect::F03);
  if(!ok){setExtendedDefaults();return;}
  extCfg.currentLimitMa=d.currentLimitMa;
  extCfg.vuLowPass=d.vuLowPass;
  extCfg.micDcOffset=d.micDcOffset;
  extCfg.spectrumLowPass=d.spectrumLowPass;
  extCfg.audioCalibrated=d.audioCalibrated?1:0;
  extCfg.selectedMusic=static_cast<MusicMode>(d.selectedMusic);
  memcpy(extCfg.music,d.music,sizeof(extCfg.music));
  extCfg.ambient=d.ambient;
  extCfg.storageValid=true; extCfg.dirty=false;
}

// -------------------- Light color/render --------------------


const TempAnchor TEMP[] PROGMEM = {
  {1800,255,147,41},{2200,255,157,61},{2700,255,170,87},{3000,255,183,114},
  {4000,255,228,206},{5000,255,244,234},{6000,245,249,255},{6500,232,241,255}
};
constexpr uint8_t TEMP_N=sizeof(TEMP)/sizeof(TEMP[0]);

uint8_t lerp8(uint8_t a,uint8_t b,uint16_t n,uint16_t d){
  if(!d) return a;
  const int32_t v=static_cast<int32_t>(a) +
      (static_cast<int32_t>(static_cast<int16_t>(b)-a)*n)/d;
  return v<0?0:(v>255?255:static_cast<uint8_t>(v));
}

TempAnchor tempAnchor(uint8_t i){
  TempAnchor a;
  memcpy_P(&a,&TEMP[i],sizeof(a));
  return a;
}

CRGB kelvinRgb(uint16_t k){
  TempAnchor a=tempAnchor(0);
  if(k<=a.k)return CRGB(a.r,a.g,a.b);
  for(uint8_t i=0;i<TEMP_N-1;++i){
    const TempAnchor b=tempAnchor(i+1);
    if(k<=b.k){
      const uint16_t n=k-a.k,d=b.k-a.k;
      return CRGB(lerp8(a.r,b.r,n,d),lerp8(a.g,b.g,n,d),lerp8(a.b,b.b,n,d));
    }
    a=b;
  }
  return CRGB(a.r,a.g,a.b);
}

CRGB lightColor(){
  return lightCfg.colorMode==ColorMode::RGB ? lightCfg.customRgb : kelvinRgb(lightCfg.kelvin);
}

void prepareOff() {
  fill_solid(leds,Cfg::LED_COUNT,CRGB::Black);
  FastLED.setBrightness(0); frameDirty=true;
}

void prepareLight() {
  fill_solid(leds,Cfg::LED_COUNT,lightColor());
  FastLED.setBrightness(lightCfg.brightness); frameDirty=true;
}

uint16_t minuteOfDay(uint8_t h,uint8_t m){return static_cast<uint16_t>(h)*60U+m;}

bool insideNightWindow(const RtcTime& now){
  const uint16_t cur=minuteOfDay(now.hour,now.minute);
  const uint16_t on=minuteOfDay(nightCfg.onHour,nightCfg.onMinute);
  const uint16_t off=minuteOfDay(nightCfg.offHour,nightCfg.offMinute);
  return on<off ? (cur>=on && cur<off) : (cur>=on || cur<off);
}

bool reconcileNight(bool announce=false){
  if(!nightCfg.scheduleEnabled){
    nightWindowActive=false;
    nightEffectivePower=nightCfg.manualEnabled;
  } else {
    RtcTime now;
    if(!rtcValidNow(&now)){
      nightWindowActive=false; nightEffectivePower=false; return false;
    }
    const bool before=nightEffectivePower;
    nightWindowActive=insideNightWindow(now);
    nightEffectivePower=nightWindowActive;
    if(announce && before!=nightEffectivePower){
      Serial.print(F("EVENT NIGHT_SCHEDULE POWER="));
      Serial.println(nightEffectivePower?F("ON"):F("OFF"));
    }
  }

  if(currentMode==SystemMode::NIGHT){
    if(nightEffectivePower){
      fill_solid(leds,Cfg::LED_COUNT,CHSV(nightCfg.hue,nightCfg.saturation,255));
      FastLED.setBrightness(nightCfg.brightness);
    } else {
      prepareOff();
      return true;
    }
    frameDirty=true;
  }
  return true;
}

void showIfSafe(){
  if(!frameDirty || serialGuard()) return;
  FastLED.show(); frameDirty=false;
}


// -------------------- Ambient + shared Music engine --------------------

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

constexpr uint8_t MUSIC_DISPLAY_BINS=20;
constexpr uint8_t LOW_BIN_FIRST=2, LOW_BIN_LAST=5;
constexpr uint8_t MID_BIN_FIRST=6, MID_BIN_LAST=10;
constexpr uint8_t HIGH_BIN_FIRST=11, HIGH_BIN_LAST=31;
constexpr unsigned long MUSIC_FRAME_MS=5UL;
constexpr uint8_t MUSIC_DECAY_STEP=20;
constexpr int16_t MIC_DC_MIN=120, MIC_DC_MAX=400;

MusicModeSettings& activeMusicCfg(){
  return extCfg.music[static_cast<uint8_t>(extCfg.selectedMusic)];
}

const __FlashStringHelper* musicName(MusicMode m){
  switch(m){
    case MusicMode::M01:return F("M01"); case MusicMode::M02:return F("M02");
    case MusicMode::M03:return F("M03"); case MusicMode::M04:return F("M04");
    case MusicMode::M05:return F("M05"); case MusicMode::M08:return F("M08");
    case MusicMode::M09:return F("M09"); default:return F("M01");
  }
}

bool parseMusicMode(const char* p,MusicMode& out){
  if(strcmp(p,"M01")==0)out=MusicMode::M01;
  else if(strcmp(p,"M02")==0)out=MusicMode::M02;
  else if(strcmp(p,"M03")==0)out=MusicMode::M03;
  else if(strcmp(p,"M04")==0)out=MusicMode::M04;
  else if(strcmp(p,"M05")==0)out=MusicMode::M05;
  else if(strcmp(p,"M08")==0)out=MusicMode::M08;
  else if(strcmp(p,"M09")==0)out=MusicMode::M09;
  else return false;
  return true;
}

const __FlashStringHelper* ambientName(){
  switch(static_cast<AmbientEffect>(extCfg.ambient.effect)){
    case AmbientEffect::F01:return F("F01");
    case AmbientEffect::F02:return F("F02");
    case AmbientEffect::F03:return F("F03");
  }
  return F("F01");
}

void audioDefaultPrescaler(){
  sbi(ADCSRA,ADPS2); sbi(ADCSRA,ADPS1); sbi(ADCSRA,ADPS0);
}

void audioFastPrescaler(){
  sbi(ADCSRA,ADPS2); cbi(ADCSRA,ADPS1); sbi(ADCSRA,ADPS0);
}

bool musicUsesFht(){
  return extCfg.selectedMusic!=MusicMode::M01 && extCfg.selectedMusic!=MusicMode::M02;
}

void configureAudioForMode(){
  if(currentMode==SystemMode::MUSIC && musicUsesFht()) audioFastPrescaler();
  else audioDefaultPrescaler();
}

void resetMusicRuntime(){
  memset(&musicRt,0,sizeof(musicRt));
  lastMusicFrameMs=0;
  if(extCfg.selectedMusic==MusicMode::M01 || extCfg.selectedMusic==MusicMode::M02){
    musicRt.vu.averageLevel=50; musicRt.vu.maxLevel=100;
  }else if(extCfg.selectedMusic==MusicMode::M09){
    musicRt.spectrum.maxFiltered256=5U*256U;
  }else{
    const uint8_t bg=activeMusicCfg().background;
    for(uint8_t i=0;i<BAND_COUNT;++i) musicRt.band.brightness[i]=bg;
  }
  configureAudioForMode();
}

void analyzeFht(){
  for(uint8_t i=0;i<FHT_N;++i)
    fht_input[i]=static_cast<int16_t>(analogRead(Pins::MIC_IN))-extCfg.micDcOffset;
  fht_window(); fht_reorder(); fht_run(); fht_mag_log();
}

Bands readBands(){
  analyzeFht();
  Bands b{};
  const uint16_t sens=activeMusicCfg().sensitivity;
  for(uint8_t i=2;i<32;++i){
    uint8_t v=fht_log_out[i];
    if(v<extCfg.spectrumLowPass)v=0;
    else{
      uint16_t scaled=static_cast<uint16_t>(v)*sens/100U;
      v=scaled>255?255:static_cast<uint8_t>(scaled);
    }
    if(v>b.peak)b.peak=v;
    if(i>=LOW_BIN_FIRST&&i<=LOW_BIN_LAST){if(v>b.low)b.low=v;}
    else if(i>=MID_BIN_FIRST&&i<=MID_BIN_LAST){if(v>b.mid)b.mid=v;}
    else if(i>=HIGH_BIN_FIRST&&i<=HIGH_BIN_LAST){if(v>b.high)b.high=v;}
  }
  return b;
}

uint8_t bandValue(const Bands& b,uint8_t i){
  return i==BAND_LOW?b.low:(i==BAND_MID?b.mid:b.high);
}

uint8_t bandHue(uint8_t i){
  return i==BAND_LOW?HUE_RED:(i==BAND_MID?HUE_GREEN:HUE_YELLOW);
}

void updateBandRuntime(){
  musicRt.band.last=readBands();
  const MusicModeSettings& cfg=activeMusicCfg();
  for(uint8_t i=0;i<BAND_COUNT;++i){
    const uint8_t v=bandValue(musicRt.band.last,i);
    const int16_t avgDelta=static_cast<int16_t>(v)-static_cast<int16_t>(musicRt.band.average[i]);
    musicRt.band.average[i]=static_cast<uint16_t>(static_cast<int16_t>(musicRt.band.average[i])+avgDelta/166);
    musicRt.band.filtered[i]=static_cast<uint16_t>(
        (static_cast<uint32_t>(v)*cfg.smooth+
         static_cast<uint32_t>(musicRt.band.filtered[i])*(100U-cfg.smooth))/100U);
    const bool fire=v>0 &&
        static_cast<uint32_t>(musicRt.band.filtered[i])*10UL >
        static_cast<uint32_t>(musicRt.band.average[i])*12UL;
    if(fire){
      musicRt.band.brightness[i]=255;
      musicRt.band.flashMask|=_BV(i);
      musicRt.band.runningMask|=_BV(i);
    }else musicRt.band.flashMask&=~_BV(i);
    uint8_t& br=musicRt.band.brightness[i];
    br=br>=MUSIC_DECAY_STEP?br-MUSIC_DECAY_STEP:0;
    if(br<cfg.background)br=cfg.background;
    if(br<=cfg.background)musicRt.band.runningMask&=~_BV(i);
  }
}

void renderVu(bool rainbow){
  const MusicModeSettings& cfg=activeMusicCfg();
  if(cfg.background) fill_solid(leds,Cfg::LED_COUNT,CHSV(HUE_PURPLE,255,cfg.background));
  else fill_solid(leds,Cfg::LED_COUNT,CRGB::Black);
  const uint8_t maxPairs=Cfg::LED_COUNT/2;
  uint8_t pairs=musicRt.vu.pairs;
  if(pairs>maxPairs)pairs=maxPairs;
  for(uint8_t p=0;p<pairs;++p){
    CRGB c;
    if(rainbow){
      const uint8_t pi=static_cast<uint8_t>((static_cast<uint16_t>(p)*255U/maxPairs)-
          static_cast<uint8_t>(musicRt.vu.rainbowHue10/10U));
      c=ColorFromPalette(RainbowColors_p,pi);
    }else{
      const uint8_t hue=static_cast<uint8_t>(96U-(static_cast<uint16_t>(p)*96U/(maxPairs-1U)));
      c=CHSV(hue,255,255);
    }
    leds[p]=c;
    leds[Cfg::LED_COUNT-1U-p]=c;
  }
  FastLED.setBrightness(cfg.brightness); frameDirty=true;
}

uint16_t readVuPeak(){
  uint16_t peak=0;
  for(uint8_t i=0;i<100;++i){const uint16_t v=analogRead(Pins::MIC_IN);if(v>peak)peak=v;}
  return peak;
}

void updateVu(bool rainbow){
  MusicModeSettings& cfg=activeMusicCfg();
  if(rainbow && millis()-musicRt.vu.lastRainbowMs>=30UL){
    musicRt.vu.lastRainbowMs=millis();
    musicRt.vu.rainbowHue10=static_cast<uint16_t>((musicRt.vu.rainbowHue10+cfg.aux)%2560U);
  }
  const uint16_t peak=readVuPeak();
  musicRt.vu.lastPeak=peak;
  uint16_t level=0;
  if(peak>extCfg.vuLowPass && extCfg.vuLowPass<1023){
    level=static_cast<uint16_t>(
        (static_cast<uint32_t>(peak-extCfg.vuLowPass)*500UL)/
        static_cast<uint16_t>(1023U-extCfg.vuLowPass));
    level=static_cast<uint16_t>(static_cast<uint32_t>(level)*cfg.sensitivity/100U);
    if(level>700)level=700;
  }
  musicRt.vu.filtered=static_cast<uint16_t>(
      (static_cast<uint32_t>(level)*cfg.smooth+
       static_cast<uint32_t>(musicRt.vu.filtered)*(100U-cfg.smooth))/100U);
  if(musicRt.vu.filtered>15){
    const int16_t delta=static_cast<int16_t>(musicRt.vu.filtered)-static_cast<int16_t>(musicRt.vu.averageLevel);
    musicRt.vu.averageLevel=static_cast<uint16_t>(
        static_cast<int16_t>(musicRt.vu.averageLevel)+delta/166);
    musicRt.vu.maxLevel=static_cast<uint16_t>(
        (static_cast<uint32_t>(musicRt.vu.averageLevel)*18UL)/10UL);
    if(musicRt.vu.maxLevel<1)musicRt.vu.maxLevel=1;
    uint32_t pairs=static_cast<uint32_t>(musicRt.vu.filtered)*(Cfg::LED_COUNT/2U)/musicRt.vu.maxLevel;
    if(pairs>Cfg::LED_COUNT/2U)pairs=Cfg::LED_COUNT/2U;
    musicRt.vu.pairs=static_cast<uint8_t>(pairs);
  }else musicRt.vu.pairs=0;
  renderVu(rainbow);
}

void renderM03(){
  const MusicModeSettings& cfg=activeMusicCfg();
  FastLED.setBrightness(cfg.brightness);
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i){
    const uint8_t zone=static_cast<uint8_t>((static_cast<uint16_t>(i)*5U)/Cfg::LED_COUNT);
    const uint8_t band=(zone==0||zone==4)?BAND_HIGH:((zone==1||zone==3)?BAND_MID:BAND_LOW);
    leds[i]=CHSV(bandHue(band),255,musicRt.band.brightness[band]);
  }
  frameDirty=true;
}

void renderM04(){
  const MusicModeSettings& cfg=activeMusicCfg();
  FastLED.setBrightness(cfg.brightness);
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i){
    const uint8_t zone=static_cast<uint8_t>((static_cast<uint16_t>(i)*3U)/Cfg::LED_COUNT);
    const uint8_t band=zone==0?BAND_HIGH:(zone==1?BAND_MID:BAND_LOW);
    leds[i]=CHSV(bandHue(band),255,musicRt.band.brightness[band]);
  }
  frameDirty=true;
}

bool selectedBand(uint8_t mask,uint8_t& band){
  switch(static_cast<BandSubmode>(activeMusicCfg().submode)){
    case BandSubmode::THREE:
      if(mask&_BV(BAND_HIGH)){band=BAND_HIGH;return true;}
      if(mask&_BV(BAND_MID)){band=BAND_MID;return true;}
      if(mask&_BV(BAND_LOW)){band=BAND_LOW;return true;}
      return false;
    case BandSubmode::LOW_ONLY: if(mask&_BV(BAND_LOW)){band=BAND_LOW;return true;} return false;
    case BandSubmode::MID_ONLY: if(mask&_BV(BAND_MID)){band=BAND_MID;return true;} return false;
    case BandSubmode::HIGH_ONLY:if(mask&_BV(BAND_HIGH)){band=BAND_HIGH;return true;}return false;
  }
  return false;
}

void renderM05(){
  const MusicModeSettings& cfg=activeMusicCfg();
  uint8_t band=0;
  if(selectedBand(musicRt.band.flashMask,band))
    fill_solid(leds,Cfg::LED_COUNT,CHSV(bandHue(band),255,musicRt.band.brightness[band]));
  else fill_solid(leds,Cfg::LED_COUNT,CHSV(HUE_PURPLE,255,cfg.background));
  FastLED.setBrightness(cfg.brightness);frameDirty=true;
}

void shiftM08(){
  constexpr uint8_t leftCenter=Cfg::LED_COUNT/2-1;
  constexpr uint8_t rightCenter=Cfg::LED_COUNT/2;
  for(uint8_t i=0;i<leftCenter;++i)leds[i]=leds[i+1];
  for(uint8_t i=Cfg::LED_COUNT-1;i>rightCenter;--i)leds[i]=leds[i-1];
}

void renderM08(){
  MusicModeSettings& cfg=activeMusicCfg();
  const unsigned long now=millis();
  const uint8_t shiftMs=cfg.speed?cfg.speed:1;
  if(now-musicRt.band.lastShiftMs>=shiftMs){
    musicRt.band.lastShiftMs=now;shiftM08();
  }
  uint8_t band=0;
  CRGB c=CHSV(HUE_PURPLE,255,cfg.background);
  if(selectedBand(musicRt.band.runningMask,band))
    c=CHSV(bandHue(band),255,musicRt.band.brightness[band]);
  leds[Cfg::LED_COUNT/2-1]=c;
  leds[Cfg::LED_COUNT/2]=c;
  FastLED.setBrightness(cfg.brightness);frameDirty=true;
}

void updateSpectrum(){
  analyzeFht();
  uint8_t frameMax=5;
  for(uint8_t i=0;i<30;++i){
    uint8_t v=fht_log_out[i+2];
    if(v<extCfg.spectrumLowPass)v=0;
    else{
      uint16_t scaled=static_cast<uint16_t>(v)*activeMusicCfg().sensitivity/100U;
      v=scaled>255?255:static_cast<uint8_t>(scaled);
    }
    if(v>frameMax)frameMax=v;
    if(i<MUSIC_DISPLAY_BINS){
      if(musicRt.spectrum.trail[i]<v)musicRt.spectrum.trail[i]=v;
      musicRt.spectrum.trail[i]=musicRt.spectrum.trail[i]>2?musicRt.spectrum.trail[i]-2:0;
    }
  }
  const int32_t target=static_cast<int32_t>(frameMax)*256L;
  const int32_t delta=target-static_cast<int32_t>(musicRt.spectrum.maxFiltered256);
  musicRt.spectrum.maxFiltered256=static_cast<uint16_t>(
      static_cast<int32_t>(musicRt.spectrum.maxFiltered256)+delta/166L);
  if(musicRt.spectrum.maxFiltered256<5U*256U)musicRt.spectrum.maxFiltered256=5U*256U;
}

void renderM09(){
  const MusicModeSettings& cfg=activeMusicCfg();
  constexpr uint8_t half=Cfg::LED_COUNT/2;
  for(uint8_t pos=0;pos<half;++pos){
    uint8_t bin=static_cast<uint8_t>((static_cast<uint16_t>(pos)*MUSIC_DISPLAY_BINS)/half);
    if(bin>=MUSIC_DISPLAY_BINS)bin=MUSIC_DISPLAY_BINS-1;
    uint32_t br32=static_cast<uint32_t>(musicRt.spectrum.trail[bin])*255UL*256UL/
        musicRt.spectrum.maxFiltered256;
    if(br32>255)br32=255;
    const uint8_t br=static_cast<uint8_t>(br32);
    const uint8_t hue=static_cast<uint8_t>(cfg.speed+static_cast<uint16_t>(pos)*cfg.aux);
    const CRGB c=CHSV(hue,255,br);
    leds[half-1-pos]=c;
    leds[half+pos]=c;
  }
  FastLED.setBrightness(cfg.brightness);frameDirty=true;
}

void updateMusic(){
  if(currentMode!=SystemMode::MUSIC)return;
  const unsigned long now=millis();
  if(now-lastMusicFrameMs<MUSIC_FRAME_MS)return;
  lastMusicFrameMs=now;
  switch(extCfg.selectedMusic){
    case MusicMode::M01:updateVu(false);break;
    case MusicMode::M02:updateVu(true);break;
    case MusicMode::M03:updateBandRuntime();renderM03();break;
    case MusicMode::M04:updateBandRuntime();renderM04();break;
    case MusicMode::M05:updateBandRuntime();renderM05();break;
    case MusicMode::M08:updateBandRuntime();renderM08();break;
    case MusicMode::M09:updateSpectrum();renderM09();break;
    default:break;
  }
}

void calibrateAudio(){
  prepareOff(); FastLED.show(); frameDirty=false; delay(120);
  audioDefaultPrescaler();
  uint32_t sum=0;uint16_t quietMax=0;
  for(uint16_t i=0;i<256;++i){const uint16_t v=analogRead(Pins::MIC_IN);sum+=v;if(v>quietMax)quietMax=v;}
  extCfg.micDcOffset=static_cast<int16_t>(sum/256UL);
  for(uint16_t i=0;i<200;++i){const uint16_t v=analogRead(Pins::MIC_IN);if(v>quietMax)quietMax=v;delay(2);}
  uint32_t lp=static_cast<uint32_t>(quietMax)+13UL;
  extCfg.vuLowPass=lp>1000?1000:static_cast<uint16_t>(lp);
  if(extCfg.micDcOffset<MIC_DC_MIN||extCfg.micDcOffset>MIC_DC_MAX){
    extCfg.audioCalibrated=0;configureAudioForMode();
    Serial.print(F("ERR CAL_BAD_DC DC="));Serial.println(extCfg.micDcOffset);return;
  }
  audioFastPrescaler();
  uint8_t spectrMax=0;
  for(uint8_t f=0;f<100;++f){
    analyzeFht();
    for(uint8_t b=2;b<32;++b)if(fht_log_out[b]>spectrMax)spectrMax=fht_log_out[b];
  }
  const uint16_t sp=static_cast<uint16_t>(spectrMax)+3U;
  extCfg.spectrumLowPass=sp>255?255:static_cast<uint8_t>(sp);
  extCfg.audioCalibrated=1;extCfg.dirty=true;saveExtended();resetMusicRuntime();
  Serial.print(F("CAL AUDIO DC="));Serial.print(extCfg.micDcOffset);
  Serial.print(F(" VU_LP="));Serial.print(extCfg.vuLowPass);
  Serial.print(F(" SPECTR_LP="));Serial.println(extCfg.spectrumLowPass);
}

void prepareAmbient(){
  ambientRuntimeHue=0;
  lastAmbientFrameMs=millis();lastAmbientAutoMs=millis();
  frameDirty=true;
}

void nextAmbient(int8_t dir){
  int8_t e=static_cast<int8_t>(extCfg.ambient.effect)+dir;
  if(e<0)e=2;if(e>2)e=0;
  extCfg.ambient.effect=static_cast<uint8_t>(e);extCfg.dirty=true;prepareAmbient();
}

void updateAmbient(){
  if(currentMode!=SystemMode::AMBIENT)return;
  AmbientSettingsV1& a=extCfg.ambient;
  const unsigned long now=millis();
  if(a.autoCycle && now-lastAmbientAutoMs>=static_cast<unsigned long>(a.autoPeriodSec)*1000UL){
    lastAmbientAutoMs=now;nextAmbient(1);
  }
  const AmbientEffect e=static_cast<AmbientEffect>(a.effect);
  if(e==AmbientEffect::F01){
    fill_solid(leds,Cfg::LED_COUNT,CHSV(a.f01Hue,a.f01Sat,255));
    FastLED.setBrightness(a.f01Brightness);frameDirty=true;return;
  }
  if(e==AmbientEffect::F02){
    const uint8_t speedMs=a.f02Speed?a.f02Speed:1;
    if(now-lastAmbientFrameMs>=speedMs){lastAmbientFrameMs=now;++ambientRuntimeHue;}
    fill_solid(leds,Cfg::LED_COUNT,CHSV(static_cast<uint8_t>(a.f02Hue+ambientRuntimeHue),a.f02Sat,255));
    FastLED.setBrightness(a.f02Brightness);frameDirty=true;return;
  }
  if(now-lastAmbientFrameMs>=30UL){lastAmbientFrameMs=now;ambientRuntimeHue=static_cast<uint8_t>(ambientRuntimeHue+a.f03Speed);}
  for(uint8_t i=0;i<Cfg::LED_COUNT;++i){
    const uint8_t h=static_cast<uint8_t>(a.f03Hue+ambientRuntimeHue+
        (static_cast<uint16_t>(i)*a.f03Step10)/10U);
    leds[i]=CHSV(h,255,255);
  }
  FastLED.setBrightness(a.f03Brightness);frameDirty=true;
}

// -------------------- Dawn --------------------

uint8_t dawnHue(uint16_t p){
  int16_t d=static_cast<int16_t>(dawnCfg.endHue)-static_cast<int16_t>(dawnCfg.startHue);
  if(d>127)d-=256; if(d<-128)d+=256;
  return static_cast<uint8_t>(static_cast<int16_t>(dawnCfg.startHue)+static_cast<int32_t>(d)*p/1000L);
}

void prepareDawn(uint16_t p){
  if(p>1000)p=1000;
  const uint8_t h=dawnHue(p);
  const uint8_t br=static_cast<uint8_t>(static_cast<uint32_t>(dawnCfg.maxBrightness)*p/1000UL);
  fill_solid(leds,Cfg::LED_COUNT,CHSV(h,255,255));
  FastLED.setBrightness(br); frameDirty=true;
}

void startDawn(unsigned long durationMs,DawnSource source){
  if(durationMs==0)durationMs=1000;
  currentMode=SystemMode::DAWN;
  dawnPhase=DawnPhase::RUNNING;
  dawnDurationMs=durationMs; dawnStartMs=millis(); lastDawnFrameMs=0;
  dawnRecoveredAtBoot=false;

  RtcTime now;
  if(rtcValidNow(&now)){
    dawnRuntime.phase=DawnPhase::RUNNING; dawnRuntime.source=source;
    dawnRuntime.startRtcSeconds=rtcSeconds2000(now);
    dawnRuntime.durationSeconds=static_cast<uint32_t>((durationMs+999UL)/1000UL);
    saveDawnRuntime();
  } else {
    dawnRuntime=DawnRuntime();
  }
  prepareDawn(0);
  Serial.print(F("EVENT DAWN_START SOURCE=")); Serial.print(dawnSourceName(source));
  Serial.print(F(" DURATION_S=")); Serial.println(durationMs/1000UL);
}

void stopDawn(){
  dawnPhase=DawnPhase::IDLE; dawnDurationMs=0; setDawnRuntimeIdle();
  currentMode=SystemMode::OFF; saveCore(); prepareOff();
  Serial.println(F("EVENT DAWN_STOP"));
}

void updateDawn(){
  if(currentMode!=SystemMode::DAWN || dawnPhase!=DawnPhase::RUNNING) return;
  const unsigned long now=millis();
  if(now-lastDawnFrameMs<Cfg::DAWN_FRAME_MS)return;
  lastDawnFrameMs=now;
  const unsigned long elapsed=now-dawnStartMs;
  if(elapsed>=dawnDurationMs){
    prepareDawn(1000); dawnPhase=DawnPhase::HOLD;
    dawnRuntime.phase=DawnPhase::HOLD;
    if(dawnRuntime.source==DawnSource::NONE)dawnRuntime.source=DawnSource::MANUAL;
    saveDawnRuntime();
    Serial.println(F("EVENT DAWN_COMPLETE"));
    return;
  }
  const uint16_t p=static_cast<uint16_t>((static_cast<uint64_t>(elapsed)*1000ULL)/dawnDurationMs);
  prepareDawn(p);
}

bool recoverDawn(bool isCold){
  dawnRecoveredAtBoot=false;
  if(!dawnRuntime.storageValid || dawnRuntime.phase==DawnPhase::IDLE)return false;

  if(dawnRuntime.phase==DawnPhase::HOLD){
    if(isCold){
      setDawnRuntimeIdle();
      return false; // cold power-on must produce L01, not stale HOLD
    }
    currentMode=SystemMode::DAWN; dawnPhase=DawnPhase::HOLD;
    prepareDawn(1000); dawnRecoveredAtBoot=true;
    Serial.print(F("EVENT DAWN_RECOVER PHASE=HOLD SOURCE="));
    Serial.println(dawnSourceName(dawnRuntime.source));
    return true;
  }

  RtcTime now;
  if(!rtcValidNow(&now) || dawnRuntime.startRtcSeconds==0 || dawnRuntime.durationSeconds==0){
    setDawnRuntimeIdle(); return false;
  }
  const uint32_t n=rtcSeconds2000(now);
  if(n<dawnRuntime.startRtcSeconds){setDawnRuntimeIdle();return false;}
  const uint32_t elapsed=n-dawnRuntime.startRtcSeconds;

  if(elapsed>=dawnRuntime.durationSeconds){
    if(isCold){
      setDawnRuntimeIdle();
      return false;
    }
    currentMode=SystemMode::DAWN; dawnPhase=DawnPhase::HOLD;
    dawnRuntime.phase=DawnPhase::HOLD; saveDawnRuntime(); prepareDawn(1000);
    dawnRecoveredAtBoot=true;
    Serial.print(F("EVENT DAWN_RECOVER PHASE=HOLD SOURCE="));
    Serial.print(dawnSourceName(dawnRuntime.source));
    Serial.print(F(" ELAPSED_S=")); Serial.println(elapsed);
    return true;
  }

  currentMode=SystemMode::DAWN; dawnPhase=DawnPhase::RUNNING;
  dawnDurationMs=dawnRuntime.durationSeconds*1000UL;
  dawnStartMs=millis()-elapsed*1000UL; lastDawnFrameMs=0;
  const uint16_t p=static_cast<uint16_t>((static_cast<uint64_t>(elapsed)*1000ULL)/dawnRuntime.durationSeconds);
  prepareDawn(p); dawnRecoveredAtBoot=true;
  Serial.print(F("EVENT DAWN_RECOVER PHASE=RUNNING SOURCE="));
  Serial.print(dawnSourceName(dawnRuntime.source));
  Serial.print(F(" ELAPSED_S=")); Serial.print(elapsed);
  Serial.print(F(" REMAINING_S=")); Serial.println(dawnRuntime.durationSeconds-elapsed);
  return true;
}

// -------------------- Alarm --------------------

bool alarmTriggeredToday(const RtcTime& t){
  return alarmCfg.lastTriggerYear==t.year &&
         alarmCfg.lastTriggerMonth==t.month &&
         alarmCfg.lastTriggerDay==t.day;
}

void updateAlarm(){
  const unsigned long ms=millis();
  if(ms-lastRtcPollMs<Cfg::RTC_POLL_MS)return;
  lastRtcPollMs=ms;

  RtcTime now;
  if(!rtcValidNow(&now))return;

  if(currentMode==SystemMode::NIGHT && nightCfg.scheduleEnabled){
    (void)reconcileNight(true);
  }

  if(!alarmCfg.enabled)return;
  if(now.hour==alarmCfg.hour && now.minute==alarmCfg.minute && !alarmTriggeredToday(now)){
    alarmCfg.lastTriggerYear=now.year; alarmCfg.lastTriggerMonth=now.month; alarmCfg.lastTriggerDay=now.day;
    saveAlarm();
    Serial.print(F("EVENT ALARM_TRIGGER RTC=")); printRtc(now); Serial.println();
    startDawn(static_cast<unsigned long>(dawnCfg.fadeMinutes)*60UL*1000UL,DawnSource::ALARM);
  }
}

// -------------------- Gyver-style clap + compact family calibration --------------------

class RawEnvelope {
public:
  void begin(){pinMode(Pins::MIC_IN,INPUT);reset();}
  void reset(){
    _windowMax=0;_rawMax=0;_maxs=0;_count=0;
    _tmrSample=micros();_tmrPeriod=millis();_tmrAmpli=millis();
  }
  bool tick(){
    const unsigned long ms=millis();
    if(ms-_tmrAmpli>=Cfg::CLAP_AMPLI_DT_MS){_tmrAmpli=ms;_maxs=0;}
    if(ms-_tmrPeriod<Cfg::CLAP_VOL_PERIOD_MS)return false;
    const unsigned long us=micros();
    if(us-_tmrSample<Cfg::CLAP_VOL_DT_US)return false;
    _tmrSample=us;
    const uint16_t s=analogRead(Pins::MIC_IN);
    if(s>_windowMax)_windowMax=s;
    if(++_count<Cfg::CLAP_VOL_WINDOW)return false;
    _tmrPeriod=ms;
    if(_windowMax>_maxs)_maxs=_windowMax;
    _rawMax=_maxs;_windowMax=0;_count=0;return true;
  }
  uint16_t rawMax()const{return _rawMax;}
private:
  uint16_t _windowMax=0,_rawMax=0,_maxs=0;
  uint8_t _count=0;
  unsigned long _tmrSample=0,_tmrPeriod=0,_tmrAmpli=0;
};

class ClapDetector {
public:
  void configure(int tr,uint16_t to){_tr=tr;_tout=to;}
  void reset(){
    _tmr=millis();_tmr2=millis();_prev=0;_primed=false;_prevSignal=0;
    _state=0;_claps=0;_ready=false;_clap=false;_start=false;
    _posPeak=0;_negPeak=0;_strength=0;
  }
  void tick(int val){
    if(millis()-_tmr<10)return;_tmr=millis();
    if(!_primed){_prev=val;_primed=true;return;}
    const int der=val-_prev;_prev=val;
    int signal=0,front=0;
    if(der>_tr)signal=1;else if(der<-_tr)signal=-1;
    if(!_prevSignal&&signal)front=signal;
    _prevSignal=signal;
    const uint32_t dt=millis()-_tmr2;

    if(_state==1&&der>_posPeak)_posPeak=der;
    if(_state==2&&-der>_negPeak)_negPeak=-der;

    if(front==1&&_state==0){
      _state=1;_posPeak=der;_negPeak=0;
      if(!_start){_claps=0;_ready=false;}
      _start=true;_clap=false;_tmr2=millis();
    }else if(front==-1&&_state==1&&dt<=200){
      _state=2;_negPeak=-der;_tmr2=millis();
    }else if(!front&&_state==2&&dt<=200){
      _state=0;++_claps;_clap=true;
      const int w=_posPeak<_negPeak?_posPeak:_negPeak;
      _strength=w>0?static_cast<uint16_t>(w):0;_tmr2=millis();
    }else if(_state&&dt>200){
      _state=0;_posPeak=0;_negPeak=0;
    }else if(_start&&dt>_tout){
      _state=0;_start=false;if(_claps)_ready=true;
    }
  }
  bool takeClap(uint16_t& v){if(!_clap)return false;_clap=false;v=_strength;return true;}
  bool takeSequence(uint8_t& n){if(!_ready)return false;_ready=false;n=_claps;_claps=0;return true;}
private:
  unsigned long _tmr=0,_tmr2=0;
  int _prev=0,_tr=Cfg::DEFAULT_CLAP_TRSH;
  uint16_t _tout=Cfg::DEFAULT_CLAP_TIMEOUT_MS;
  uint8_t _state=0,_claps=0;
  int8_t _prevSignal=0;
  bool _primed=false,_ready=false,_clap=false,_start=false;
  int _posPeak=0,_negPeak=0;
  uint16_t _strength=0;
};

RawEnvelope clapVol;
ClapDetector clapDetector;
unsigned long clapIgnoreUntil=0;

bool clapCalActive=false,clapCalFinished=false;
uint8_t clapCalTargetPairs=0,clapCalGoodPairs=0;
uint16_t clapCalWeak[Cfg::CLAPCAL_MAX_PAIRS];
uint16_t clapCalQuietP99=0,clapCalP20=0,clapCalSuggested=0;

void resetClapDetector(){
  clapDetector.configure(clapCfg.threshold,clapCfg.timeoutMs);
  clapDetector.reset();
  clapIgnoreUntil=millis()+Cfg::CLAP_COMMAND_GUARD_MS;
}

void toggleLightByClap(){
  if(currentMode==SystemMode::LIGHT){
    currentMode=SystemMode::OFF;saveCore();prepareOff();
    Serial.println(F("EVENT CLAP_TOGGLE LIGHT=OFF"));
  }else if(currentMode==SystemMode::OFF){
    currentMode=SystemMode::LIGHT;saveCore();prepareLight();
    Serial.println(F("EVENT CLAP_TOGGLE LIGHT=ON"));
  }else return;
  resetClapDetector();
}

void updateClap(){
  clapVol.tick();
  if(clapCalActive||!coreCfg.clapEnabled)return;
  if(currentMode!=SystemMode::LIGHT&&currentMode!=SystemMode::OFF)return;
  if(static_cast<long>(millis()-clapIgnoreUntil)<0)return;
  clapDetector.tick(clapVol.rawMax());
  uint8_t n=0;
  if(clapDetector.takeSequence(n)&&n==2)toggleLightByClap();
}

void sortU16(uint16_t* v,uint8_t n){
  for(uint8_t i=1;i<n;++i){
    const uint16_t key=v[i];int8_t j=static_cast<int8_t>(i)-1;
    while(j>=0&&v[j]>key){v[j+1]=v[j];--j;}v[j+1]=key;
  }
}

uint16_t measureClapQuietP99(){
  uint8_t hist[32]={0};
  clapVol.reset();
  uint16_t prev=0;bool primed=false;
  uint16_t samples=0;unsigned long last=0;
  delay(Cfg::CLAPCAL_PRE_DELAY_MS);
  const unsigned long start=millis();

  while(millis()-start<Cfg::CLAPCAL_QUIET_WINDOW_MS){
    clapVol.tick();
    if(millis()-last<Cfg::CLAPCAL_QUIET_SAMPLE_MS)continue;
    last=millis();
    const uint16_t cur=clapVol.rawMax();
    if(!primed){prev=cur;primed=true;continue;}
    const int d=static_cast<int>(cur)-static_cast<int>(prev);prev=cur;
    const uint16_t a=d<0?static_cast<uint16_t>(-d):static_cast<uint16_t>(d);
    const uint8_t bin=a>=248?31:static_cast<uint8_t>(a/8);
    if(hist[bin]<255)++hist[bin];++samples;
  }

  if(!samples)return 0;
  const uint16_t target=static_cast<uint16_t>((samples*99UL+99UL)/100UL);
  uint16_t sum=0;
  for(uint8_t i=0;i<32;++i){sum+=hist[i];if(sum>=target)return i*8U+7U;}
  return 255;
}

void resetClapCalibration(){
  clapCalActive=false;clapCalFinished=false;
  clapCalTargetPairs=0;clapCalGoodPairs=0;
  clapCalQuietP99=0;clapCalP20=0;clapCalSuggested=0;
  for(uint8_t i=0;i<Cfg::CLAPCAL_MAX_PAIRS;++i)clapCalWeak[i]=0;
}

void startClapCalibration(uint8_t pairs){
  resetClapCalibration();clapCalActive=true;clapCalTargetPairs=pairs;
  clapCalQuietP99=measureClapQuietP99();
  Serial.print(F("CAL Q "));Serial.print(clapCalQuietP99);
  Serial.print(' ');Serial.println(clapCalTargetPairs);
}

void captureClapCalibrationSample(){
  if(!clapCalActive){Serial.println(F("ERR CAL_STATE"));return;}
  if(clapCalGoodPairs>=clapCalTargetPairs){Serial.println(F("ERR CAL_DONE"));return;}

  delay(Cfg::CLAPCAL_PRE_DELAY_MS);
  clapVol.reset();
  clapDetector.configure(Cfg::CLAPCAL_DER_FLOOR,1000);
  clapDetector.reset();

  uint16_t top1=0,top2=0;uint8_t count=0;
  const unsigned long start=millis();
  while(millis()-start<Cfg::CLAPCAL_SAMPLE_WINDOW_MS){
    clapVol.tick();clapDetector.tick(clapVol.rawMax());
    uint16_t s=0;
    if(clapDetector.takeClap(s)&&s>=Cfg::CLAPCAL_DER_FLOOR){
      ++count;
      if(s>=top1){top2=top1;top1=s;}
      else if(s>top2)top2=s;
    }
    uint8_t ignored=0;(void)clapDetector.takeSequence(ignored);
  }
  resetClapDetector();

  if(count<2){Serial.print(F("CAL F "));Serial.println(count);return;}
  clapCalWeak[clapCalGoodPairs]=top2;
  ++clapCalGoodPairs;
  Serial.print(F("CAL S "));Serial.print(clapCalGoodPairs);
  Serial.print('/');Serial.print(clapCalTargetPairs);
  Serial.print(' ');Serial.print(top2);
  Serial.print(' ');Serial.println(top1);
}

void finishClapCalibration(){
  if(!clapCalActive||clapCalGoodPairs<clapCalTargetPairs){
    Serial.println(F("ERR CAL_INCOMPLETE"));return;
  }

  sortU16(clapCalWeak,clapCalGoodPairs);
  const uint8_t p20=static_cast<uint8_t>(
      (static_cast<uint16_t>(clapCalGoodPairs-1)*20U)/100U);
  clapCalP20=clapCalWeak[p20];

  const uint16_t fromClap=static_cast<uint16_t>(
      (static_cast<uint32_t>(clapCalP20)*70UL)/100UL);
  const uint16_t fromNoise=clapCalQuietP99+10U;
  uint16_t v=fromClap>fromNoise?fromClap:fromNoise;
  if(v<35)v=35;if(v>150)v=150;

  if(fromNoise>=clapCalP20||v>=clapCalP20){
    Serial.println(F("ERR CAL_SEPARATION"));return;
  }

  clapCalSuggested=v;clapCfg.threshold=v;clapCfg.dirty=true;
  clapCalFinished=true;clapCalActive=false;resetClapDetector();
  Serial.print(F("CAL R "));Serial.print(clapCalQuietP99);
  Serial.print(' ');Serial.print(clapCalP20);
  Serial.print(' ');Serial.println(clapCalSuggested);
}

void printClapCalStatus(){
  Serial.print(F("CAL ST "));
  Serial.print(clapCalActive?1:0);Serial.print(' ');
  Serial.print(clapCalFinished?1:0);Serial.print(' ');
  Serial.print(clapCalGoodPairs);Serial.print('/');
  Serial.print(clapCalTargetPairs);Serial.print(' ');
  Serial.print(clapCalQuietP99);Serial.print(' ');
  Serial.print(clapCalSuggested);Serial.println();
}

// -------------------- mode/render/reset --------------------

void applyMode(SystemMode m,bool persist){
  currentMode=m;
  if(m==SystemMode::LIGHT){
    audioDefaultPrescaler(); prepareLight(); resetClapDetector();
  } else if(m==SystemMode::NIGHT){
    audioDefaultPrescaler(); (void)reconcileNight(false);
  } else if(m==SystemMode::AMBIENT){
    audioDefaultPrescaler(); prepareAmbient();
  } else if(m==SystemMode::MUSIC){
    resetMusicRuntime();
  } else if(m==SystemMode::OFF){
    audioDefaultPrescaler(); prepareOff(); resetClapDetector();
  }
  if(persist && m!=SystemMode::DAWN)saveCore();
}

void classifyReset(){
  rawResetFlags=MCUSR;
  MCUSR=0;
  const bool cookieValid=(ramResetCookie==Cfg::RAM_COOKIE_MAGIC);
  const bool por=(rawResetFlags&_BV(PORF))!=0;
  const bool bor=(rawResetFlags&_BV(BORF))!=0;
  coldBoot=por||bor||!cookieValid;
  ramResetCookie=Cfg::RAM_COOKIE_MAGIC;
}

const __FlashStringHelper* resetName(){
  if(coldBoot)return F("POWER");
  if(rawResetFlags&_BV(EXTRF))return F("EXTERNAL");
  if(rawResetFlags&_BV(WDRF))return F("WATCHDOG");
  return F("WARM_UNKNOWN");
}

// -------------------- status --------------------

void printMainStatus(){
  RtcTime now;
  const bool rv=rtcValidNow(&now);
  Serial.print(F("STATUS FW=ARDU_V1 REV="));Serial.print(Cfg::FW_REV);
  Serial.print(F(" MODE="));Serial.print(modeName(currentMode));
  Serial.print(F(" RESET="));Serial.print(resetName());
  Serial.print(F(" MCUSR="));Serial.print(rawResetFlags);
  Serial.print(F(" LEDS="));Serial.print(Cfg::LED_COUNT);
  Serial.print(F(" RING_A=D6 RING_B=D7 MIRROR=YES"));
  Serial.print(F(" LIMIT_MA="));Serial.print(extCfg.currentLimitMa);
  Serial.print(F(" RTC_VALID="));Serial.print(rv?F("YES"):F("NO"));
  if(rv){Serial.print(F(" TIME="));printRtc(now);}
  Serial.print(F(" CLAP="));Serial.print(coreCfg.clapEnabled?F("ON"):F("OFF"));
  Serial.print(F(" CLAP_TRSH="));Serial.print(clapCfg.threshold);
  Serial.print(F(" UPTIME_MS="));Serial.println(millis());
}

void printLightStatus(){
  const CRGB c=lightColor();
  Serial.print(F("LIGHT STATUS MODE="));
  Serial.print(lightCfg.colorMode==ColorMode::KELVIN?F("KELVIN"):F("RGB"));
  Serial.print(F(" KELVIN="));Serial.print(lightCfg.kelvin);
  Serial.print(F(" RGB="));Serial.print(c.r);Serial.print(',');Serial.print(c.g);Serial.print(',');Serial.print(c.b);
  Serial.print(F(" BRIGHTNESS="));Serial.print(lightCfg.brightness);
  Serial.print(F(" SAVED="));Serial.print(lightCfg.storageValid?F("YES"):F("NO"));
  Serial.print(F(" DIRTY="));Serial.println(lightCfg.dirty?F("YES"):F("NO"));
}

void printClapStatus(){
  Serial.print(F("CLAP STATUS ENABLED="));Serial.print(coreCfg.clapEnabled?F("YES"):F("NO"));
  Serial.print(F(" GESTURE=DOUBLE TRSH="));Serial.print(clapCfg.threshold);
  Serial.print(F(" TIMEOUT_MS="));Serial.print(clapCfg.timeoutMs);
  Serial.print(F(" SAVED="));Serial.print(clapCfg.storageValid?F("YES"):F("NO"));
  Serial.print(F(" DIRTY="));Serial.print(clapCfg.dirty?F("YES"):F("NO"));
  Serial.print(F(" CALIBRATION=READY"));
  Serial.println();
}

void printAlarmDawnStatus(){
  Serial.print(F("ALARM STATUS ENABLED="));Serial.print(alarmCfg.enabled?F("YES"):F("NO"));
  Serial.print(F(" TIME="));print2(alarmCfg.hour);Serial.print(':');print2(alarmCfg.minute);
  Serial.print(F(" DAWN="));Serial.print(dawnPhaseName());
  Serial.print(F(" SOURCE="));Serial.print(dawnSourceName(dawnRuntime.source));
  Serial.print(F(" FADE_MIN="));Serial.print(dawnCfg.fadeMinutes);
  Serial.print(F(" MAX_BR="));Serial.print(dawnCfg.maxBrightness);
  Serial.print(F(" START_HUE="));Serial.print(dawnCfg.startHue);
  Serial.print(F(" END_HUE="));Serial.print(dawnCfg.endHue);
  Serial.print(F(" RECOVERED="));Serial.println(dawnRecoveredAtBoot?F("YES"):F("NO"));
}

void printNightStatus(){
  Serial.print(F("NIGHT STATUS MODE_SELECTED="));Serial.print(currentMode==SystemMode::NIGHT?F("YES"):F("NO"));
  Serial.print(F(" POWER="));Serial.print(nightEffectivePower?F("ON"):F("OFF"));
  Serial.print(F(" HUE="));Serial.print(nightCfg.hue);
  Serial.print(F(" SAT="));Serial.print(nightCfg.saturation);
  Serial.print(F(" BRIGHTNESS="));Serial.print(nightCfg.brightness);
  Serial.print(F(" SCHEDULE="));Serial.print(nightCfg.scheduleEnabled?F("ON"):F("OFF"));
  Serial.print(F(" ON="));print2(nightCfg.onHour);Serial.print(':');print2(nightCfg.onMinute);
  Serial.print(F(" OFF="));print2(nightCfg.offHour);Serial.print(':');print2(nightCfg.offMinute);
  Serial.print(F(" WINDOW="));Serial.println(nightWindowActive?F("ACTIVE"):F("INACTIVE"));
}

// -------------------- commands --------------------

bool parseRgb(const char* p,uint8_t& r,uint8_t& g,uint8_t& b){
  char* e=nullptr; long rv=strtol(p,&e,10); if(e==p||rv<0||rv>255)return false;
  while(*e==' ')++e; char* e2=nullptr; long gv=strtol(e,&e2,10); if(e2==e||gv<0||gv>255)return false;
  while(*e2==' ')++e2; char* e3=nullptr; long bv=strtol(e2,&e3,10); if(e3==e2||bv<0||bv>255)return false;
  while(*e3==' ')++e3; if(*e3!='\0')return false;
  r=rv;g=gv;b=bv;return true;
}

void handleLight(char* a){
  if(strcmp(a,"STATUS")==0){printLightStatus();return;}
  if(strcmp(a,"ON")==0){applyMode(SystemMode::LIGHT,true);Serial.println(F("OK LIGHT=ON"));return;}
  if(strcmp(a,"OFF")==0){applyMode(SystemMode::OFF,true);Serial.println(F("OK LIGHT=OFF"));return;}
  if(strcmp(a,"SAVE")==0){saveLight();Serial.println(F("OK LIGHT_SAVED"));return;}
  if(strcmp(a,"LOAD")==0){loadLight();if(currentMode==SystemMode::LIGHT)prepareLight();Serial.println(F("OK LIGHT_LOADED"));return;}
  if(strcmp(a,"CLAP ON")==0){coreCfg.clapEnabled=true;saveCore();Serial.println(F("OK CLAP=ON"));return;}
  if(strcmp(a,"CLAP OFF")==0){coreCfg.clapEnabled=false;saveCore();Serial.println(F("OK CLAP=OFF"));return;}

  if(strncmp(a,"BRIGHT ",7)==0){
    uint8_t v=0;if(!parseByte(a+7,v)){Serial.println(F("ERR BAD_BRIGHTNESS"));return;}
    lightCfg.brightness=v;lightCfg.dirty=true;if(currentMode==SystemMode::LIGHT)prepareLight();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"KELVIN ",7)==0){
    long v=0;if(!parseLongRange(a+7,Cfg::MIN_KELVIN,Cfg::MAX_KELVIN,v)){Serial.println(F("ERR BAD_KELVIN"));return;}
    lightCfg.colorMode=ColorMode::KELVIN;lightCfg.kelvin=v;lightCfg.dirty=true;
    if(currentMode==SystemMode::LIGHT)prepareLight();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"PRESET ",7)==0){
    long v=0;if(!parseLongRange(a+7,0,10000,v)||(v!=2700&&v!=4000&&v!=6000)){Serial.println(F("ERR BAD_PRESET"));return;}
    lightCfg.colorMode=ColorMode::KELVIN;lightCfg.kelvin=v;lightCfg.dirty=true;
    if(currentMode==SystemMode::LIGHT)prepareLight();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"RGB ",4)==0){
    uint8_t r,g,b;if(!parseRgb(a+4,r,g,b)){Serial.println(F("ERR BAD_RGB"));return;}
    lightCfg.colorMode=ColorMode::RGB;lightCfg.customRgb=CRGB(r,g,b);lightCfg.dirty=true;
    if(currentMode==SystemMode::LIGHT)prepareLight();Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR LIGHT_COMMAND"));
}

void handleClap(char* a){
  if(strcmp(a,"STATUS")==0){printClapStatus();return;}
  if(strcmp(a,"SAVE")==0){saveClap();resetClapDetector();Serial.println(F("OK CLAP_SAVED"));return;}
  if(strcmp(a,"DEFAULTS")==0){
    clapCfg.threshold=Cfg::DEFAULT_CLAP_TRSH;clapCfg.timeoutMs=Cfg::DEFAULT_CLAP_TIMEOUT_MS;
    clapCfg.dirty=true;resetClapDetector();Serial.println(F("OK CLAP_DEFAULTS TRSH=70 TIMEOUT=500"));return;
  }
  if(strncmp(a,"TRSH ",5)==0){
    long v=0;if(!parseLongRange(a+5,20,300,v)){Serial.println(F("ERR BAD_TRSH"));return;}
    clapCfg.threshold=v;clapCfg.dirty=true;resetClapDetector();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"TIMEOUT ",8)==0){
    long v=0;if(!parseLongRange(a+8,250,1200,v)){Serial.println(F("ERR BAD_TIMEOUT"));return;}
    clapCfg.timeoutMs=v;clapCfg.dirty=true;resetClapDetector();Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR CLAP_COMMAND"));
}


void handleClapCal(char* a){
  if(strncmp(a,"START ",6)==0){
    long p=0;
    if(!parseLongRange(a+6,Cfg::CLAPCAL_MIN_PAIRS,Cfg::CLAPCAL_MAX_PAIRS,p)){
      Serial.println(F("ERR CAL_PAIRS"));return;
    }
    startClapCalibration(static_cast<uint8_t>(p));return;
  }
  if(strcmp(a,"SAMPLE")==0){captureClapCalibrationSample();return;}
  if(strcmp(a,"FINISH")==0){finishClapCalibration();return;}
  if(strcmp(a,"SAVE")==0){
    if(!clapCalFinished&&!clapCfg.dirty){Serial.println(F("ERR CAL_SAVE"));return;}
    saveClap();resetClapDetector();Serial.println(F("OK CAL_SAVED"));return;
  }
  if(strcmp(a,"STATUS")==0){printClapCalStatus();return;}
  if(strcmp(a,"CANCEL")==0){
    resetClapCalibration();loadClap();resetClapDetector();
    Serial.println(F("OK CAL_CANCEL"));return;
  }
  Serial.println(F("ERR CAL_CMD"));
}

void handleNight(char* a){
  if(strcmp(a,"STATUS")==0){printNightStatus();return;}
  if(strcmp(a,"ON")==0){
    nightCfg.scheduleEnabled=false;nightCfg.manualEnabled=true;saveNight();
    applyMode(SystemMode::NIGHT,true);Serial.println(F("OK NIGHT=ON CONTROL=MANUAL"));return;
  }
  if(strcmp(a,"OFF")==0){
    nightCfg.scheduleEnabled=false;nightCfg.manualEnabled=false;saveNight();
    applyMode(SystemMode::NIGHT,true);Serial.println(F("OK NIGHT=OFF CONTROL=MANUAL"));return;
  }
  if(strcmp(a,"SCHEDULE ON")==0){
    if(!rtcValidNow()){Serial.println(F("ERR RTC_INVALID"));return;}
    nightCfg.scheduleEnabled=true;saveNight();currentMode=SystemMode::NIGHT;saveCore();(void)reconcileNight(false);
    Serial.println(F("OK NIGHT_SCHEDULE=ON"));return;
  }
  if(strcmp(a,"SCHEDULE OFF")==0){
    nightCfg.scheduleEnabled=false;saveNight();(void)reconcileNight(false);
    Serial.println(F("OK NIGHT_SCHEDULE=OFF"));return;
  }
  if(strncmp(a,"HUE ",4)==0){
    uint8_t v;if(!parseByte(a+4,v)){Serial.println(F("ERR BAD_HUE"));return;}
    nightCfg.hue=v;saveNight();(void)reconcileNight(false);Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SAT ",4)==0){
    uint8_t v;if(!parseByte(a+4,v)){Serial.println(F("ERR BAD_SAT"));return;}
    nightCfg.saturation=v;saveNight();(void)reconcileNight(false);Serial.println(F("OK"));return;
  }
  if(strncmp(a,"BRIGHT ",7)==0){
    uint8_t v;if(!parseByte(a+7,v)){Serial.println(F("ERR BAD_BRIGHTNESS"));return;}
    nightCfg.brightness=v;saveNight();(void)reconcileNight(false);Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SCHEDULESET ",12)==0){
    char* p=a+12;char* sp=strchr(p,' ');if(!sp){Serial.println(F("ERR BAD_SCHEDULE"));return;}
    *sp='\0';uint8_t oh,om,fh,fm;
    if(!parseClock(p,oh,om)||!parseClock(sp+1,fh,fm)||(oh==fh&&om==fm)){Serial.println(F("ERR BAD_SCHEDULE"));return;}
    nightCfg.onHour=oh;nightCfg.onMinute=om;nightCfg.offHour=fh;nightCfg.offMinute=fm;saveNight();
    if(nightCfg.scheduleEnabled)(void)reconcileNight(false);Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR NIGHT_COMMAND"));
}

void handleAlarm(char* a){
  if(strcmp(a,"STATUS")==0){printAlarmDawnStatus();return;}
  if(strcmp(a,"ON")==0){alarmCfg.enabled=true;saveAlarm();Serial.println(F("OK ALARM=ON"));return;}
  if(strcmp(a,"OFF")==0){alarmCfg.enabled=false;saveAlarm();Serial.println(F("OK ALARM=OFF"));return;}
  if(strcmp(a,"CLEARLAST")==0){
    alarmCfg.lastTriggerYear=0;alarmCfg.lastTriggerMonth=0;alarmCfg.lastTriggerDay=0;saveAlarm();
    Serial.println(F("OK LAST_TRIGGER=NONE"));return;
  }
  if(strncmp(a,"SET ",4)==0){
    uint8_t h,m;if(!parseClock(a+4,h,m)){Serial.println(F("ERR BAD_ALARM_TIME"));return;}
    alarmCfg.hour=h;alarmCfg.minute=m;saveAlarm();Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR ALARM_COMMAND"));
}

void handleDawn(char* a){
  if(strcmp(a,"STATUS")==0){printAlarmDawnStatus();return;}
  if(strcmp(a,"START")==0){
    startDawn(static_cast<unsigned long>(dawnCfg.fadeMinutes)*60UL*1000UL,DawnSource::MANUAL);return;
  }
  if(strcmp(a,"STOP")==0){stopDawn();return;}
  if(strncmp(a,"TEST ",5)==0){
    long s=0;if(!parseLongRange(a+5,5,120,s)){Serial.println(F("ERR BAD_TEST_SECONDS"));return;}
    startDawn(static_cast<unsigned long>(s)*1000UL,DawnSource::TEST);return;
  }
  if(strncmp(a,"FADEMIN ",8)==0){
    long v=0;if(!parseLongRange(a+8,1,120,v)){Serial.println(F("ERR BAD_FADE"));return;}
    dawnCfg.fadeMinutes=v;saveDawnSettings();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"MAXBR ",6)==0){
    long v=0;if(!parseLongRange(a+6,1,255,v)){Serial.println(F("ERR BAD_MAXBR"));return;}
    dawnCfg.maxBrightness=v;saveDawnSettings();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"STARTHUE ",9)==0){
    uint8_t v;if(!parseByte(a+9,v)){Serial.println(F("ERR BAD_HUE"));return;}
    dawnCfg.startHue=v;saveDawnSettings();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"ENDHUE ",7)==0){
    uint8_t v;if(!parseByte(a+7,v)){Serial.println(F("ERR BAD_HUE"));return;}
    dawnCfg.endHue=v;saveDawnSettings();Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR DAWN_COMMAND"));
}


void printMusicStatus(){
  const MusicModeSettings& m=activeMusicCfg();
  Serial.print(F("MUSIC STATUS ID="));Serial.print(musicName(extCfg.selectedMusic));
  Serial.print(F(" BRIGHT="));Serial.print(m.brightness);
  Serial.print(F(" BG="));Serial.print(m.background);
  Serial.print(F(" SMOOTH="));Serial.print(m.smooth);
  Serial.print(F(" SENS="));Serial.print(m.sensitivity);
  Serial.print(F(" SPEED="));Serial.print(m.speed);
  Serial.print(F(" AUX="));Serial.print(m.aux);
  Serial.print(F(" SUB="));Serial.print(m.submode);
  Serial.print(F(" CAL="));Serial.print(extCfg.audioCalibrated?F("YES"):F("NO"));
  Serial.print(F(" DC="));Serial.print(extCfg.micDcOffset);
  Serial.print(F(" VU_LP="));Serial.print(extCfg.vuLowPass);
  Serial.print(F(" SPECTR_LP="));Serial.println(extCfg.spectrumLowPass);
}

void printAmbientStatus(){
  AmbientSettingsV1& a=extCfg.ambient;
  Serial.print(F("AMBIENT STATUS EFFECT="));Serial.print(ambientName());
  Serial.print(F(" AUTO="));Serial.print(a.autoCycle?F("ON"):F("OFF"));
  Serial.print(F(" PERIOD="));Serial.print(a.autoPeriodSec);
  Serial.print(F(" F01="));Serial.print(a.f01Hue);Serial.print(',');Serial.print(a.f01Sat);Serial.print(',');Serial.print(a.f01Brightness);
  Serial.print(F(" F02="));Serial.print(a.f02Hue);Serial.print(',');Serial.print(a.f02Sat);Serial.print(',');Serial.print(a.f02Brightness);Serial.print(',');Serial.print(a.f02Speed);
  Serial.print(F(" F03="));Serial.print(a.f03Hue);Serial.print(',');Serial.print(a.f03Brightness);Serial.print(',');Serial.print(a.f03Speed);Serial.print(',');Serial.println(a.f03Step10);
}

void printSystemStatus(){
  Serial.print(F("SYSTEM STATUS LIMIT_MA="));Serial.print(extCfg.currentLimitMa);
  Serial.print(F(" HARD_LIMIT_MA="));Serial.print(Cfg::HARD_CURRENT_LIMIT_MA);
  Serial.print(F(" EXT_SAVED="));Serial.print(extCfg.storageValid?F("YES"):F("NO"));
  Serial.print(F(" EXT_DIRTY="));Serial.println(extCfg.dirty?F("YES"):F("NO"));
}

bool parseSubmode(const char* p,uint8_t& out){
  if(strcmp(p,"THREE")==0)out=0;
  else if(strcmp(p,"LOW")==0)out=1;
  else if(strcmp(p,"MID")==0)out=2;
  else if(strcmp(p,"HIGH")==0)out=3;
  else return false;
  return true;
}

void handleSystem(char* a){
  if(strcmp(a,"STATUS")==0){printSystemStatus();return;}
  if(strcmp(a,"SAVE")==0){saveExtended();Serial.println(F("OK SYSTEM_SAVED"));return;}
  if(strncmp(a,"LIMIT ",6)==0){
    long v=0;if(!parseLongRange(a+6,Cfg::MIN_CURRENT_LIMIT_MA,Cfg::HARD_CURRENT_LIMIT_MA,v)){Serial.println(F("ERR BAD_LIMIT"));return;}
    extCfg.currentLimitMa=static_cast<uint16_t>(v);extCfg.dirty=true;
    FastLED.setMaxPowerInVoltsAndMilliamps(5,extCfg.currentLimitMa);
    saveExtended();Serial.print(F("OK LIMIT_MA="));Serial.println(extCfg.currentLimitMa);return;
  }
  Serial.println(F("ERR SYSTEM_COMMAND"));
}

void handleMusic(char* a){
  if(strcmp(a,"STATUS")==0){printMusicStatus();return;}
  if(strcmp(a,"ON")==0){applyMode(SystemMode::MUSIC,true);Serial.println(F("OK MODE=MUSIC"));return;}
  if(strcmp(a,"OFF")==0){applyMode(SystemMode::OFF,true);Serial.println(F("OK MODE=OFF"));return;}
  if(strcmp(a,"CAL")==0){calibrateAudio();return;}
  if(strcmp(a,"SAVE")==0){saveExtended();Serial.println(F("OK MUSIC_SAVED"));return;}
  if(strncmp(a,"MODE ",5)==0){
    MusicMode mm;if(!parseMusicMode(a+5,mm)){Serial.println(F("ERR BAD_MUSIC_MODE"));return;}
    extCfg.selectedMusic=mm;extCfg.dirty=true;saveExtended();applyMode(SystemMode::MUSIC,true);
    Serial.print(F("OK MUSIC_MODE="));Serial.println(musicName(mm));return;
  }
  MusicModeSettings& m=activeMusicCfg(); long v=0;
  if(strncmp(a,"BRIGHT ",7)==0){
    if(!parseLongRange(a+7,0,255,v)){Serial.println(F("ERR BAD_BRIGHTNESS"));return;}
    m.brightness=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"BACKGROUND ",11)==0){
    if(!parseLongRange(a+11,0,255,v)){Serial.println(F("ERR BAD_BACKGROUND"));return;}
    m.background=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SMOOTH ",7)==0){
    if(!parseLongRange(a+7,5,100,v)){Serial.println(F("ERR BAD_SMOOTH"));return;}
    m.smooth=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SENS ",5)==0){
    if(!parseLongRange(a+5,50,200,v)){Serial.println(F("ERR BAD_SENS"));return;}
    m.sensitivity=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SUBMODE ",8)==0){
    if(extCfg.selectedMusic!=MusicMode::M05&&extCfg.selectedMusic!=MusicMode::M08){Serial.println(F("ERR NOT_APPLICABLE"));return;}
    uint8_t sm;if(!parseSubmode(a+8,sm)){Serial.println(F("ERR BAD_SUBMODE"));return;}
    m.submode=sm;extCfg.dirty=true;resetMusicRuntime();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SPEED ",6)==0){
    if(extCfg.selectedMusic!=MusicMode::M08||!parseLongRange(a+6,1,255,v)){Serial.println(F("ERR BAD_SPEED"));return;}
    m.speed=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"RAINSTEP10 ",11)==0){
    if(extCfg.selectedMusic!=MusicMode::M02||!parseLongRange(a+11,5,200,v)){Serial.println(F("ERR BAD_RAINSTEP"));return;}
    m.aux=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"HUESTART ",9)==0){
    if(extCfg.selectedMusic!=MusicMode::M09||!parseLongRange(a+9,0,255,v)){Serial.println(F("ERR BAD_HUE_START"));return;}
    m.speed=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"HUESTEP ",8)==0){
    if(extCfg.selectedMusic!=MusicMode::M09||!parseLongRange(a+8,1,255,v)){Serial.println(F("ERR BAD_HUE_STEP"));return;}
    m.aux=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR MUSIC_COMMAND"));
}

void handleAmbient(char* a){
  AmbientSettingsV1& x=extCfg.ambient; long v=0;
  if(strcmp(a,"STATUS")==0){printAmbientStatus();return;}
  if(strcmp(a,"ON")==0){applyMode(SystemMode::AMBIENT,true);Serial.println(F("OK MODE=AMBIENT"));return;}
  if(strcmp(a,"OFF")==0){applyMode(SystemMode::OFF,true);Serial.println(F("OK MODE=OFF"));return;}
  if(strcmp(a,"SAVE")==0){saveExtended();Serial.println(F("OK AMBIENT_SAVED"));return;}
  if(strcmp(a,"NEXT")==0){nextAmbient(1);Serial.println(F("OK"));return;}
  if(strcmp(a,"PREV")==0){nextAmbient(-1);Serial.println(F("OK"));return;}
  if(strncmp(a,"EFFECT ",7)==0){
    if(strcmp(a+7,"F01")==0)x.effect=0;else if(strcmp(a+7,"F02")==0)x.effect=1;
    else if(strcmp(a+7,"F03")==0)x.effect=2;else{Serial.println(F("ERR BAD_EFFECT"));return;}
    extCfg.dirty=true;prepareAmbient();applyMode(SystemMode::AMBIENT,true);Serial.println(F("OK"));return;
  }
  if(strcmp(a,"AUTO ON")==0){x.autoCycle=1;extCfg.dirty=true;lastAmbientAutoMs=millis();Serial.println(F("OK"));return;}
  if(strcmp(a,"AUTO OFF")==0){x.autoCycle=0;extCfg.dirty=true;Serial.println(F("OK"));return;}
  if(strncmp(a,"PERIOD ",7)==0){
    if(!parseLongRange(a+7,1,255,v)){Serial.println(F("ERR BAD_PERIOD"));return;}x.autoPeriodSec=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"HUE ",4)==0){
    if(!parseLongRange(a+4,0,255,v)){Serial.println(F("ERR BAD_HUE"));return;}
    if(x.effect==0)x.f01Hue=v;else if(x.effect==1)x.f02Hue=v;else x.f03Hue=v;
    extCfg.dirty=true;prepareAmbient();Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SAT ",4)==0){
    if(!parseLongRange(a+4,0,255,v)){Serial.println(F("ERR BAD_SAT"));return;}
    if(x.effect==0)x.f01Sat=v;else if(x.effect==1)x.f02Sat=v;else{Serial.println(F("ERR NOT_APPLICABLE"));return;}
    extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"BRIGHT ",7)==0){
    if(!parseLongRange(a+7,0,255,v)){Serial.println(F("ERR BAD_BRIGHTNESS"));return;}
    if(x.effect==0)x.f01Brightness=v;else if(x.effect==1)x.f02Brightness=v;else x.f03Brightness=v;
    extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"SPEED ",6)==0){
    if(!parseLongRange(a+6,1,255,v)){Serial.println(F("ERR BAD_SPEED"));return;}
    if(x.effect==1)x.f02Speed=v;else if(x.effect==2)x.f03Speed=v;else{Serial.println(F("ERR NOT_APPLICABLE"));return;}
    extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  if(strncmp(a,"STEP10 ",7)==0){
    if(x.effect!=2||!parseLongRange(a+7,5,100,v)){Serial.println(F("ERR BAD_STEP10"));return;}
    x.f03Step10=v;extCfg.dirty=true;Serial.println(F("OK"));return;
  }
  Serial.println(F("ERR AMBIENT_COMMAND"));
}

void printAllSettings(){
  Serial.println(F("SETTINGS BEGIN V=1"));
  printLightStatus();printClapStatus();printMusicStatus();printAmbientStatus();printNightStatus();printAlarmDawnStatus();printSystemStatus();
  Serial.println(F("SETTINGS END"));
}

void handleCommand(char* cmd){
  resetClapDetector();

  if(strcmp(cmd,"PING")==0){Serial.println(F("PONG"));return;}
  if(strcmp(cmd,"STATUS")==0){printMainStatus();return;}
  if(strcmp(cmd,"TIME")==0){
    RtcTime now;if(!rtcValidNow(&now)){Serial.println(F("ERR RTC_INVALID"));return;}
    Serial.print(F("TIME "));printRtc(now);Serial.println();return;
  }
  if(strncmp(cmd,"SET ",4)==0){
    RtcTime t;if(!parseSetTime(cmd+4,t)){Serial.println(F("ERR BAD_TIME_FORMAT"));return;}
    if(!rtcSet(t)){Serial.println(F("ERR RTC_WRITE"));return;}
    rtcLostPower=false;Serial.print(F("OK TIME="));printRtc(t);Serial.println();return;
  }
  if(strcmp(cmd,"MODE LIGHT")==0||strcmp(cmd,"MODE L01")==0){applyMode(SystemMode::LIGHT,true);Serial.println(F("OK MODE=L01"));return;}
  if(strcmp(cmd,"MODE NIGHT")==0){applyMode(SystemMode::NIGHT,true);Serial.println(F("OK MODE=NIGHT"));return;}
  if(strcmp(cmd,"MODE AMBIENT")==0){applyMode(SystemMode::AMBIENT,true);Serial.println(F("OK MODE=AMBIENT"));return;}
  if(strcmp(cmd,"MODE MUSIC")==0){applyMode(SystemMode::MUSIC,true);Serial.println(F("OK MODE=MUSIC"));return;}
  if(strcmp(cmd,"MODE OFF")==0){applyMode(SystemMode::OFF,true);Serial.println(F("OK MODE=OFF"));return;}

  if(strncmp(cmd,"LIGHT ",6)==0){handleLight(cmd+6);return;}
  if(strncmp(cmd,"CLAP ",5)==0){handleClap(cmd+5);return;}
  if(strncmp(cmd,"CLAPCAL ",8)==0){handleClapCal(cmd+8);return;}
  if(strncmp(cmd,"NIGHT ",6)==0){handleNight(cmd+6);return;}
  if(strncmp(cmd,"ALARM ",6)==0){handleAlarm(cmd+6);return;}
  if(strncmp(cmd,"DAWN ",5)==0){handleDawn(cmd+5);return;}
  if(strncmp(cmd,"MUSIC ",6)==0){handleMusic(cmd+6);return;}
  if(strncmp(cmd,"AMBIENT ",8)==0){handleAmbient(cmd+8);return;}
  if(strncmp(cmd,"SYSTEM ",7)==0){handleSystem(cmd+7);return;}
  if(strcmp(cmd,"SETTINGS")==0){printAllSettings();return;}

  if(strcmp(cmd,"HELP")==0){
    Serial.println(F("STATUS SETTINGS TIME SET MODE LIGHT CLAP CLAPCAL NIGHT ALARM DAWN MUSIC AMBIENT SYSTEM PING"));
    return;
  }
  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial(){
  while(Serial.available()>0){
    const char c=static_cast<char>(Serial.read());
    lastSerialRxMs=millis();
    if(c=='\r')continue;
    if(c=='\n'){
      if(rxLength){
        rxBuffer[rxLength]='\0';handleCommand(rxBuffer);rxLength=0;
      }
      continue;
    }
    if(rxLength<Cfg::RX_BUFFER_SIZE-1)rxBuffer[rxLength++]=c;
    else{rxLength=0;Serial.println(F("ERR LINE_TOO_LONG"));}
  }
}

// -------------------- setup/loop --------------------

void setup(){
  classifyReset();

  Serial.begin(Cfg::SERIAL_BAUD);
  analogReference(DEFAULT);

  loadExtended();

  FastLED.addLeds<WS2812B,Pins::RING_A,GRB>(leds,Cfg::LED_COUNT);
  FastLED.addLeds<WS2812B,Pins::RING_B,GRB>(leds,Cfg::LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,extCfg.currentLimitMa);
  prepareOff();
  FastLED.show();

  Wire.begin();
  Wire.setClock(100000UL);
  delay(80);

  loadAlarm();
  loadDawnSettings();
  loadDawnRuntime();
  loadNight();
  loadLight();
  loadClap();
  loadCore();
  resetClapCalibration();

  clapVol.begin();
  resetClapDetector();

  rtcPresent=rtcProbe();
  if(rtcPresent){
    bool lost=true;
    if(rtcReadLostPower(lost))rtcLostPower=lost;
  }

  Serial.println(F("ARDU NANO ARDU_V1 R1 READY"));
  Serial.println(F("44+44 D6/D7 L01 CLAPCAL RTC NIGHT DAWN AMBIENT MUSIC"));

  const bool recovered=recoverDawn(coldBoot);

  if(!recovered){
    if(coldBoot){
      currentMode=SystemMode::LIGHT;
      prepareLight();
      saveCore();
      Serial.println(F("EVENT COLD_POWER_ON MODE=L01"));
    } else {
      applyMode(coreCfg.storageValid?coreCfg.lastMode:SystemMode::LIGHT,false);
      Serial.print(F("EVENT WARM_RESET RESTORE_MODE="));
      Serial.println(modeName(currentMode));
    }
  }

  showIfSafe();
  printMainStatus();
}

void loop(){
  pollSerial();
  updateAlarm();
  updateDawn();
  updateClap();
  updateAmbient();
  updateMusic();
  showIfSafe();
}
