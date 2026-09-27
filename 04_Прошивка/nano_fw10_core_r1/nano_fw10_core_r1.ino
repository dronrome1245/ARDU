/*
  ARDU FW-10 CORE R1
  First integrated Nano layer after subsystem hardware passes.

  Included:
  - 44 LED ring A on D6 (D7 intentionally held LOW in this one-ring core test)
  - L01 ordinary light: Kelvin/RGB/brightness + persisted startup profile
  - Gyver-style double-clap toggle with default TRSH=70 / TIMEOUT=500 ms
  - DS3231 RTC
  - Alarm + Dawn R2 settings/runtime recovery
  - Night R2 manual/schedule
  - reset/power-on state policy

  Not included yet:
  - Ambient F01/F02/F03
  - Music M01/M02/M03/M04/M05/M08/M09
  - second physical D7 ring
  - app-driven CLAPCAL wizard (kept in separate proven diagnostic branch)

  Old 43-LED test sketches are intentionally NOT modified.
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <avr/io.h>

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
constexpr uint16_t ONE_RING_TEST_LIMIT_MA = 500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 120;

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
  DAWN = 3
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

struct CoreState {
  SystemMode lastMode = SystemMode::LIGHT;
  bool clapEnabled = true;
  bool storageValid = false;
};

// -------------------- globals --------------------

AlarmSettings alarmCfg;
DawnSettings dawnCfg;
DawnRuntime dawnRuntime;
NightSettings nightCfg;
LightSettings lightCfg;
ClapSettings clapCfg;
CoreState coreCfg;

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

struct LightPersist {
  uint16_t magic;
  uint8_t version;
  uint8_t colorMode;
  uint8_t brightness;
  uint16_t kelvin;
  uint8_t r,g,b;
  uint8_t sum;
};

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

void loadClap() {
  uint8_t b[8];
  for(uint8_t i=0;i<8;++i) b[i]=EEPROM.read(Cfg::CLAP_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const uint16_t tr=static_cast<uint16_t>(b[3])|(static_cast<uint16_t>(b[4])<<8);
  const uint16_t to=static_cast<uint16_t>(b[5])|(static_cast<uint16_t>(b[6])<<8);
  const bool ok=magic==Cfg::CLAP_MAGIC && b[2]==Cfg::CLAP_VER &&
      EEPROM.read(Cfg::CLAP_BASE+8)==checksum(b,8) &&
      tr>=20 && tr<=300 && to>=250 && to<=1200;
  if(!ok){ clapCfg=ClapSettings(); return; }
  clapCfg.threshold=tr; clapCfg.timeoutMs=to; clapCfg.storageValid=true; clapCfg.dirty=false;
}

void saveClap() {
  uint8_t b[8];
  b[0]=Cfg::CLAP_MAGIC&0xFF; b[1]=Cfg::CLAP_MAGIC>>8; b[2]=Cfg::CLAP_VER;
  b[3]=clapCfg.threshold&0xFF; b[4]=clapCfg.threshold>>8;
  b[5]=clapCfg.timeoutMs&0xFF; b[6]=clapCfg.timeoutMs>>8; b[7]=0;
  for(uint8_t i=0;i<8;++i) EEPROM.update(Cfg::CLAP_BASE+i,b[i]);
  EEPROM.update(Cfg::CLAP_BASE+8,checksum(b,8));
  clapCfg.storageValid=true; clapCfg.dirty=false;
}

void loadCore() {
  uint8_t b[5];
  for(uint8_t i=0;i<5;++i) b[i]=EEPROM.read(Cfg::CORE_BASE+i);
  const uint16_t magic=static_cast<uint16_t>(b[0])|(static_cast<uint16_t>(b[1])<<8);
  const bool ok=magic==Cfg::CORE_MAGIC && b[2]==Cfg::CORE_VER &&
      b[3]<=static_cast<uint8_t>(SystemMode::NIGHT) && b[4]<=1 &&
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

// -------------------- Light color/render --------------------

struct TempAnchor { uint16_t k; uint8_t r,g,b; };
const TempAnchor TEMP[] = {
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

CRGB kelvinRgb(uint16_t k){
  if(k<=TEMP[0].k) return CRGB(TEMP[0].r,TEMP[0].g,TEMP[0].b);
  for(uint8_t i=0;i<TEMP_N-1;++i){
    if(k<=TEMP[i+1].k){
      const uint16_t n=k-TEMP[i].k, d=TEMP[i+1].k-TEMP[i].k;
      return CRGB(
        lerp8(TEMP[i].r,TEMP[i+1].r,n,d),
        lerp8(TEMP[i].g,TEMP[i+1].g,n,d),
        lerp8(TEMP[i].b,TEMP[i+1].b,n,d));
    }
  }
  return CRGB(TEMP[TEMP_N-1].r,TEMP[TEMP_N-1].g,TEMP[TEMP_N-1].b);
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

// -------------------- Gyver-style clap --------------------

class RawEnvelope {
public:
  void begin(){
    pinMode(Pins::MIC_IN,INPUT);
    _tmrSample=micros(); _tmrPeriod=millis(); _tmrAmpli=millis();
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
    if(++_count>=Cfg::CLAP_VOL_WINDOW){
      _tmrPeriod=ms; _raw=_windowMax;
      if(_windowMax>_maxs)_maxs=_windowMax;
      _rawMax=_maxs; _windowMax=0; _count=0; return true;
    }
    return false;
  }
  uint16_t rawMax()const{return _rawMax;}
private:
  uint16_t _windowMax=0,_raw=0,_rawMax=0,_maxs=0;
  uint8_t _count=0;
  unsigned long _tmrSample=0,_tmrPeriod=0,_tmrAmpli=0;
};

class ClapDetector {
public:
  void configure(int tr,uint16_t to){_tr=tr;_tout=to;}
  void reset(){
    _tmr=millis();_tmr2=millis();_prev=0;_primed=false;_prevSignal=0;
    _state=0;_claps=0;_ready=false;_clap=false;_start=false;
  }
  void tick(int val){
    if(millis()-_tmr<10)return; _tmr=millis();
    if(!_primed){_prev=val;_primed=true;return;}
    const int der=val-_prev; _prev=val;
    int signal=0,front=0;
    if(der>_tr)signal=1; if(der<-_tr)signal=-1;
    if(_prevSignal==0&&signal==1)front=1;
    if(_prevSignal==0&&signal==-1)front=-1;
    _prevSignal=signal;
    const uint32_t deb=millis()-_tmr2;
    if(front==1&&_state==0){
      _state=1;
      if(!_start){_claps=0;_ready=false;}
      _start=true;_clap=false;_tmr2=millis();
    } else if(front==-1&&_state==1&&deb<=200){
      _state=2;_tmr2=millis();
    } else if(front==0&&_state==2&&deb<=200){
      _state=0;++_claps;_clap=true;_tmr2=millis();
    } else if(_start&&deb>_tout){
      _state=0;_start=false;if(_claps)_ready=true;
    }
  }
  bool takeSequence(uint8_t& c){
    if(!_ready)return false;_ready=false;c=_claps;_claps=0;return true;
  }
private:
  unsigned long _tmr=0,_tmr2=0;
  int _prev=0,_tr=Cfg::DEFAULT_CLAP_TRSH;
  uint16_t _tout=Cfg::DEFAULT_CLAP_TIMEOUT_MS;
  uint8_t _state=0,_claps=0;
  int8_t _prevSignal=0;
  bool _primed=false,_ready=false,_clap=false,_start=false;
};

RawEnvelope clapVol;
ClapDetector clapDetector;
unsigned long clapIgnoreUntil=0;

void resetClapDetector(){
  clapDetector.configure(clapCfg.threshold,clapCfg.timeoutMs);
  clapDetector.reset();
  clapIgnoreUntil=millis()+Cfg::CLAP_COMMAND_GUARD_MS;
}

void toggleLightByClap(){
  if(currentMode==SystemMode::LIGHT){
    currentMode=SystemMode::OFF; saveCore(); prepareOff();
    Serial.println(F("EVENT CLAP_TOGGLE LIGHT=OFF"));
  } else if(currentMode==SystemMode::OFF){
    currentMode=SystemMode::LIGHT; saveCore(); prepareLight();
    Serial.println(F("EVENT CLAP_TOGGLE LIGHT=ON"));
  }
}

void updateClap(){
  clapVol.tick();
  if(!coreCfg.clapEnabled)return;
  if(currentMode!=SystemMode::LIGHT && currentMode!=SystemMode::OFF)return;
  if(static_cast<long>(millis()-clapIgnoreUntil)<0)return;

  clapDetector.tick(clapVol.rawMax());
  uint8_t count=0;
  if(clapDetector.takeSequence(count)){
    if(count==2)toggleLightByClap();
  }
}

// -------------------- mode/render/reset --------------------

void applyMode(SystemMode m,bool persist){
  currentMode=m;
  if(m==SystemMode::LIGHT)prepareLight();
  else if(m==SystemMode::NIGHT)(void)reconcileNight(false);
  else if(m==SystemMode::OFF)prepareOff();
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
  Serial.print(F("STATUS FW=FW10_CORE REV="));Serial.print(Cfg::FW_REV);
  Serial.print(F(" MODE="));Serial.print(modeName(currentMode));
  Serial.print(F(" RESET="));Serial.print(resetName());
  Serial.print(F(" MCUSR="));Serial.print(rawResetFlags);
  Serial.print(F(" LEDS="));Serial.print(Cfg::LED_COUNT);
  Serial.print(F(" RING_A=D6 RING_B=DISCONNECTED_TEST"));
  Serial.print(F(" LIMIT_MA="));Serial.print(Cfg::ONE_RING_TEST_LIMIT_MA);
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
  Serial.print(F(" CALIBRATION=APP_LATER"));
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

void handleCommand(char* cmd){
  clapIgnoreUntil=millis()+Cfg::CLAP_COMMAND_GUARD_MS;

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
  if(strcmp(cmd,"MODE OFF")==0){applyMode(SystemMode::OFF,true);Serial.println(F("OK MODE=OFF"));return;}

  if(strncmp(cmd,"LIGHT ",6)==0){handleLight(cmd+6);return;}
  if(strncmp(cmd,"CLAP ",5)==0){handleClap(cmd+5);return;}
  if(strncmp(cmd,"NIGHT ",6)==0){handleNight(cmd+6);return;}
  if(strncmp(cmd,"ALARM ",6)==0){handleAlarm(cmd+6);return;}
  if(strncmp(cmd,"DAWN ",5)==0){handleDawn(cmd+5);return;}

  if(strcmp(cmd,"HELP")==0){
    Serial.println(F("CMDS STATUS TIME SET MODE LIGHT CLAP NIGHT ALARM DAWN PING HELP"));
    Serial.println(F("MODE L01|NIGHT|OFF"));
    Serial.println(F("LIGHT ON|OFF|STATUS|BRIGHT|KELVIN|PRESET|RGB|SAVE|LOAD|CLAP ON|OFF"));
    Serial.println(F("CLAP STATUS|TRSH|TIMEOUT|SAVE|DEFAULTS"));
    Serial.println(F("NIGHT ON|OFF|STATUS|HUE|SAT|BRIGHT|SCHEDULESET|SCHEDULE ON|OFF"));
    Serial.println(F("ALARM ON|OFF|STATUS|SET HH:MM|CLEARLAST"));
    Serial.println(F("DAWN START|STOP|STATUS|TEST|FADEMIN|MAXBR|STARTHUE|ENDHUE"));
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

  pinMode(Pins::RING_B,OUTPUT);
  digitalWrite(Pins::RING_B,LOW);

  FastLED.addLeds<WS2812B,Pins::RING_A,GRB>(leds,Cfg::LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,Cfg::ONE_RING_TEST_LIMIT_MA);
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

  clapVol.begin();
  resetClapDetector();

  rtcPresent=rtcProbe();
  if(rtcPresent){
    bool lost=true;
    if(rtcReadLostPower(lost))rtcLostPower=lost;
  }

  Serial.println(F("ARDU NANO FW10 CORE R1 READY"));
  Serial.println(F("44 LED / ONE-RING SAFE LIMIT / L01+CLAP+RTC+ALARM+DAWN+NIGHT"));

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
  showIfSafe();
}
