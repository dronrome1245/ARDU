/*
  ARDU RTC R1 — DS3231 hardware sanity test.

  No external RTC library is required: the sketch talks to DS3231 directly
  over Arduino Wire/I2C.

  ARDU wiring:
  - DS3231 SDA -> Nano A4
  - DS3231 SCL -> Nano A5
  - DS3231 VCC -> Nano 5V
  - DS3231 GND -> Nano GND

  SAFETY:
  The project DS3231 module has a CR2032 and a charging circuit.
  Per project decision D-017, remove resistor "201" from the charge path
  before powering the module with CR2032 installed.
*/

#include <Arduino.h>
#include <Wire.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t RTC_SDA = A4;
constexpr uint8_t RTC_SCL = A5;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;
constexpr uint8_t DS3231_ADDRESS = 0x68;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct RtcTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

bool rtcPresent = false;
bool lastReadOk = false;

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
  if (t.hour > 23) return false;
  if (t.minute > 59) return false;
  if (t.second > 59) return false;
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

bool readStatusRegister(uint8_t& status) {
  return readRegisters(0x0F, &status, 1);
}

bool readLostPower(bool& lostPower) {
  uint8_t status = 0;
  if (!readStatusRegister(status)) return false;
  lostPower = (status & 0x80) != 0;  // OSF bit.
  return true;
}

bool clearLostPowerFlag() {
  uint8_t status = 0;
  if (!readStatusRegister(status)) return false;
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
    // 12-hour mode.
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

  const bool century = (data[5] & 0x80) != 0;
  t.month = bcdToDec(data[5] & 0x1F);
  const uint8_t year2 = bcdToDec(data[6]);

  // ARDU v1 only accepts 2000..2099 on SET. Still decode century bit
  // explicitly so diagnostics are not silently wrong.
  t.year = static_cast<uint16_t>((century ? 2100 : 2000) + year2);

  return true;
}

bool setRtcTime(const RtcTime& t) {
  if (!validTime(t)) return false;

  // Day-of-week is not used by ARDU yet. Write 1 as a valid placeholder.
  Wire.beginTransmission(ArduConfig::DS3231_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(t.second));
  Wire.write(decToBcd(t.minute));
  Wire.write(decToBcd(t.hour));   // Force 24-hour mode.
  Wire.write(decToBcd(1));        // Day of week.
  Wire.write(decToBcd(t.day));
  Wire.write(decToBcd(t.month));  // Century=0 for 2000..2099.
  Wire.write(decToBcd(static_cast<uint8_t>(t.year - 2000)));

  if (Wire.endTransmission() != 0) {
    return false;
  }

  return clearLostPowerFlag();
}

bool readTemperatureCentiC(int16_t& centiC) {
  uint8_t data[2] = {0};

  if (!readRegisters(0x11, data, sizeof(data))) {
    return false;
  }

  const int8_t integerPart = static_cast<int8_t>(data[0]);
  const uint8_t fractionQuarter = static_cast<uint8_t>(data[1] >> 6);

  centiC =
      static_cast<int16_t>(integerPart) * 100 +
      static_cast<int16_t>(fractionQuarter) * 25;

  return true;
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

void printTimeValue(const RtcTime& t) {
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

void printTimeLine() {
  RtcTime t;
  if (!readRtcTime(t)) {
    lastReadOk = false;
    rtcPresent = probeRtc();
    Serial.println(F("ERR RTC_READ"));
    return;
  }

  lastReadOk = true;
  rtcPresent = true;

  Serial.print(F("TIME "));
  printTimeValue(t);
  Serial.println();
}

void printTemperature() {
  int16_t centiC = 0;

  if (!readTemperatureCentiC(centiC)) {
    Serial.println(F("ERR RTC_TEMP"));
    return;
  }

  Serial.print(F("TEMP_C="));

  if (centiC < 0) {
    Serial.print('-');
    centiC = -centiC;
  }

  Serial.print(centiC / 100);
  Serial.print('.');
  print2(static_cast<uint8_t>(centiC % 100));
  Serial.println();
}

void printStatus() {
  rtcPresent = probeRtc();

  Serial.print(F("STATUS FW=RTC REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" RTC=DS3231 I2C_ADDR=0x68 SDA=A4 SCL=A5 PRESENT="));
  Serial.print(rtcPresent ? F("YES") : F("NO"));

  if (rtcPresent) {
    bool lostPower = false;
    if (readLostPower(lostPower)) {
      Serial.print(F(" OSF="));
      Serial.print(lostPower ? 1 : 0);
      Serial.print(F(" LOST_POWER="));
      Serial.print(lostPower ? F("YES") : F("NO"));
    } else {
      Serial.print(F(" OSF=? LOST_POWER=?"));
    }

    RtcTime t;
    if (readRtcTime(t)) {
      lastReadOk = true;
      Serial.print(F(" TIME="));
      printTimeValue(t);
    } else {
      lastReadOk = false;
      Serial.print(F(" TIME=ERR"));
    }
  } else {
    lastReadOk = false;
  }

  Serial.print(F(" LAST_READ="));
  Serial.print(lastReadOk ? F("OK") : F("ERR"));
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
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
  // Exact format: YYYY-MM-DD HH:MM:SS
  if (strlen(text) != 19) return false;

  if (text[4] != '-' ||
      text[7] != '-' ||
      text[10] != ' ' ||
      text[13] != ':' ||
      text[16] != ':') {
    return false;
  }

  uint16_t year = 0;
  uint16_t month = 0;
  uint16_t day = 0;
  uint16_t hour = 0;
  uint16_t minute = 0;
  uint16_t second = 0;

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

  if (strcmp(command, "TEMP") == 0) {
    printTemperature();
    return;
  }

  if (strcmp(command, "CLEAROSF") == 0) {
    if (!probeRtc()) {
      Serial.println(F("ERR RTC_NOT_FOUND"));
      return;
    }

    if (!clearLostPowerFlag()) {
      Serial.println(F("ERR RTC_WRITE"));
      return;
    }

    Serial.println(F("OK OSF=0"));
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

    Serial.print(F("OK TIME="));
    printTimeValue(t);
    Serial.println();
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS TIME TEMP SET CLEAROSF HELP"));
    Serial.println(F("FORMAT SET YYYY-MM-DD HH:MM:SS"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

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

  // Keep both LED data outputs electrically quiet during the isolated RTC test.
  pinMode(ArduPins::RING_A, OUTPUT);
  digitalWrite(ArduPins::RING_A, LOW);
  pinMode(ArduPins::RING_B, OUTPUT);
  digitalWrite(ArduPins::RING_B, LOW);

  Wire.begin();
  Wire.setClock(100000UL);
  delay(50);

  rtcPresent = probeRtc();

  Serial.println(F("ARDU NANO RTC R1 READY"));
  Serial.println(F("SAFETY: CR2032 REQUIRES CHARGE RESISTOR 201 REMOVED"));
  printStatus();
}

void loop() {
  pollSerial();
}
