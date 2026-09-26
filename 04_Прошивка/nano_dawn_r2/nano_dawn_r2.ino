/*
  ARDU DAWN R2 — DS3231 daily alarm + dawn + reset recovery.

  This layer extends the passed ALARM R1:
  - keeps ALARM R1 EEPROM layout compatible at offsets 0..9;
  - adds separate persisted dawn settings at EEPROM offset 16;
  - alarm trigger starts the dawn renderer;
  - manual accelerated DAWN TEST validates the same renderer quickly;
  - DAWN STOP immediately turns the ring off.

  R2 persists only dawn runtime transitions (start/complete/stop) and uses
  DS3231 wall-clock time to reconstruct progress after Nano reset.
  No periodic EEPROM writes are used for dawn progress.

  Hardware:
  - DS3231 SDA -> A4, SCL -> A5
  - WS2812B ring A -> D6, 43 LEDs
  - D7 held LOW until second-ring testing
  - FastLED/UART coexistence fix inherited from M05 R4
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 2;
constexpr uint8_t DS3231_ADDRESS = 0x68;
constexpr size_t RX_BUFFER_SIZE = 64;

constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr unsigned long RTC_POLL_MS = 250UL;
constexpr unsigned long DAWN_FRAME_MS = 50UL;

// ALARM R1-compatible EEPROM block, offsets 0..9.
constexpr uint16_t ALARM_EEPROM_MAGIC = 0xA8D1;
constexpr uint8_t ALARM_EEPROM_VERSION = 1;
constexpr int ALARM_EEPROM_BASE = 0;

// DAWN R1 separate EEPROM block.
constexpr uint16_t DAWN_EEPROM_MAGIC = 0xDA01;
constexpr uint8_t DAWN_EEPROM_VERSION = 1;
constexpr int DAWN_EEPROM_BASE = 16;

// DAWN R2 runtime state. Written only on START / COMPLETE / STOP.
constexpr uint16_t RUNTIME_EEPROM_MAGIC = 0xDA02;
constexpr uint8_t RUNTIME_EEPROM_VERSION = 1;
constexpr int RUNTIME_EEPROM_BASE = 32;

constexpr uint8_t DEFAULT_ALARM_HOUR = 7;
constexpr uint8_t DEFAULT_ALARM_MINUTE = 0;

constexpr uint8_t DEFAULT_FADE_MINUTES = 30;
constexpr uint8_t DEFAULT_MAX_BRIGHTNESS = 120;
constexpr uint8_t DEFAULT_START_HUE = 8;
constexpr uint8_t DEFAULT_END_HUE = 32;
}

struct RtcTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

struct AlarmSettings {
  bool enabled = false;
  uint8_t hour = ArduConfig::DEFAULT_ALARM_HOUR;
  uint8_t minute = ArduConfig::DEFAULT_ALARM_MINUTE;

  uint16_t lastTriggerYear = 0;
  uint8_t lastTriggerMonth = 0;
  uint8_t lastTriggerDay = 0;

  bool storageValid = false;
};

struct DawnSettings {
  uint8_t fadeMinutes = ArduConfig::DEFAULT_FADE_MINUTES;
  uint8_t maxBrightness = ArduConfig::DEFAULT_MAX_BRIGHTNESS;
  uint8_t startHue = ArduConfig::DEFAULT_START_HUE;
  uint8_t endHue = ArduConfig::DEFAULT_END_HUE;
  bool storageValid = false;
};

enum class DawnPhase : uint8_t {
  IDLE,
  RUNNING,
  HOLD
};

enum class DawnSource : uint8_t {
  NONE,
  ALARM,
  MANUAL,
  TEST
};

struct DawnRuntime {
  DawnPhase phase = DawnPhase::IDLE;
  DawnSource source = DawnSource::NONE;
  uint32_t startRtcSeconds = 0;
  uint32_t durationSeconds = 0;
  bool storageValid = false;
};

AlarmSettings alarm;
DawnSettings dawnSettings;
DawnPhase dawnPhase = DawnPhase::IDLE;
DawnRuntime dawnRuntime;
bool recoveredAtBoot = false;

CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

bool rtcPresent = false;
bool rtcLostPower = true;
bool lastReadOk = false;

unsigned long lastRtcPollMs = 0;
unsigned long lastSerialRxMs = 0;
unsigned long lastDawnFrameMs = 0;
unsigned long dawnStartMs = 0;
unsigned long dawnDurationMs = 0;
bool frameDirty = false;

uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

uint8_t decToBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

bool isLeapYear(uint16_t year) {
  if ((year % 400) == 0) return true;
  if ((year % 100) == 0) return false;
  return (year % 4) == 0;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t kDays[] = {
      31, 28, 31, 30, 31, 30,
      31, 31, 30, 31, 30, 31
  };

  if (month < 1 || month > 12) return 0;
  if (month == 2 && isLeapYear(year)) return 29;
  return kDays[month - 1];
}

bool validTime(const RtcTime& t) {
  if (t.year < 2000 || t.year > 2099) return false;
  if (t.month < 1 || t.month > 12) return false;
  if (t.day < 1 || t.day > daysInMonth(t.year, t.month)) return false;
  if (t.hour > 23 || t.minute > 59 || t.second > 59) return false;
  return true;
}

bool probeRtc() {
  Wire.beginTransmission(ArduConfig::DS3231_ADDRESS);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t startRegister, uint8_t* data, uint8_t length) {
  Wire.beginTransmission(ArduConfig::DS3231_ADDRESS);
  Wire.write(startRegister);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received =
      Wire.requestFrom(ArduConfig::DS3231_ADDRESS, length);

  if (received != length) {
    while (Wire.available()) {
      (void)Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    if (!Wire.available()) return false;
    data[i] = Wire.read();
  }

  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ArduConfig::DS3231_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readLostPower(bool& lostPower) {
  uint8_t status = 0;
  if (!readRegisters(0x0F, &status, 1)) return false;
  lostPower = (status & 0x80) != 0;
  return true;
}

bool clearLostPowerFlag() {
  uint8_t status = 0;
  if (!readRegisters(0x0F, &status, 1)) return false;
  status &= static_cast<uint8_t>(~0x80);
  return writeRegister(0x0F, status);
}

bool readRtcTime(RtcTime& t) {
  uint8_t data[7] = {0};

  if (!readRegisters(0x00, data, sizeof(data))) {
    return false;
  }

  t.second = bcdToDec(data[0] & 0x7F);
  t.minute = bcdToDec(data[1] & 0x7F);

  const uint8_t hourReg = data[2];
  if (hourReg & 0x40) {
    uint8_t hour12 = bcdToDec(hourReg & 0x1F);
    const bool pm = (hourReg & 0x20) != 0;

    if (hour12 == 12) {
      t.hour = pm ? 12 : 0;
    } else {
      t.hour = static_cast<uint8_t>(hour12 + (pm ? 12 : 0));
    }
  } else {
    t.hour = bcdToDec(hourReg & 0x3F);
  }

  t.day = bcdToDec(data[4] & 0x3F);
  t.month = bcdToDec(data[5] & 0x1F);
  t.year = static_cast<uint16_t>(2000 + bcdToDec(data[6]));

  return validTime(t);
}

bool setRtcTime(const RtcTime& t) {
  if (!validTime(t)) return false;

  Wire.beginTransmission(ArduConfig::DS3231_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(t.second));
  Wire.write(decToBcd(t.minute));
  Wire.write(decToBcd(t.hour));
  Wire.write(decToBcd(1));
  Wire.write(decToBcd(t.day));
  Wire.write(decToBcd(t.month));
  Wire.write(decToBcd(static_cast<uint8_t>(t.year - 2000)));

  if (Wire.endTransmission() != 0) return false;
  return clearLostPowerFlag();
}

void print2(uint8_t value) {
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void print4(uint16_t value) {
  if (value < 1000) Serial.print('0');
  if (value < 100) Serial.print('0');
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void printRtcTime(const RtcTime& t) {
  print4(t.year);
  Serial.print('-');
  print2(t.month);
  Serial.print('-');
  print2(t.day);
  Serial.print(' ');
  print2(t.hour);
  Serial.print(':');
  print2(t.minute);
  Serial.print(':');
  print2(t.second);
}

void printAlarmTime() {
  print2(alarm.hour);
  Serial.print(':');
  print2(alarm.minute);
}

uint32_t rtcSecondsSince2000(const RtcTime& t) {
  uint32_t days = 0;

  for (uint16_t year = 2000; year < t.year; ++year) {
    days += isLeapYear(year) ? 366UL : 365UL;
  }

  for (uint8_t month = 1; month < t.month; ++month) {
    days += daysInMonth(t.year, month);
  }

  days += static_cast<uint32_t>(t.day - 1);

  return days * 86400UL +
         static_cast<uint32_t>(t.hour) * 3600UL +
         static_cast<uint32_t>(t.minute) * 60UL +
         t.second;
}

uint8_t computeChecksum(const uint8_t* bytes, uint8_t length) {
  uint8_t checksum = 0x5A;

  for (uint8_t i = 0; i < length; ++i) {
    checksum = static_cast<uint8_t>((checksum << 1) | (checksum >> 7));
    checksum ^= bytes[i];
  }

  return checksum;
}

uint8_t eepromReadAt(int base, uint8_t offset) {
  return EEPROM.read(base + offset);
}

void eepromUpdateAt(int base, uint8_t offset, uint8_t value) {
  EEPROM.update(base + offset, value);
}

void saveAlarmSettings() {
  uint8_t bytes[9];
  bytes[0] = static_cast<uint8_t>(ArduConfig::ALARM_EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::ALARM_EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::ALARM_EEPROM_VERSION;
  bytes[3] = alarm.enabled ? 1 : 0;
  bytes[4] = alarm.hour;
  bytes[5] = alarm.minute;
  bytes[6] =
      (alarm.lastTriggerYear >= 2000 && alarm.lastTriggerYear <= 2099)
          ? static_cast<uint8_t>(alarm.lastTriggerYear - 2000)
          : 0xFF;
  bytes[7] = alarm.lastTriggerMonth;
  bytes[8] = alarm.lastTriggerDay;

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    eepromUpdateAt(ArduConfig::ALARM_EEPROM_BASE, i, bytes[i]);
  }

  eepromUpdateAt(
      ArduConfig::ALARM_EEPROM_BASE,
      9,
      computeChecksum(bytes, sizeof(bytes))
  );

  alarm.storageValid = true;
}

void loadAlarmSettings() {
  uint8_t bytes[9];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = eepromReadAt(ArduConfig::ALARM_EEPROM_BASE, i);
  }

  const uint8_t storedChecksum =
      eepromReadAt(ArduConfig::ALARM_EEPROM_BASE, 9);
  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const bool valid =
      magic == ArduConfig::ALARM_EEPROM_MAGIC &&
      bytes[2] == ArduConfig::ALARM_EEPROM_VERSION &&
      storedChecksum == computeChecksum(bytes, sizeof(bytes)) &&
      bytes[3] <= 1 &&
      bytes[4] <= 23 &&
      bytes[5] <= 59 &&
      bytes[7] <= 12 &&
      bytes[8] <= 31;

  if (!valid) {
    alarm = AlarmSettings();
    alarm.storageValid = false;
    return;
  }

  alarm.enabled = bytes[3] != 0;
  alarm.hour = bytes[4];
  alarm.minute = bytes[5];

  if (bytes[6] == 0xFF || bytes[7] == 0 || bytes[8] == 0) {
    alarm.lastTriggerYear = 0;
    alarm.lastTriggerMonth = 0;
    alarm.lastTriggerDay = 0;
  } else {
    alarm.lastTriggerYear = static_cast<uint16_t>(2000 + bytes[6]);
    alarm.lastTriggerMonth = bytes[7];
    alarm.lastTriggerDay = bytes[8];
  }

  alarm.storageValid = true;
}

void saveDawnSettings() {
  uint8_t bytes[7];
  bytes[0] = static_cast<uint8_t>(ArduConfig::DAWN_EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::DAWN_EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::DAWN_EEPROM_VERSION;
  bytes[3] = dawnSettings.fadeMinutes;
  bytes[4] = dawnSettings.maxBrightness;
  bytes[5] = dawnSettings.startHue;
  bytes[6] = dawnSettings.endHue;

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    eepromUpdateAt(ArduConfig::DAWN_EEPROM_BASE, i, bytes[i]);
  }

  eepromUpdateAt(
      ArduConfig::DAWN_EEPROM_BASE,
      7,
      computeChecksum(bytes, sizeof(bytes))
  );

  dawnSettings.storageValid = true;
}

void loadDawnSettings() {
  uint8_t bytes[7];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = eepromReadAt(ArduConfig::DAWN_EEPROM_BASE, i);
  }

  const uint8_t storedChecksum =
      eepromReadAt(ArduConfig::DAWN_EEPROM_BASE, 7);
  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const bool valid =
      magic == ArduConfig::DAWN_EEPROM_MAGIC &&
      bytes[2] == ArduConfig::DAWN_EEPROM_VERSION &&
      storedChecksum == computeChecksum(bytes, sizeof(bytes)) &&
      bytes[3] >= 1 &&
      bytes[3] <= 120 &&
      bytes[4] >= 1;

  if (!valid) {
    dawnSettings = DawnSettings();
    dawnSettings.storageValid = false;
    return;
  }

  dawnSettings.fadeMinutes = bytes[3];
  dawnSettings.maxBrightness = bytes[4];
  dawnSettings.startHue = bytes[5];
  dawnSettings.endHue = bytes[6];
  dawnSettings.storageValid = true;
}

void writeUint32Le(uint8_t* dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFF);
  dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

uint32_t readUint32Le(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8) |
         (static_cast<uint32_t>(src[2]) << 16) |
         (static_cast<uint32_t>(src[3]) << 24);
}

void saveDawnRuntime() {
  uint8_t bytes[13];
  bytes[0] = static_cast<uint8_t>(ArduConfig::RUNTIME_EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::RUNTIME_EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::RUNTIME_EEPROM_VERSION;
  bytes[3] = static_cast<uint8_t>(dawnRuntime.phase);
  bytes[4] = static_cast<uint8_t>(dawnRuntime.source);
  writeUint32Le(bytes + 5, dawnRuntime.startRtcSeconds);
  writeUint32Le(bytes + 9, dawnRuntime.durationSeconds);

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    eepromUpdateAt(ArduConfig::RUNTIME_EEPROM_BASE, i, bytes[i]);
  }

  eepromUpdateAt(
      ArduConfig::RUNTIME_EEPROM_BASE,
      13,
      computeChecksum(bytes, sizeof(bytes))
  );

  dawnRuntime.storageValid = true;
}

void loadDawnRuntime() {
  uint8_t bytes[13];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = eepromReadAt(ArduConfig::RUNTIME_EEPROM_BASE, i);
  }

  const uint8_t storedChecksum =
      eepromReadAt(ArduConfig::RUNTIME_EEPROM_BASE, 13);
  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const uint8_t rawPhase = bytes[3];
  const uint8_t rawSource = bytes[4];

  const bool valid =
      magic == ArduConfig::RUNTIME_EEPROM_MAGIC &&
      bytes[2] == ArduConfig::RUNTIME_EEPROM_VERSION &&
      storedChecksum == computeChecksum(bytes, sizeof(bytes)) &&
      rawPhase <= static_cast<uint8_t>(DawnPhase::HOLD) &&
      rawSource <= static_cast<uint8_t>(DawnSource::TEST);

  if (!valid) {
    dawnRuntime = DawnRuntime();
    dawnRuntime.storageValid = false;
    return;
  }

  dawnRuntime.phase = static_cast<DawnPhase>(rawPhase);
  dawnRuntime.source = static_cast<DawnSource>(rawSource);
  dawnRuntime.startRtcSeconds = readUint32Le(bytes + 5);
  dawnRuntime.durationSeconds = readUint32Le(bytes + 9);
  dawnRuntime.storageValid = true;
}

void setRuntimeIdle() {
  dawnRuntime.phase = DawnPhase::IDLE;
  dawnRuntime.source = DawnSource::NONE;
  dawnRuntime.startRtcSeconds = 0;
  dawnRuntime.durationSeconds = 0;
  saveDawnRuntime();
}

bool alreadyTriggeredToday(const RtcTime& t) {
  return alarm.lastTriggerYear == t.year &&
         alarm.lastTriggerMonth == t.month &&
         alarm.lastTriggerDay == t.day;
}

void markTriggeredToday(const RtcTime& t) {
  alarm.lastTriggerYear = t.year;
  alarm.lastTriggerMonth = t.month;
  alarm.lastTriggerDay = t.day;
  saveAlarmSettings();
}

const __FlashStringHelper* dawnPhaseName() {
  switch (dawnPhase) {
    case DawnPhase::IDLE: return F("IDLE");
    case DawnPhase::RUNNING: return F("RUNNING");
    case DawnPhase::HOLD: return F("HOLD");
  }
  return F("UNKNOWN");
}

const __FlashStringHelper* dawnSourceName(DawnSource source) {
  switch (source) {
    case DawnSource::NONE: return F("NONE");
    case DawnSource::ALARM: return F("ALARM");
    case DawnSource::MANUAL: return F("MANUAL");
    case DawnSource::TEST: return F("TEST");
  }
  return F("UNKNOWN");
}

uint8_t interpolateHue(uint8_t startHue, uint8_t endHue, uint16_t progress1000) {
  int16_t delta =
      static_cast<int16_t>(endHue) - static_cast<int16_t>(startHue);

  // Shortest circular HSV path.
  if (delta > 127) delta -= 256;
  if (delta < -128) delta += 256;

  const int32_t scaled =
      static_cast<int32_t>(delta) * progress1000 / 1000L;

  return static_cast<uint8_t>(
      static_cast<int16_t>(startHue) + static_cast<int16_t>(scaled)
  );
}

void prepareDawnFrame(uint16_t progress1000) {
  if (progress1000 > 1000) progress1000 = 1000;

  const uint8_t hue =
      interpolateHue(
          dawnSettings.startHue,
          dawnSettings.endHue,
          progress1000
      );

  const uint8_t brightness =
      static_cast<uint8_t>(
          static_cast<uint32_t>(dawnSettings.maxBrightness) *
          progress1000 / 1000UL
      );

  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      CHSV(hue, 255, 255)
  );
  FastLED.setBrightness(brightness);
  frameDirty = true;
}

void prepareOffFrame() {
  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  FastLED.setBrightness(0);
  frameDirty = true;
}

void showFrameIfSafe() {
  if (!frameDirty || serialRxGuardActive()) {
    return;
  }

  FastLED.show();
  frameDirty = false;
}

void startDawn(unsigned long durationMs, DawnSource source) {
  if (durationMs == 0) durationMs = 1000UL;

  dawnDurationMs = durationMs;
  dawnStartMs = millis();
  lastDawnFrameMs = 0;
  dawnPhase = DawnPhase::RUNNING;
  recoveredAtBoot = false;

  RtcTime now;
  if (rtcPresent &&
      !rtcLostPower &&
      readRtcTime(now)) {
    dawnRuntime.phase = DawnPhase::RUNNING;
    dawnRuntime.source = source;
    dawnRuntime.startRtcSeconds = rtcSecondsSince2000(now);
    dawnRuntime.durationSeconds =
        static_cast<uint32_t>((durationMs + 999UL) / 1000UL);
    saveDawnRuntime();
  } else {
    // Renderer may still run, but reset recovery is not safe without valid RTC.
    dawnRuntime = DawnRuntime();
    dawnRuntime.storageValid = false;
  }

  prepareDawnFrame(0);

  Serial.print(F("EVENT DAWN_START SOURCE="));
  Serial.print(dawnSourceName(source));
  Serial.print(F(" DURATION_S="));
  Serial.println(dawnDurationMs / 1000UL);
}

void stopDawn() {
  dawnPhase = DawnPhase::IDLE;
  dawnDurationMs = 0;
  prepareOffFrame();
  setRuntimeIdle();
  Serial.println(F("EVENT DAWN_STOP"));
}

void updateDawn() {
  if (dawnPhase != DawnPhase::RUNNING) {
    showFrameIfSafe();
    return;
  }

  const unsigned long now = millis();

  if (now - lastDawnFrameMs < ArduConfig::DAWN_FRAME_MS) {
    showFrameIfSafe();
    return;
  }
  lastDawnFrameMs = now;

  const unsigned long elapsed = now - dawnStartMs;

  if (elapsed >= dawnDurationMs) {
    prepareDawnFrame(1000);
    dawnPhase = DawnPhase::HOLD;

    dawnRuntime.phase = DawnPhase::HOLD;
    if (dawnRuntime.source == DawnSource::NONE) {
      dawnRuntime.source = DawnSource::MANUAL;
    }
    saveDawnRuntime();

    Serial.println(F("EVENT DAWN_COMPLETE"));
    showFrameIfSafe();
    return;
  }

  const uint16_t progress1000 =
      static_cast<uint16_t>(
          (static_cast<uint64_t>(elapsed) * 1000ULL) /
          dawnDurationMs
      );

  prepareDawnFrame(progress1000);
  showFrameIfSafe();
}

void emitAlarmTrigger(const RtcTime& t) {
  Serial.print(F("EVENT ALARM_TRIGGER RTC="));
  printRtcTime(t);
  Serial.println();
}

void updateAlarmTrigger() {
  const unsigned long nowMs = millis();
  if (nowMs - lastRtcPollMs < ArduConfig::RTC_POLL_MS) {
    return;
  }
  lastRtcPollMs = nowMs;

  rtcPresent = probeRtc();
  if (!rtcPresent) {
    lastReadOk = false;
    return;
  }

  bool lostPower = true;
  if (!readLostPower(lostPower)) {
    lastReadOk = false;
    return;
  }
  rtcLostPower = lostPower;

  RtcTime now;
  if (!readRtcTime(now)) {
    lastReadOk = false;
    return;
  }

  lastReadOk = true;

  if (!alarm.enabled || rtcLostPower) return;

  if (now.hour == alarm.hour &&
      now.minute == alarm.minute &&
      !alreadyTriggeredToday(now)) {
    markTriggeredToday(now);
    emitAlarmTrigger(now);

    const unsigned long durationMs =
        static_cast<unsigned long>(dawnSettings.fadeMinutes) *
        60UL * 1000UL;

    startDawn(durationMs, DawnSource::ALARM);
  }
}

bool parseAlarmTime(const char* text, uint8_t& hour, uint8_t& minute) {
  if (strlen(text) != 5 || text[2] != ':') return false;

  if (text[0] < '0' || text[0] > '9' ||
      text[1] < '0' || text[1] > '9' ||
      text[3] < '0' || text[3] > '9' ||
      text[4] < '0' || text[4] > '9') {
    return false;
  }

  hour = static_cast<uint8_t>(
      (text[0] - '0') * 10 + (text[1] - '0')
  );
  minute = static_cast<uint8_t>(
      (text[3] - '0') * 10 + (text[4] - '0')
  );

  return hour <= 23 && minute <= 59;
}

bool parseByte(const char* text, uint8_t& value) {
  if (text == nullptr || *text == '\0') return false;

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);

  if (*end != '\0' || parsed < 0 || parsed > 255) return false;

  value = static_cast<uint8_t>(parsed);
  return true;
}

bool parseFixedUInt(const char* text, uint8_t length, uint16_t& value) {
  value = 0;

  for (uint8_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') return false;
    value = static_cast<uint16_t>(value * 10 + (c - '0'));
  }

  return true;
}

bool parseSetTime(const char* text, RtcTime& t) {
  if (strlen(text) != 19) return false;

  if (text[4] != '-' ||
      text[7] != '-' ||
      text[10] != ' ' ||
      text[13] != ':' ||
      text[16] != ':') {
    return false;
  }

  uint16_t year, month, day, hour, minute, second;

  if (!parseFixedUInt(text + 0, 4, year)) return false;
  if (!parseFixedUInt(text + 5, 2, month)) return false;
  if (!parseFixedUInt(text + 8, 2, day)) return false;
  if (!parseFixedUInt(text + 11, 2, hour)) return false;
  if (!parseFixedUInt(text + 14, 2, minute)) return false;
  if (!parseFixedUInt(text + 17, 2, second)) return false;

  t.year = year;
  t.month = static_cast<uint8_t>(month);
  t.day = static_cast<uint8_t>(day);
  t.hour = static_cast<uint8_t>(hour);
  t.minute = static_cast<uint8_t>(minute);
  t.second = static_cast<uint8_t>(second);

  return validTime(t);
}

void recoverDawnAfterReset() {
  recoveredAtBoot = false;

  if (!dawnRuntime.storageValid ||
      dawnRuntime.phase == DawnPhase::IDLE) {
    dawnPhase = DawnPhase::IDLE;
    prepareOffFrame();
    return;
  }

  if (dawnRuntime.phase == DawnPhase::HOLD) {
    dawnPhase = DawnPhase::HOLD;
    prepareDawnFrame(1000);
    recoveredAtBoot = true;

    Serial.print(F("EVENT DAWN_RECOVER PHASE=HOLD SOURCE="));
    Serial.println(dawnSourceName(dawnRuntime.source));
    return;
  }

  if (!rtcPresent || rtcLostPower ||
      dawnRuntime.startRtcSeconds == 0 ||
      dawnRuntime.durationSeconds == 0) {
    dawnPhase = DawnPhase::IDLE;
    prepareOffFrame();
    setRuntimeIdle();
    Serial.println(F("EVENT DAWN_RECOVER_ABORT REASON=RTC_OR_STATE_INVALID"));
    return;
  }

  RtcTime now;
  if (!readRtcTime(now)) {
    dawnPhase = DawnPhase::IDLE;
    prepareOffFrame();
    setRuntimeIdle();
    Serial.println(F("EVENT DAWN_RECOVER_ABORT REASON=RTC_READ"));
    return;
  }

  const uint32_t nowSeconds = rtcSecondsSince2000(now);

  if (nowSeconds < dawnRuntime.startRtcSeconds) {
    dawnPhase = DawnPhase::IDLE;
    prepareOffFrame();
    setRuntimeIdle();
    Serial.println(F("EVENT DAWN_RECOVER_ABORT REASON=RTC_BEFORE_START"));
    return;
  }

  const uint32_t elapsedSeconds =
      nowSeconds - dawnRuntime.startRtcSeconds;

  if (elapsedSeconds >= dawnRuntime.durationSeconds) {
    dawnPhase = DawnPhase::HOLD;
    prepareDawnFrame(1000);
    dawnRuntime.phase = DawnPhase::HOLD;
    saveDawnRuntime();
    recoveredAtBoot = true;

    Serial.print(F("EVENT DAWN_RECOVER PHASE=HOLD SOURCE="));
    Serial.print(dawnSourceName(dawnRuntime.source));
    Serial.print(F(" ELAPSED_S="));
    Serial.println(elapsedSeconds);
    return;
  }

  dawnDurationMs = dawnRuntime.durationSeconds * 1000UL;
  const unsigned long elapsedMs = elapsedSeconds * 1000UL;
  dawnStartMs = millis() - elapsedMs;
  lastDawnFrameMs = 0;
  dawnPhase = DawnPhase::RUNNING;
  recoveredAtBoot = true;

  const uint16_t progress1000 =
      static_cast<uint16_t>(
          (static_cast<uint64_t>(elapsedSeconds) * 1000ULL) /
          dawnRuntime.durationSeconds
      );
  prepareDawnFrame(progress1000);

  Serial.print(F("EVENT DAWN_RECOVER PHASE=RUNNING SOURCE="));
  Serial.print(dawnSourceName(dawnRuntime.source));
  Serial.print(F(" ELAPSED_S="));
  Serial.print(elapsedSeconds);
  Serial.print(F(" REMAINING_S="));
  Serial.println(dawnRuntime.durationSeconds - elapsedSeconds);
}

void printStatus() {
  rtcPresent = probeRtc();

  if (rtcPresent) {
    bool lostPower = true;
    if (readLostPower(lostPower)) rtcLostPower = lostPower;
  }

  Serial.print(F("STATUS FW=DAWN REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" RTC_PRESENT="));
  Serial.print(rtcPresent ? F("YES") : F("NO"));
  Serial.print(F(" RTC_VALID="));
  Serial.print((rtcPresent && !rtcLostPower) ? F("YES") : F("NO"));

  Serial.print(F(" ALARM="));
  Serial.print(alarm.enabled ? F("ON") : F("OFF"));
  Serial.print(F(" ALARM_TIME="));
  printAlarmTime();
  Serial.print(F(" ALARM_SAVED="));
  Serial.print(alarm.storageValid ? F("YES") : F("NO"));

  Serial.print(F(" DAWN="));
  Serial.print(dawnPhaseName());
  Serial.print(F(" SOURCE="));
  Serial.print(dawnSourceName(dawnRuntime.source));
  Serial.print(F(" RUNTIME_SAVED="));
  Serial.print(dawnRuntime.storageValid ? F("YES") : F("NO"));
  Serial.print(F(" RECOVERED="));
  Serial.print(recoveredAtBoot ? F("YES") : F("NO"));
  Serial.print(F(" FADE_MIN="));
  Serial.print(dawnSettings.fadeMinutes);
  Serial.print(F(" MAX_BR="));
  Serial.print(dawnSettings.maxBrightness);
  Serial.print(F(" START_HUE="));
  Serial.print(dawnSettings.startHue);
  Serial.print(F(" END_HUE="));
  Serial.print(dawnSettings.endHue);
  Serial.print(F(" DAWN_SAVED="));
  Serial.print(dawnSettings.storageValid ? F("YES") : F("NO"));

  if (rtcPresent) {
    RtcTime now;
    if (readRtcTime(now)) {
      lastReadOk = true;
      Serial.print(F(" TIME="));
      printRtcTime(now);
    } else {
      lastReadOk = false;
      Serial.print(F(" TIME=ERR"));
    }
  }

  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

void printTimeLine() {
  RtcTime now;
  if (!readRtcTime(now)) {
    Serial.println(F("ERR RTC_READ"));
    return;
  }

  Serial.print(F("TIME "));
  printRtcTime(now);
  Serial.println();
}

void handleCommand(char* command) {
  if (strcmp(command, "PING") == 0) {
    Serial.println(F("PONG"));
    return;
  }

  if (strcmp(command, "STATUS") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "TIME") == 0) {
    printTimeLine();
    return;
  }

  if (strcmp(command, "ALARM ON") == 0) {
    alarm.enabled = true;
    saveAlarmSettings();
    Serial.println(F("OK ALARM=ON"));
    return;
  }

  if (strcmp(command, "ALARM OFF") == 0) {
    alarm.enabled = false;
    saveAlarmSettings();
    Serial.println(F("OK ALARM=OFF"));
    return;
  }

  if (strncmp(command, "ALARMSET ", 9) == 0) {
    uint8_t hour = 0;
    uint8_t minute = 0;

    if (!parseAlarmTime(command + 9, hour, minute)) {
      Serial.println(F("ERR BAD_ALARM_TIME"));
      return;
    }

    alarm.hour = hour;
    alarm.minute = minute;
    saveAlarmSettings();

    Serial.print(F("OK ALARM_TIME="));
    printAlarmTime();
    Serial.println();
    return;
  }

  if (strcmp(command, "ALARM CLEARLAST") == 0) {
    alarm.lastTriggerYear = 0;
    alarm.lastTriggerMonth = 0;
    alarm.lastTriggerDay = 0;
    saveAlarmSettings();
    Serial.println(F("OK LAST_TRIGGER=NONE"));
    return;
  }

  if (strncmp(command, "FADEMIN ", 8) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 8, value) || value < 1 || value > 120) {
      Serial.println(F("ERR BAD_FADE_MIN"));
      return;
    }

    dawnSettings.fadeMinutes = value;
    saveDawnSettings();
    Serial.print(F("OK FADE_MIN="));
    Serial.println(dawnSettings.fadeMinutes);
    return;
  }

  if (strncmp(command, "MAXBR ", 6) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 6, value) || value < 1) {
      Serial.println(F("ERR BAD_MAX_BR"));
      return;
    }

    dawnSettings.maxBrightness = value;
    saveDawnSettings();
    Serial.print(F("OK MAX_BR="));
    Serial.println(dawnSettings.maxBrightness);
    return;
  }

  if (strncmp(command, "STARTHUE ", 9) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 9, value)) {
      Serial.println(F("ERR BAD_START_HUE"));
      return;
    }

    dawnSettings.startHue = value;
    saveDawnSettings();
    Serial.print(F("OK START_HUE="));
    Serial.println(dawnSettings.startHue);
    return;
  }

  if (strncmp(command, "ENDHUE ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_END_HUE"));
      return;
    }

    dawnSettings.endHue = value;
    saveDawnSettings();
    Serial.print(F("OK END_HUE="));
    Serial.println(dawnSettings.endHue);
    return;
  }

  if (strncmp(command, "DAWN TEST ", 10) == 0) {
    uint8_t seconds = 0;
    if (!parseByte(command + 10, seconds) || seconds < 5 || seconds > 120) {
      Serial.println(F("ERR BAD_TEST_SECONDS"));
      return;
    }

    startDawn(
        static_cast<unsigned long>(seconds) * 1000UL,
        DawnSource::TEST
    );
    return;
  }

  if (strcmp(command, "DAWN START") == 0) {
    const unsigned long durationMs =
        static_cast<unsigned long>(dawnSettings.fadeMinutes) *
        60UL * 1000UL;

    startDawn(durationMs, DawnSource::MANUAL);
    return;
  }

  if (strcmp(command, "DAWN STOP") == 0) {
    stopDawn();
    return;
  }

  if (strncmp(command, "SET ", 4) == 0) {
    RtcTime t;
    if (!parseSetTime(command + 4, t)) {
      Serial.println(F("ERR BAD_TIME_FORMAT"));
      return;
    }

    if (!probeRtc()) {
      Serial.println(F("ERR RTC_NOT_FOUND"));
      return;
    }

    if (!setRtcTime(t)) {
      Serial.println(F("ERR RTC_WRITE"));
      return;
    }

    rtcLostPower = false;

    Serial.print(F("OK TIME="));
    printRtcTime(t);
    Serial.println();
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS STATUS TIME SET ALARMSET ALARM ON/OFF/CLEARLAST FADEMIN MAXBR STARTHUE ENDHUE DAWN TEST/START/STOP HELP"));
    Serial.println(F("FORMAT DAWN TEST <5..120 sec> | FADEMIN <1..120> | MAXBR <1..255> | HUE <0..255>"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    lastSerialRxMs = millis();

    if (c == '\r') continue;

    if (c == '\n') {
      if (rxLength > 0) {
        rxBuffer[rxLength] = '\0';
        handleCommand(rxBuffer);
        rxLength = 0;
      }
      continue;
    }

    if (rxLength < ArduConfig::RX_BUFFER_SIZE - 1) {
      rxBuffer[rxLength++] = c;
    } else {
      rxLength = 0;
      Serial.println(F("ERR LINE_TOO_LONG"));
    }
  }
}

void setup() {
  Serial.begin(ArduConfig::SERIAL_BAUD);

  pinMode(ArduPins::RING_B, OUTPUT);
  digitalWrite(ArduPins::RING_B, LOW);

  FastLED.addLeds<WS2812B, ArduPins::RING_A, GRB>(
      leds,
      ArduConfig::LED_COUNT
  );
  FastLED.setMaxPowerInVoltsAndMilliamps(
      5,
      ArduConfig::MAX_MILLIAMPS
  );
  prepareOffFrame();
  FastLED.show();

  Wire.begin();
  Wire.setClock(100000UL);
  delay(50);

  loadAlarmSettings();
  loadDawnSettings();
  loadDawnRuntime();

  rtcPresent = probeRtc();
  if (rtcPresent) {
    bool lostPower = true;
    if (readLostPower(lostPower)) rtcLostPower = lostPower;
  }

  Serial.println(F("ARDU NANO DAWN R2 READY"));
  Serial.println(F("DAWN R2: RTC-BASED RESET RECOVERY ENABLED"));
  recoverDawnAfterReset();
  showFrameIfSafe();
  printStatus();
}

void loop() {
  pollSerial();
  updateAlarmTrigger();
  updateDawn();
}
