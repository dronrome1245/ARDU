/*
  ARDU — isolated WS2812B strip test.

  Purpose:
  - test one 43-LED WS2812B ring/strip on D6;
  - do not use MAX9814, A0, FHT, RTC or ESP8266;
  - keep D7 LOW until the second ring is intentionally tested.

  Hardware baseline already verified in ARDU:
  D6 -> 220 ohm -> DIN WS2812B
  common GND
  strip powered from 5 V supply
  1000 uF 6.3 V across strip +5V/GND
*/

#include <Arduino.h>
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr uint8_t DEFAULT_BRIGHTNESS = 32;
constexpr unsigned long CHASE_INTERVAL_MS = 80UL;
constexpr size_t RX_BUFFER_SIZE = 48;
}

enum class TestMode : uint8_t {
  OFF,
  RED,
  GREEN,
  BLUE,
  WHITE,
  CHASE
};

CRGB leds[ArduConfig::LED_COUNT];

TestMode mode = TestMode::OFF;
uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t chaseIndex = 0;
unsigned long lastChaseMs = 0;

const __FlashStringHelper* modeName() {
  switch (mode) {
    case TestMode::OFF:   return F("OFF");
    case TestMode::RED:   return F("RED");
    case TestMode::GREEN: return F("GREEN");
    case TestMode::BLUE:  return F("BLUE");
    case TestMode::WHITE: return F("WHITE");
    case TestMode::CHASE: return F("CHASE");
  }
  return F("UNKNOWN");
}

void showSolid(const CRGB& color) {
  FastLED.setBrightness(brightness);
  fill_solid(leds, ArduConfig::LED_COUNT, color);
  FastLED.show();
}

void applyMode() {
  switch (mode) {
    case TestMode::OFF:
      showSolid(CRGB::Black);
      break;

    case TestMode::RED:
      showSolid(CRGB::Red);
      break;

    case TestMode::GREEN:
      showSolid(CRGB::Green);
      break;

    case TestMode::BLUE:
      showSolid(CRGB::Blue);
      break;

    case TestMode::WHITE:
      showSolid(CRGB::White);
      break;

    case TestMode::CHASE:
      chaseIndex = 0;
      lastChaseMs = 0;
      showSolid(CRGB::Black);
      break;
  }
}

void setMode(TestMode nextMode) {
  mode = nextMode;
  applyMode();

  Serial.print(F("OK MODE="));
  Serial.println(modeName());
}

void updateChase() {
  if (mode != TestMode::CHASE) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastChaseMs < ArduConfig::CHASE_INTERVAL_MS) {
    return;
  }
  lastChaseMs = now;

  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);

  // White moving point plus red marker at LED 0.
  leds[0] = CRGB(32, 0, 0);
  leds[chaseIndex] = CRGB::White;

  FastLED.setBrightness(brightness);
  FastLED.show();

  chaseIndex++;
  if (chaseIndex >= ArduConfig::LED_COUNT) {
    chaseIndex = 0;
  }
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

void printStatus() {
  Serial.print(F("STATUS FW=STRIP_TEST PIN=D6 LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" MODE="));
  Serial.print(modeName());
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(brightness);
  Serial.print(F(" MAX_MA="));
  Serial.print(ArduConfig::MAX_MILLIAMPS);
  Serial.print(F(" D7=LOW UPTIME_MS="));
  Serial.println(millis());
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

  if (strcmp(command, "OFF") == 0) {
    setMode(TestMode::OFF);
    return;
  }

  if (strcmp(command, "RED") == 0) {
    setMode(TestMode::RED);
    return;
  }

  if (strcmp(command, "GREEN") == 0) {
    setMode(TestMode::GREEN);
    return;
  }

  if (strcmp(command, "BLUE") == 0) {
    setMode(TestMode::BLUE);
    return;
  }

  if (strcmp(command, "WHITE") == 0) {
    setMode(TestMode::WHITE);
    return;
  }

  if (strcmp(command, "CHASE") == 0) {
    setMode(TestMode::CHASE);
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    brightness = value;
    applyMode();

    Serial.print(F("OK BRIGHTNESS="));
    Serial.println(brightness);
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS OFF RED GREEN BLUE WHITE CHASE BRIGHT HELP"));
    Serial.println(F("FORMAT BRIGHT <0..255>"));
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
  FastLED.setBrightness(brightness);

  mode = TestMode::OFF;
  applyMode();

  Serial.println(F("ARDU NANO STRIP TEST READY"));
  Serial.println(F("START SAFE: STRIP OFF"));
  Serial.println(F("RUN STATUS, THEN RED/GREEN/BLUE/WHITE/CHASE"));
  printStatus();
}

void loop() {
  pollSerial();
  updateChase();
}
