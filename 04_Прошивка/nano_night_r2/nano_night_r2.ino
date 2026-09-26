/*
  ARDU NIGHT R2 — persistent night-light + DS3231 schedule.

  Behavior:
  - constant HSV night-light on one 43-LED WS2812B ring;
  - persistent manual ON/OFF + HUE + SAT + BRIGHTNESS;
  - optional daily RTC schedule with ON/OFF times;
  - overnight windows are supported, e.g. 22:00 -> 07:00;
  - schedule is OFF by default;
  - explicit manual ON/OFF disables schedule and returns to manual control;
  - schedule transitions are derived from RTC and do not write EEPROM.

  Hardware:
  - WS2812B ring A -> D6
  - D7 held LOW until second-ring testing
  - DS3231 SDA -> A4, SCL -> A5
  - proven FastLED/UART coexistence fix carried forward
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 2;

constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr unsigned long RTC_POLL_MS = 250UL;
constexpr size_t RX_BUFFER_SIZE = 80;

constexpr uint8_t DS3231_ADDRESS = 0x68;

constexpr bool DEFAULT_MANUAL_ENABLED = false;
constexpr uint8_t DEFAULT_HUE = 24;
constexpr uint8_t DEFAULT_SATURATION = 180;
constexpr uint8_t DEFAULT_BRIGHTNESS = 18;

constexpr bool DEFAULT_SCHEDULE_ENABLED = false;
constexpr uint8_t DEFAULT_SCHEDULE_ON_HOUR = 22;
constexpr uint8_t DEFAULT_SCHEDULE_ON_MINUTE = 0;
constexpr uint8_t DEFAULT_SCHEDULE_OFF_HOUR = 7;
constexpr uint8_t DEFAULT_SCHEDULE_OFF_MINUTE = 0;

// NIGHT R2 replaces the unverified R1 block at the same isolated base.
constexpr uint16_t EEPROM_MAGIC = 0x4E32;  // "N2"
constexpr uint8_t EEPROM_VERSION = 2;
constexpr int EEPROM_BASE = 64;
}

struct RtcTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

struct NightSettings {
  bool manualEnabled = ArduConfig::DEFAULT_MANUAL_ENABLED;
  uint8_t hue = ArduConfig::DEFAULT_HUE;
  uint8_t saturation = ArduConfig::DEFAULT_SATURATION;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;

  bool scheduleEnabled = ArduConfig::DEFAULT_SCHEDULE_ENABLED;
  uint8_t scheduleOnHour = ArduConfig::DEFAULT_SCHEDULE_ON_HOUR;
  uint8_t scheduleOnMinute = ArduConfig::DEFAULT_SCHEDULE_ON_MINUTE;
  uint8_t scheduleOffHour = ArduConfig::DEFAULT_SCHEDULE_OFF_HOUR;
  uint8_t scheduleOffMinute = ArduConfig::DEFAULT_SCHEDULE_OFF_MINUTE;

  bool storageValid = false;
};

NightSettings night;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

unsigned long lastSerialRxMs = 0;
unsigned long lastRtcPollMs = 0;
bool frameDirty = true;

bool rtcPresent = false;
bool rtcLostPower = true;
bool rtcReadOk = false;
bool effectivePower = false;
bool scheduleWindowActive = false;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
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

bool readLostPower(bool& lostPower) {
  uint8_t status = 0;

  if (!readRegisters(0x0F, &status, 1)) {
    return false;
  }

  lostPower = (status & 0x80) != 0;
  return true;
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

  return
      t.month >= 1 && t.month <= 12 &&
      t.day >= 1 && t.day <= 31 &&
      t.hour <= 23 &&
      t.minute <= 59 &&
      t.second <= 59;
}

bool rtcValidNow(RtcTime* out = nullptr) {
  rtcPresent = probeRtc();

  if (!rtcPresent) {
    rtcReadOk = false;
    rtcLostPower = true;
    return false;
  }

  bool lostPower = true;

  if (!readLostPower(lostPower)) {
    rtcReadOk = false;
    rtcLostPower = true;
    return false;
  }

  rtcLostPower = lostPower;

  RtcTime now;
  if (!readRtcTime(now)) {
    rtcReadOk = false;
    return false;
  }

  rtcReadOk = true;

  if (out != nullptr) {
    *out = now;
  }

  return !rtcLostPower;
}

uint8_t computeChecksum(const uint8_t* bytes, uint8_t length) {
  uint8_t checksum = 0x5A;

  for (uint8_t i = 0; i < length; ++i) {
    checksum = static_cast<uint8_t>((checksum << 1) | (checksum >> 7));
    checksum ^= bytes[i];
  }

  return checksum;
}

void saveNightSettings() {
  uint8_t bytes[12];

  bytes[0] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::EEPROM_VERSION;
  bytes[3] = night.manualEnabled ? 1 : 0;
  bytes[4] = night.hue;
  bytes[5] = night.saturation;
  bytes[6] = night.brightness;
  bytes[7] = night.scheduleEnabled ? 1 : 0;
  bytes[8] = night.scheduleOnHour;
  bytes[9] = night.scheduleOnMinute;
  bytes[10] = night.scheduleOffHour;
  bytes[11] = night.scheduleOffMinute;

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    EEPROM.update(ArduConfig::EEPROM_BASE + i, bytes[i]);
  }

  EEPROM.update(
      ArduConfig::EEPROM_BASE + 12,
      computeChecksum(bytes, sizeof(bytes))
  );

  night.storageValid = true;
}

void loadNightSettings() {
  uint8_t bytes[12];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = EEPROM.read(ArduConfig::EEPROM_BASE + i);
  }

  const uint8_t storedChecksum =
      EEPROM.read(ArduConfig::EEPROM_BASE + 12);

  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const bool valid =
      magic == ArduConfig::EEPROM_MAGIC &&
      bytes[2] == ArduConfig::EEPROM_VERSION &&
      bytes[3] <= 1 &&
      bytes[7] <= 1 &&
      bytes[8] <= 23 &&
      bytes[9] <= 59 &&
      bytes[10] <= 23 &&
      bytes[11] <= 59 &&
      !(bytes[8] == bytes[10] && bytes[9] == bytes[11]) &&
      storedChecksum == computeChecksum(bytes, sizeof(bytes));

  if (!valid) {
    night = NightSettings();
    night.storageValid = false;
    return;
  }

  night.manualEnabled = bytes[3] != 0;
  night.hue = bytes[4];
  night.saturation = bytes[5];
  night.brightness = bytes[6];

  night.scheduleEnabled = bytes[7] != 0;
  night.scheduleOnHour = bytes[8];
  night.scheduleOnMinute = bytes[9];
  night.scheduleOffHour = bytes[10];
  night.scheduleOffMinute = bytes[11];

  night.storageValid = true;
}

bool parseByte(const char* text, uint8_t& value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);

  if (*end != '\0' || parsed < 0 || parsed > 255) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
  return true;
}

bool parseClock(const char* text, uint8_t& hour, uint8_t& minute) {
  if (text == nullptr || strlen(text) != 5 || text[2] != ':') {
    return false;
  }

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

uint16_t minuteOfDay(uint8_t hour, uint8_t minute) {
  return static_cast<uint16_t>(hour) * 60U + minute;
}

bool isInsideScheduleWindow(const RtcTime& now) {
  const uint16_t current =
      minuteOfDay(now.hour, now.minute);
  const uint16_t start =
      minuteOfDay(night.scheduleOnHour, night.scheduleOnMinute);
  const uint16_t stop =
      minuteOfDay(night.scheduleOffHour, night.scheduleOffMinute);

  if (start < stop) {
    return current >= start && current < stop;
  }

  // Overnight window, e.g. 22:00 -> 07:00.
  return current >= start || current < stop;
}

void prepareNightFrame() {
  if (effectivePower) {
    fill_solid(
        leds,
        ArduConfig::LED_COUNT,
        CHSV(night.hue, night.saturation, 255)
    );
    FastLED.setBrightness(night.brightness);
  } else {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
    FastLED.setBrightness(0);
  }

  frameDirty = true;
}

void showFrameIfSafe() {
  if (!frameDirty || serialRxGuardActive()) {
    return;
  }

  FastLED.show();
  frameDirty = false;
}

void setEffectivePower(bool on, bool announce) {
  if (effectivePower == on) {
    return;
  }

  effectivePower = on;
  prepareNightFrame();

  if (announce) {
    Serial.print(F("EVENT NIGHT_SCHEDULE POWER="));
    Serial.println(effectivePower ? F("ON") : F("OFF"));
  }
}

bool reconcileSchedule(bool announce) {
  if (!night.scheduleEnabled) {
    scheduleWindowActive = false;
    setEffectivePower(night.manualEnabled, false);
    return true;
  }

  RtcTime now;
  if (!rtcValidNow(&now)) {
    // Fail safe: an enabled schedule cannot make timing decisions without
    // trustworthy RTC. Keep the light OFF until RTC becomes valid again.
    scheduleWindowActive = false;
    setEffectivePower(false, false);
    return false;
  }

  scheduleWindowActive = isInsideScheduleWindow(now);
  setEffectivePower(scheduleWindowActive, announce);
  return true;
}

void updateSchedule() {
  const unsigned long nowMs = millis();

  if (nowMs - lastRtcPollMs < ArduConfig::RTC_POLL_MS) {
    return;
  }

  lastRtcPollMs = nowMs;

  if (night.scheduleEnabled) {
    (void)reconcileSchedule(true);
  }
}

void print2(uint8_t value) {
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void printClock(uint8_t hour, uint8_t minute) {
  print2(hour);
  Serial.print(':');
  print2(minute);
}

void printRtcTime(const RtcTime& t) {
  Serial.print(t.year);
  Serial.print('-');
  print2(t.month);
  Serial.print('-');
  print2(t.day);
  Serial.print(' ');
  printClock(t.hour, t.minute);
  Serial.print(':');
  print2(t.second);
}

void printStatus() {
  RtcTime now;
  const bool rtcValid = rtcValidNow(&now);

  Serial.print(F("STATUS FW=NIGHT REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=NIGHT POWER="));
  Serial.print(effectivePower ? F("ON") : F("OFF"));
  Serial.print(F(" CONTROL="));
  Serial.print(night.scheduleEnabled ? F("SCHEDULE") : F("MANUAL"));
  Serial.print(F(" MANUAL_POWER="));
  Serial.print(night.manualEnabled ? F("ON") : F("OFF"));

  Serial.print(F(" HUE="));
  Serial.print(night.hue);
  Serial.print(F(" SAT="));
  Serial.print(night.saturation);
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(night.brightness);

  Serial.print(F(" SCHEDULE="));
  Serial.print(night.scheduleEnabled ? F("ON") : F("OFF"));
  Serial.print(F(" SCHED_ON="));
  printClock(night.scheduleOnHour, night.scheduleOnMinute);
  Serial.print(F(" SCHED_OFF="));
  printClock(night.scheduleOffHour, night.scheduleOffMinute);
  Serial.print(F(" WINDOW="));
  Serial.print(scheduleWindowActive ? F("ACTIVE") : F("INACTIVE"));

  Serial.print(F(" RTC_PRESENT="));
  Serial.print(rtcPresent ? F("YES") : F("NO"));
  Serial.print(F(" RTC_VALID="));
  Serial.print(rtcValid ? F("YES") : F("NO"));

  if (rtcReadOk) {
    Serial.print(F(" TIME="));
    printRtcTime(now);
  }

  Serial.print(F(" SAVED="));
  Serial.print(night.storageValid ? F("YES") : F("NO"));
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

void printTimeLine() {
  RtcTime now;

  if (!rtcValidNow(&now)) {
    Serial.println(F("ERR RTC_INVALID"));
    return;
  }

  Serial.print(F("TIME "));
  printRtcTime(now);
  Serial.println();
}

void saveVisualAndRefresh() {
  saveNightSettings();

  if (!night.scheduleEnabled) {
    effectivePower = night.manualEnabled;
  }

  prepareNightFrame();
}

void disableScheduleForManualControl() {
  night.scheduleEnabled = false;
  scheduleWindowActive = false;
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

  if (strcmp(command, "ON") == 0) {
    disableScheduleForManualControl();
    night.manualEnabled = true;
    effectivePower = true;
    saveNightSettings();
    prepareNightFrame();
    Serial.println(F("OK POWER=ON CONTROL=MANUAL"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    disableScheduleForManualControl();
    night.manualEnabled = false;
    effectivePower = false;
    saveNightSettings();
    prepareNightFrame();
    Serial.println(F("OK POWER=OFF CONTROL=MANUAL"));
    return;
  }

  if (strncmp(command, "HUE ", 4) == 0) {
    uint8_t value = 0;

    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_HUE"));
      return;
    }

    night.hue = value;
    saveVisualAndRefresh();

    Serial.print(F("OK HUE="));
    Serial.println(night.hue);
    return;
  }

  if (strncmp(command, "SAT ", 4) == 0) {
    uint8_t value = 0;

    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_SAT"));
      return;
    }

    night.saturation = value;
    saveVisualAndRefresh();

    Serial.print(F("OK SAT="));
    Serial.println(night.saturation);
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    uint8_t value = 0;

    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    night.brightness = value;
    saveVisualAndRefresh();

    Serial.print(F("OK BRIGHTNESS="));
    Serial.println(night.brightness);
    return;
  }

  if (strncmp(command, "SCHEDULESET ", 12) == 0) {
    char* value = command + 12;
    char* separator = strchr(value, ' ');

    if (separator == nullptr) {
      Serial.println(F("ERR BAD_SCHEDULE_FORMAT"));
      return;
    }

    *separator = '\0';
    const char* onText = value;
    const char* offText = separator + 1;

    uint8_t onHour = 0;
    uint8_t onMinute = 0;
    uint8_t offHour = 0;
    uint8_t offMinute = 0;

    if (!parseClock(onText, onHour, onMinute) ||
        !parseClock(offText, offHour, offMinute)) {
      Serial.println(F("ERR BAD_SCHEDULE_FORMAT"));
      return;
    }

    if (onHour == offHour && onMinute == offMinute) {
      Serial.println(F("ERR SAME_SCHEDULE_TIME"));
      return;
    }

    night.scheduleOnHour = onHour;
    night.scheduleOnMinute = onMinute;
    night.scheduleOffHour = offHour;
    night.scheduleOffMinute = offMinute;
    saveNightSettings();

    if (night.scheduleEnabled) {
      (void)reconcileSchedule(false);
    }

    Serial.print(F("OK SCHEDULE "));
    printClock(night.scheduleOnHour, night.scheduleOnMinute);
    Serial.print(' ');
    printClock(night.scheduleOffHour, night.scheduleOffMinute);
    Serial.println();
    return;
  }

  if (strcmp(command, "SCHEDULE ON") == 0) {
    RtcTime now;

    if (!rtcValidNow(&now)) {
      Serial.println(F("ERR RTC_INVALID"));
      return;
    }

    night.scheduleEnabled = true;
    saveNightSettings();
    (void)reconcileSchedule(false);

    Serial.print(F("OK SCHEDULE=ON WINDOW="));
    Serial.println(scheduleWindowActive ? F("ACTIVE") : F("INACTIVE"));
    return;
  }

  if (strcmp(command, "SCHEDULE OFF") == 0) {
    night.scheduleEnabled = false;
    scheduleWindowActive = false;
    saveNightSettings();
    setEffectivePower(night.manualEnabled, false);

    Serial.print(F("OK SCHEDULE=OFF POWER="));
    Serial.println(effectivePower ? F("ON") : F("OFF"));
    return;
  }

  if (strcmp(command, "DEFAULTS") == 0) {
    night = NightSettings();
    saveNightSettings();
    scheduleWindowActive = false;
    effectivePower = night.manualEnabled;
    prepareNightFrame();

    Serial.println(F("OK DEFAULTS"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS TIME ON OFF HUE SAT BRIGHT SCHEDULESET SCHEDULE ON SCHEDULE OFF DEFAULTS HELP"));
    Serial.println(F("FORMAT SCHEDULESET HH:MM HH:MM | HUE <0..255> | SAT <0..255> | BRIGHT <0..255>"));
    Serial.println(F("NOTE manual ON/OFF disables schedule"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    lastSerialRxMs = millis();

    if (c == '\r') {
      continue;
    }

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

  Wire.begin();
  Wire.setClock(100000UL);
  delay(50);

  loadNightSettings();

  effectivePower = night.manualEnabled;

  if (night.scheduleEnabled) {
    (void)reconcileSchedule(false);
  } else {
    scheduleWindowActive = false;
    effectivePower = night.manualEnabled;
  }

  prepareNightFrame();

  Serial.println(F("ARDU NANO NIGHT R2 READY"));
  Serial.println(F("NIGHT R2: DS3231 SCHEDULE ENABLED; MANUAL ON/OFF DISABLES SCHEDULE"));
  printStatus();
}

void loop() {
  pollSerial();
  updateSchedule();
  showFrameIfSafe();
}
