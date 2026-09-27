/*
  ARDU L01 R1 — ordinary constant white light, single-ring hardware layer.

  Purpose:
  - validate ordinary RGB-white light on the proven D6 / 43-LED ring;
  - validate brightness control, ON/OFF, FastLED current limiting and UART;
  - clap detection is intentionally NOT included in R1;
  - full settings persistence is intentionally deferred to the integrated firmware.

  Hardware:
  - ring A: D6, 43 WS2812B;
  - ring B: D7 held LOW until the second ring is intentionally tested.

  Startup:
  - POWER=ON in this standalone R1 to validate the owner's cold-power-on behavior.
  - Distinguishing service Reset from a real 230 V power-on belongs to the
    integrated state machine and is not claimed by this isolated test sketch.
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
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

unsigned long lastSerialRxMs = 0;
bool renderDirty = true;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

void renderLightNow() {
  if (!settings.power) {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  } else {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::White);
  }

  FastLED.setBrightness(settings.brightness);
  FastLED.show();
  renderDirty = false;
}

void requestRender() {
  renderDirty = true;
}

void renderIfReady() {
  if (!renderDirty) return;
  if (serialRxGuardActive()) return;
  renderLightNow();
}

void printStatus() {
  Serial.print(F("STATUS FW=L01 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=L01 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" WHITE=RGB255,255,255"));
  Serial.print(F(" LIMIT_MA="));
  Serial.print(ArduConfig::MAX_MILLIAMPS);
  Serial.print(F(" STARTUP=ON_R1"));
  Serial.print(F(" PERSISTENCE=NO_R1"));
  Serial.print(F(" CLAP=NOT_IN_R1"));
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

bool parseByte(const char* text, uint8_t& value) {
  if (text == nullptr || *text == '\0') return false;

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);

  if (*end != '\0' || parsed < 0 || parsed > 255) {
    return false;
  }

  value = static_cast<uint8_t>(parsed);
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
    settings.power = true;
    requestRender();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    renderLightNow();
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
    requestRender();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BRIGHT HELP"));
    Serial.println(F("FORMAT BRIGHT <0..255>"));
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

  Serial.println(F("ARDU NANO L01 R1 READY"));
  Serial.println(F("ORDINARY WHITE LIGHT; STARTUP ON; SINGLE-RING LIMIT 500MA"));
  printStatus();
}

void loop() {
  pollSerial();
  renderIfReady();
}
