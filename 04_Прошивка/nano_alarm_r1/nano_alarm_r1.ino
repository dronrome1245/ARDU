/*
  ARDU ALARM R1 — DS3231 daily alarm trigger + persisted settings.

  Purpose of this layer:
  - keep RTC handling isolated and verified;
  - persist alarm enabled/hour/minute in Arduino Nano internal EEPROM;
  - generate one textual ALARM_TRIGGER event per calendar day;
  - do NOT drive the LED dawn yet.

  DS3231 wiring:
  - SDA -> A4
  - SCL -> A5

  Safety:
  Project DS3231 CR2032 charging resistor "201" must already be removed
  per D-017 before normal powered use with CR2032 installed.
*/

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;
constexpr uint8_t DS3231_ADDRESS = 0x68;
constexpr size_t RX_BUFFER_SIZE = 64;

constexpr uint16_t EEPROM_MAGIC = 0xA8D1;
constexpr uint8_t EEPROM_VERSION = 1;
constexpr int EEPROM_BASE = 0;

constexpr uint8_t DEFAULT_ALARM_HOUR = 7;
constexpr uint8_t DEFAULT_ALARM_MINUTE = 0;
constexpr unsigned long RTC_POLL_MS = 250UL;
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

AlarmSettings alarm;

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

bool rtcPresent = false;
bool rtcLostPower = true;
bool lastReadOk = false;
unsigned long lastRtcPollMs = 0;

uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

uint8_t decToBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
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
  Wire.write(decToBcd(1));  // day-of-week unused by ARDU v1
  Wire.write(decToBcd(t.day));
  Wire.write(decToBcd(t.month));
  Wire.write(decToBcd(static_cast<uint8_t>(t.year - 2000)));

  if (Wire.endTransmission() != 0) {
    return false;
  }

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

uint8_t storageByte(uint8_t offset) {
  return EEPROM.read(ArduConfig::EEPROM_BASE + offset);
}

void updateStorageByte(uint8_t offset, uint8_t value) {
  EEPROM.update(ArduConfig::EEPROM_BASE + offset, value);
}

uint8_t computeStorageChecksum(const uint8_t* bytes, uint8_t length) {
  uint8_t checksum = 0x5A;
  for (uint8_t i = 0; i < length; ++i) {
    checksum = static_cast<uint8_t>((checksum << 1) | (checksum >> 7));
    checksum ^= bytes[i];
  }
  return checksum;
}

void saveAlarmSettings() {
  uint8_t bytes[9];
  bytes[0] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::EEPROM_VERSION;
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
    updateStorageByte(i, bytes[i]);
  }

  updateStorageByte(
      9,
      computeStorageChecksum(bytes, sizeof(bytes))
  );

  alarm.storageValid = true;
}

void loadAlarmSettings() {
  uint8_t bytes[9];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = storageByte(i);
  }

  const uint8_t storedChecksum = storageByte(9);
  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const bool valid =
      magic == ArduConfig::EEPROM_MAGIC &&
      bytes[2] == ArduConfig::EEPROM_VERSION &&
      storedChecksum == computeStorageChecksum(bytes, sizeof(bytes)) &&
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

  if (!alarm.enabled || rtcLostPower) {
    return;
  }

  if (now.hour == alarm.hour &&
      now.minute == alarm.minute &&
      !alreadyTriggeredToday(now)) {
    markTriggeredToday(now);
    emitAlarmTrigger(now);
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

void printStatus() {
  rtcPresent = probeRtc();

  if (rtcPresent) {
    bool lostPower = true;
    if (readLostPower(lostPower)) {
      rtcLostPower = lostPower;
    }
  }

  Serial.print(F("STATUS FW=ALARM REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" RTC_PRESENT="));
  Serial.print(rtcPresent ? F("YES") : F("NO"));
  Serial.print(F(" RTC_VALID="));
  Serial.print((rtcPresent && !rtcLostPower) ? F("YES") : F("NO"));
  Serial.print(F(" ALARM="));
  Serial.print(alarm.enabled ? F("ON") : F("OFF"));
  Serial.print(F(" ALARM_TIME="));
  printAlarmTime();
  Serial.print(F(" SAVED="));
  Serial.print(alarm.storageValid ? F("YES") : F("NO"));

  Serial.print(F(" LAST_TRIGGER="));
  if (alarm.lastTriggerYear == 0) {
    Serial.print(F("NONE"));
  } else {
    print4(alarm.lastTriggerYear);
    Serial.print('-');
    print2(alarm.lastTriggerMonth);
    Serial.print('-');
    print2(alarm.lastTriggerDay);
  }

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
    Serial.println(F("CMDS PING STATUS TIME SET ALARMSET ALARM ON ALARM OFF ALARM CLEARLAST HELP"));
    Serial.println(F("FORMAT ALARMSET HH:MM | SET YYYY-MM-DD HH:MM:SS"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

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

  // LEDs intentionally inactive in ALARM R1; dawn is a later layer.
  pinMode(ArduPins::RING_A, OUTPUT);
  digitalWrite(ArduPins::RING_A, LOW);
  pinMode(ArduPins::RING_B, OUTPUT);
  digitalWrite(ArduPins::RING_B, LOW);

  Wire.begin();
  Wire.setClock(100000UL);
  delay(50);

  loadAlarmSettings();

  rtcPresent = probeRtc();
  if (rtcPresent) {
    bool lostPower = true;
    if (readLostPower(lostPower)) {
      rtcLostPower = lostPower;
    }
  }

  Serial.println(F("ARDU NANO ALARM R1 READY"));
  Serial.println(F("ALARM R1: TEXT TRIGGER ONLY, DAWN LED OUTPUT NOT YET ENABLED"));
  printStatus();
}

void loop() {
  pollSerial();
  updateAlarmTrigger();
}
