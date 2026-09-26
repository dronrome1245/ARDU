/*
  ARDU F02 R1 — smooth color cycle background effect.

  Behavior is adapted from AlexGyver ColorMusic v2.10 light_mode=1 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU adaptation:
  - one WS2812B ring on D6, 43 LEDs;
  - D7 kept LOW until the second ring is intentionally tested;
  - ColorMusic defaults: HUE=0, SAT=255, COLOR_SPEED=100 ms;
  - proven M05 R4 FastLED/UART coexistence fix carried forward.
*/

#include <Arduino.h>

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
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;

constexpr uint8_t DEFAULT_HUE = 0;
constexpr uint8_t DEFAULT_SATURATION = 255;
constexpr uint8_t DEFAULT_SPEED_MS = 100;

constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t hue = ArduConfig::DEFAULT_HUE;
  uint8_t saturation = ArduConfig::DEFAULT_SATURATION;
  uint8_t speedMs = ArduConfig::DEFAULT_SPEED_MS;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

unsigned long lastColorStepMs = 0;
unsigned long lastSerialRxMs = 0;
bool frameDirty = true;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
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

void prepareFrame() {
  FastLED.setBrightness(settings.brightness);

  if (settings.power) {
    fill_solid(
        leds,
        ArduConfig::LED_COUNT,
        CHSV(settings.hue, settings.saturation, 255)
    );
  } else {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
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

void updateF02() {
  if (settings.power) {
    const unsigned long now = millis();

    if (now - lastColorStepMs >= settings.speedMs) {
      lastColorStepMs = now;
      ++settings.hue;  // uint8_t wrap = ColorMusic 0..255 cycle.
      prepareFrame();
    }
  }

  showFrameIfSafe();
}

void printStatus() {
  Serial.print(F("STATUS FW=F02 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=F02 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" HUE="));
  Serial.print(settings.hue);
  Serial.print(F(" SAT="));
  Serial.print(settings.saturation);
  Serial.print(F(" SPEED_MS="));
  Serial.print(settings.speedMs);
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" UPTIME_MS="));
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

  if (strcmp(command, "ON") == 0) {
    settings.power = true;
    lastColorStepMs = millis();
    prepareFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    prepareFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    settings.brightness = value;
    prepareFrame();
    Serial.print(F("OK BRIGHTNESS="));
    Serial.println(settings.brightness);
    return;
  }

  if (strncmp(command, "HUE ", 4) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_HUE"));
      return;
    }

    settings.hue = value;
    lastColorStepMs = millis();
    prepareFrame();
    Serial.print(F("OK HUE="));
    Serial.println(settings.hue);
    return;
  }

  if (strncmp(command, "SAT ", 4) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_SAT"));
      return;
    }

    settings.saturation = value;
    prepareFrame();
    Serial.print(F("OK SAT="));
    Serial.println(settings.saturation);
    return;
  }

  if (strncmp(command, "SPEED ", 6) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 6, value) || value == 0) {
      Serial.println(F("ERR BAD_SPEED"));
      return;
    }

    settings.speedMs = value;
    lastColorStepMs = millis();
    Serial.print(F("OK SPEED_MS="));
    Serial.println(settings.speedMs);
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BRIGHT HUE SAT SPEED HELP"));
    Serial.println(F("FORMAT BRIGHT <0..255> | HUE <0..255> | SAT <0..255> | SPEED <1..255>"));
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

  prepareFrame();
  showFrameIfSafe();

  Serial.println(F("ARDU NANO F02 R1 READY"));
  printStatus();
}

void loop() {
  pollSerial();
  updateF02();
}
