/*
  ARDU F03 R1 — running rainbow background effect.

  Behavior is adapted from AlexGyver ColorMusic v2.10 light_mode=2 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU adaptation:
  - one WS2812B ring on D6, 43 LEDs;
  - D7 kept LOW until the second ring is intentionally tested;
  - original baseline: rainbow timer 30 ms, RAINBOW_PERIOD=1, RAINBOW_STEP_2=0.5;
  - SPEED is hue shift per 30 ms (1..20);
  - STEP10 is spatial rainbow step multiplied by 10 (5..100 => 0.5..10.0);
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

constexpr uint8_t DEFAULT_SPEED = 1;
constexpr uint8_t DEFAULT_STEP10 = 5;  // 0.5 hue units per LED.
constexpr unsigned long RAINBOW_INTERVAL_MS = 30UL;

constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t speed = ArduConfig::DEFAULT_SPEED;
  uint8_t step10 = ArduConfig::DEFAULT_STEP10;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint8_t baseHue = 0;
unsigned long lastRainbowStepMs = 0;
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

uint8_t hueForLed(uint16_t index) {
  // Keep ColorMusic's fractional RAINBOW_STEP_2 without float state:
  // step10 stores tenths of a hue unit.
  const uint32_t offset10 =
      static_cast<uint32_t>(index) * settings.step10;
  const uint16_t offsetHue = static_cast<uint16_t>(offset10 / 10UL);

  return static_cast<uint8_t>(baseHue + offsetHue);
}

void prepareFrame() {
  FastLED.setBrightness(settings.brightness);

  if (settings.power) {
    for (uint16_t i = 0; i < ArduConfig::LED_COUNT; ++i) {
      leds[i] = CHSV(hueForLed(i), 255, 255);
    }
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

void updateF03() {
  if (settings.power) {
    const unsigned long now = millis();

    if (now - lastRainbowStepMs >= ArduConfig::RAINBOW_INTERVAL_MS) {
      lastRainbowStepMs = now;
      baseHue = static_cast<uint8_t>(baseHue + settings.speed);
      prepareFrame();
    }
  }

  showFrameIfSafe();
}

void printStatus() {
  Serial.print(F("STATUS FW=F03 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=F03 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" BASE_HUE="));
  Serial.print(baseHue);
  Serial.print(F(" SPEED="));
  Serial.print(settings.speed);
  Serial.print(F(" STEP10="));
  Serial.print(settings.step10);
  Serial.print(F(" FRAME_MS="));
  Serial.print(ArduConfig::RAINBOW_INTERVAL_MS);
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
    lastRainbowStepMs = millis();
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

  if (strncmp(command, "SPEED ", 6) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 6, value) || value < 1 || value > 20) {
      Serial.println(F("ERR BAD_SPEED"));
      return;
    }

    settings.speed = value;
    Serial.print(F("OK SPEED="));
    Serial.println(settings.speed);
    return;
  }

  if (strncmp(command, "STEP10 ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value) || value < 5 || value > 100) {
      Serial.println(F("ERR BAD_STEP10"));
      return;
    }

    settings.step10 = value;
    prepareFrame();
    Serial.print(F("OK STEP10="));
    Serial.println(settings.step10);
    return;
  }

  if (strncmp(command, "HUE ", 4) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_HUE"));
      return;
    }

    baseHue = value;
    prepareFrame();
    Serial.print(F("OK BASE_HUE="));
    Serial.println(baseHue);
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BRIGHT SPEED STEP10 HUE HELP"));
    Serial.println(F("FORMAT BRIGHT <0..255> | SPEED <1..20> | STEP10 <5..100> | HUE <0..255>"));
    Serial.println(F("STEP10=5 means rainbow step 0.5 per LED"));
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

  Serial.println(F("ARDU NANO F03 R1 READY"));
  printStatus();
}

void loop() {
  pollSerial();
  updateF03();
}
