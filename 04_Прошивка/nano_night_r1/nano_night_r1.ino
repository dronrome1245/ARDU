/*
  ARDU NIGHT R1 — persistent low-brightness night-light.

  Purpose:
  - constant HSV color on one 43-LED WS2812B ring;
  - low-brightness defaults from the current API draft;
  - persist ON/OFF + HUE + SAT + BRIGHTNESS in Nano EEPROM;
  - restore the same state after Nano reset/power cycle;
  - no schedule in R1.

  Hardware:
  - WS2812B ring A -> D6
  - D7 held LOW until second-ring testing
  - proven FastLED/UART coexistence fix carried forward
*/

#include <Arduino.h>
#include <EEPROM.h>

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;

constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;

// Matches the current API v1 night example.
constexpr bool DEFAULT_ENABLED = false;
constexpr uint8_t DEFAULT_HUE = 24;
constexpr uint8_t DEFAULT_SATURATION = 180;
constexpr uint8_t DEFAULT_BRIGHTNESS = 18;

// Separate from ALARM/DAWN blocks used by the previous standalone sketches.
constexpr uint16_t EEPROM_MAGIC = 0x4E31;  // "N1"
constexpr uint8_t EEPROM_VERSION = 1;
constexpr int EEPROM_BASE = 64;
}

struct NightSettings {
  bool enabled = ArduConfig::DEFAULT_ENABLED;
  uint8_t hue = ArduConfig::DEFAULT_HUE;
  uint8_t saturation = ArduConfig::DEFAULT_SATURATION;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  bool storageValid = false;
};

NightSettings night;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

unsigned long lastSerialRxMs = 0;
bool frameDirty = true;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
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
  uint8_t bytes[7];
  bytes[0] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC & 0xFF);
  bytes[1] = static_cast<uint8_t>(ArduConfig::EEPROM_MAGIC >> 8);
  bytes[2] = ArduConfig::EEPROM_VERSION;
  bytes[3] = night.enabled ? 1 : 0;
  bytes[4] = night.hue;
  bytes[5] = night.saturation;
  bytes[6] = night.brightness;

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    EEPROM.update(ArduConfig::EEPROM_BASE + i, bytes[i]);
  }

  EEPROM.update(
      ArduConfig::EEPROM_BASE + 7,
      computeChecksum(bytes, sizeof(bytes))
  );

  night.storageValid = true;
}

void loadNightSettings() {
  uint8_t bytes[7];

  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = EEPROM.read(ArduConfig::EEPROM_BASE + i);
  }

  const uint8_t storedChecksum =
      EEPROM.read(ArduConfig::EEPROM_BASE + 7);

  const uint16_t magic =
      static_cast<uint16_t>(bytes[0]) |
      (static_cast<uint16_t>(bytes[1]) << 8);

  const bool valid =
      magic == ArduConfig::EEPROM_MAGIC &&
      bytes[2] == ArduConfig::EEPROM_VERSION &&
      bytes[3] <= 1 &&
      storedChecksum == computeChecksum(bytes, sizeof(bytes));

  if (!valid) {
    night = NightSettings();
    night.storageValid = false;
    return;
  }

  night.enabled = bytes[3] != 0;
  night.hue = bytes[4];
  night.saturation = bytes[5];
  night.brightness = bytes[6];
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

void prepareNightFrame() {
  if (night.enabled) {
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

void printStatus() {
  Serial.print(F("STATUS FW=NIGHT REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=NIGHT POWER="));
  Serial.print(night.enabled ? F("ON") : F("OFF"));
  Serial.print(F(" HUE="));
  Serial.print(night.hue);
  Serial.print(F(" SAT="));
  Serial.print(night.saturation);
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(night.brightness);
  Serial.print(F(" SAVED="));
  Serial.print(night.storageValid ? F("YES") : F("NO"));
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

void applyAndSave() {
  saveNightSettings();
  prepareNightFrame();
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

  if (strcmp(command, "ON") == 0) {
    night.enabled = true;
    applyAndSave();
    Serial.println(F("OK POWER=ON"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    night.enabled = false;
    applyAndSave();
    Serial.println(F("OK POWER=OFF"));
    return;
  }

  if (strncmp(command, "HUE ", 4) == 0) {
    uint8_t value = 0;

    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_HUE"));
      return;
    }

    night.hue = value;
    applyAndSave();

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
    applyAndSave();

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
    applyAndSave();

    Serial.print(F("OK BRIGHTNESS="));
    Serial.println(night.brightness);
    return;
  }

  if (strcmp(command, "DEFAULTS") == 0) {
    night = NightSettings();
    saveNightSettings();
    prepareNightFrame();

    Serial.println(F("OK DEFAULTS"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF HUE SAT BRIGHT DEFAULTS HELP"));
    Serial.println(F("FORMAT HUE <0..255> | SAT <0..255> | BRIGHT <0..255>"));
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

  loadNightSettings();
  prepareNightFrame();

  Serial.println(F("ARDU NANO NIGHT R1 READY"));
  printStatus();
}

void loop() {
  pollSerial();
  showFrameIfSafe();
}
