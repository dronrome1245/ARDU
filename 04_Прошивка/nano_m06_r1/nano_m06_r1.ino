/*
  ARDU M06 R1 — manually started, frequency-limited strobe.

  Strobe behavior is adapted from AlexGyver ColorMusic v2.10 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU R1 safety constraints:
  - never starts automatically;
  - startup POWER=OFF;
  - frequency command is limited to 0.5..3.0 flashes/s;
  - one ON session auto-stops after 15 s;
  - brightness is limited to 1..128 in this test revision;
  - OFF stops immediately.
  These are firmware limits, so ESP/app/raw developer commands cannot bypass them.

  Hardware:
  - one WS2812B ring on D6, 43 LEDs;
  - D7 held LOW until the second ring is intentionally tested.
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
constexpr uint8_t MAX_BRIGHTNESS = 128;

// Frequency is stored as flashes/second x10.
// 5 = 0.5 Hz, 10 = 1.0 Hz, 30 = 3.0 Hz.
constexpr uint8_t DEFAULT_FREQ10 = 10;
constexpr uint8_t MIN_FREQ10 = 5;
constexpr uint8_t MAX_FREQ10 = 30;

// ColorMusic v2.10 uses 20% duty by default.
constexpr uint8_t STROBE_DUTY_PERCENT = 20;
constexpr uint8_t DEFAULT_SMOOTH = 200;

constexpr unsigned long FRAME_INTERVAL_MS = 5UL;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr unsigned long MAX_RUN_MS = 15000UL;

constexpr size_t RX_BUFFER_SIZE = 80;
}

struct ArduSettings {
  bool power = false;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t frequency10 = ArduConfig::DEFAULT_FREQ10;
  uint8_t smooth = ArduConfig::DEFAULT_SMOOTH;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint8_t strobeLevel = 0;
unsigned long runStartedMs = 0;
unsigned long lastFrameMs = 0;
unsigned long lastSerialRxMs = 0;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

unsigned long strobePeriodMs() {
  // Ceiling division keeps the real rate at or below the requested rate.
  return (10000UL + settings.frequency10 - 1UL) /
         static_cast<unsigned long>(settings.frequency10);
}

unsigned long strobeLightMs() {
  unsigned long result =
      strobePeriodMs() * ArduConfig::STROBE_DUTY_PERCENT / 100UL;
  if (result < 1UL) result = 1UL;
  return result;
}

void clearRing() {
  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  FastLED.show();
}

void stopStrobe(bool automatic) {
  settings.power = false;
  strobeLevel = 0;
  clearRing();

  if (automatic) {
    Serial.println(F("EVENT STROBE_AUTO_STOP"));
  }
}

void startStrobe() {
  settings.power = true;
  strobeLevel = 0;
  runStartedMs = millis();
  lastFrameMs = 0;
}

void updateLevel(bool flashPhase) {
  if (flashPhase) {
    const uint16_t next =
        static_cast<uint16_t>(strobeLevel) + settings.smooth;
    strobeLevel = next > 255U ? 255U : static_cast<uint8_t>(next);
  } else {
    const int16_t next =
        static_cast<int16_t>(strobeLevel) - settings.smooth;
    strobeLevel = next < 0 ? 0 : static_cast<uint8_t>(next);
  }
}

void renderStrobe() {
  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      CHSV(0, 0, strobeLevel)
  );

  FastLED.setBrightness(settings.brightness);
  if (!serialRxGuardActive()) {
    FastLED.show();
  }
}

void updateM06() {
  if (!settings.power) return;

  const unsigned long now = millis();
  const unsigned long elapsed = now - runStartedMs;

  if (elapsed >= ArduConfig::MAX_RUN_MS) {
    stopStrobe(true);
    return;
  }

  if (now - lastFrameMs < ArduConfig::FRAME_INTERVAL_MS) return;
  lastFrameMs = now;

  const unsigned long period = strobePeriodMs();
  const unsigned long phase = elapsed % period;
  const bool flashPhase = phase < strobeLightMs();

  updateLevel(flashPhase);
  renderStrobe();
}

unsigned long remainingRunMs() {
  if (!settings.power) return 0UL;
  const unsigned long elapsed = millis() - runStartedMs;
  if (elapsed >= ArduConfig::MAX_RUN_MS) return 0UL;
  return ArduConfig::MAX_RUN_MS - elapsed;
}

void printStatus() {
  Serial.print(F("STATUS FW=M06 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=M06 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" FREQ10="));
  Serial.print(settings.frequency10);
  Serial.print(F(" PERIOD_MS="));
  Serial.print(strobePeriodMs());
  Serial.print(F(" DUTY="));
  Serial.print(ArduConfig::STROBE_DUTY_PERCENT);
  Serial.print(F(" SMOOTH="));
  Serial.print(settings.smooth);
  Serial.print(F(" LEVEL="));
  Serial.print(strobeLevel);
  Serial.print(F(" RUN_REMAIN_MS="));
  Serial.print(remainingRunMs());
  Serial.print(F(" MAX_RUN_MS="));
  Serial.print(ArduConfig::MAX_RUN_MS);
  Serial.print(F(" MAX_FREQ10="));
  Serial.print(ArduConfig::MAX_FREQ10);
  Serial.print(F(" MAX_BRIGHT="));
  Serial.print(ArduConfig::MAX_BRIGHTNESS);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

bool parseLongRange(
    const char* text,
    long minValue,
    long maxValue,
    long& value
) {
  if (text == nullptr || *text == '\0') return false;

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);

  if (*end != '\0' || parsed < minValue || parsed > maxValue) {
    return false;
  }

  value = parsed;
  return true;
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
    startStrobe();
    Serial.println(F("OK STROBE=ON"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    stopStrobe(false);
    Serial.println(F("OK STROBE=OFF"));
    return;
  }

  if (strncmp(command, "FREQ10 ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(
            command + 7,
            ArduConfig::MIN_FREQ10,
            ArduConfig::MAX_FREQ10,
            value)) {
      Serial.println(F("ERR BAD_FREQ10 RANGE=5..30"));
      return;
    }

    settings.frequency10 = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "SMOOTH ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(command + 7, 1, 255, value)) {
      Serial.println(F("ERR BAD_SMOOTH RANGE=1..255"));
      return;
    }

    settings.smooth = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(
            command + 7,
            1,
            ArduConfig::MAX_BRIGHTNESS,
            value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS RANGE=1..128"));
      return;
    }

    settings.brightness = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(
        F("CMDS PING STATUS ON OFF FREQ10 SMOOTH BRIGHT")
    );
    Serial.println(
        F("FREQ10 5..30 (0.5..3.0Hz) | SMOOTH 1..255 | BRIGHT 1..128")
    );
    Serial.println(F("ON starts max 15s manual session; OFF stops immediately"));
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
  FastLED.clear(true);

  Serial.println(F("ARDU NANO M06 R1 READY"));
  Serial.println(F("STROBE DEFAULT OFF; MANUAL ON ONLY; AUTO-STOP 15S"));
  printStatus();
}

void loop() {
  pollSerial();
  updateM06();
}
