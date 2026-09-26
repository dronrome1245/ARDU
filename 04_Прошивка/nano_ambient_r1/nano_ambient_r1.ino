/*
  ARDU AMBIENT R1 — integrated background controller for F01/F02/F03.

  Effects:
  - F01: constant HSV color;
  - F02: smooth whole-ring hue cycle;
  - F03: moving rainbow along the ring.

  References:
  - ARDU F01 hardware baseline;
  - ARDU F02 R1 hardware pass;
  - ARDU F03 R1 hardware pass;
  - AlexGyver ColorMusic v2.10 light modes (MIT).

  ARDU hardware:
  - one WS2812B ring on D6, 43 LEDs;
  - D7 held LOW until second-ring testing;
  - proven FastLED/UART coexistence fix:
    FASTLED_ALLOW_INTERRUPTS=1 + 5 ms Serial RX guard.
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

constexpr uint8_t F01_DEFAULT_HUE = 0;
constexpr uint8_t F01_DEFAULT_SAT = 255;

constexpr uint8_t F02_DEFAULT_HUE = 0;
constexpr uint8_t F02_DEFAULT_SAT = 255;
constexpr uint8_t F02_DEFAULT_SPEED_MS = 100;

constexpr uint8_t F03_DEFAULT_HUE = 0;
constexpr uint8_t F03_DEFAULT_SPEED = 1;
constexpr uint8_t F03_DEFAULT_STEP10 = 5;
constexpr unsigned long F03_FRAME_MS = 30UL;

constexpr uint8_t DEFAULT_AUTO_PERIOD_S = 10;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;
}

enum class AmbientEffect : uint8_t {
  F01 = 0,
  F02 = 1,
  F03 = 2
};

struct AmbientSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;

  AmbientEffect effect = AmbientEffect::F01;

  uint8_t f01Hue = ArduConfig::F01_DEFAULT_HUE;
  uint8_t f01Sat = ArduConfig::F01_DEFAULT_SAT;

  uint8_t f02Hue = ArduConfig::F02_DEFAULT_HUE;
  uint8_t f02Sat = ArduConfig::F02_DEFAULT_SAT;
  uint8_t f02SpeedMs = ArduConfig::F02_DEFAULT_SPEED_MS;

  uint8_t f03Hue = ArduConfig::F03_DEFAULT_HUE;
  uint8_t f03Speed = ArduConfig::F03_DEFAULT_SPEED;
  uint8_t f03Step10 = ArduConfig::F03_DEFAULT_STEP10;

  bool autoCycle = false;
  uint8_t autoPeriodSec = ArduConfig::DEFAULT_AUTO_PERIOD_S;
};

AmbientSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

unsigned long lastSerialRxMs = 0;
unsigned long lastEffectStepMs = 0;
unsigned long lastAutoSwitchMs = 0;
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

const __FlashStringHelper* effectName() {
  switch (settings.effect) {
    case AmbientEffect::F01: return F("F01");
    case AmbientEffect::F02: return F("F02");
    case AmbientEffect::F03: return F("F03");
  }
  return F("UNKNOWN");
}

uint8_t f03HueForLed(uint16_t index) {
  const uint32_t offset10 =
      static_cast<uint32_t>(index) * settings.f03Step10;
  const uint16_t offsetHue = static_cast<uint16_t>(offset10 / 10UL);
  return static_cast<uint8_t>(settings.f03Hue + offsetHue);
}

void prepareCurrentEffectFrame() {
  FastLED.setBrightness(settings.brightness);

  if (!settings.power) {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
    frameDirty = true;
    return;
  }

  switch (settings.effect) {
    case AmbientEffect::F01:
      fill_solid(
          leds,
          ArduConfig::LED_COUNT,
          CHSV(settings.f01Hue, settings.f01Sat, 255)
      );
      break;

    case AmbientEffect::F02:
      fill_solid(
          leds,
          ArduConfig::LED_COUNT,
          CHSV(settings.f02Hue, settings.f02Sat, 255)
      );
      break;

    case AmbientEffect::F03:
      for (uint16_t i = 0; i < ArduConfig::LED_COUNT; ++i) {
        leds[i] = CHSV(f03HueForLed(i), 255, 255);
      }
      break;
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

void selectEffect(AmbientEffect effect, bool resetAutoTimer) {
  settings.effect = effect;
  lastEffectStepMs = millis();

  if (resetAutoTimer) {
    lastAutoSwitchMs = millis();
  }

  prepareCurrentEffectFrame();
}

void nextEffect(bool resetAutoTimer) {
  switch (settings.effect) {
    case AmbientEffect::F01:
      selectEffect(AmbientEffect::F02, resetAutoTimer);
      break;
    case AmbientEffect::F02:
      selectEffect(AmbientEffect::F03, resetAutoTimer);
      break;
    case AmbientEffect::F03:
      selectEffect(AmbientEffect::F01, resetAutoTimer);
      break;
  }
}

void previousEffect(bool resetAutoTimer) {
  switch (settings.effect) {
    case AmbientEffect::F01:
      selectEffect(AmbientEffect::F03, resetAutoTimer);
      break;
    case AmbientEffect::F02:
      selectEffect(AmbientEffect::F01, resetAutoTimer);
      break;
    case AmbientEffect::F03:
      selectEffect(AmbientEffect::F02, resetAutoTimer);
      break;
  }
}

void updateAutoCycle(unsigned long now) {
  if (!settings.power || !settings.autoCycle) {
    return;
  }

  const unsigned long periodMs =
      static_cast<unsigned long>(settings.autoPeriodSec) * 1000UL;

  if (now - lastAutoSwitchMs >= periodMs) {
    nextEffect(false);
    lastAutoSwitchMs = now;
  }
}

void updateCurrentEffect(unsigned long now) {
  if (!settings.power) {
    return;
  }

  switch (settings.effect) {
    case AmbientEffect::F01:
      break;

    case AmbientEffect::F02:
      if (now - lastEffectStepMs >= settings.f02SpeedMs) {
        lastEffectStepMs = now;
        ++settings.f02Hue;
        prepareCurrentEffectFrame();
      }
      break;

    case AmbientEffect::F03:
      if (now - lastEffectStepMs >= ArduConfig::F03_FRAME_MS) {
        lastEffectStepMs = now;
        settings.f03Hue =
            static_cast<uint8_t>(settings.f03Hue + settings.f03Speed);
        prepareCurrentEffectFrame();
      }
      break;
  }
}

void updateAmbient() {
  const unsigned long now = millis();

  updateAutoCycle(now);
  updateCurrentEffect(now);
  showFrameIfSafe();
}

void printStatus() {
  Serial.print(F("STATUS FW=AMBIENT REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=AMBIENT POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" EFFECT="));
  Serial.print(effectName());
  Serial.print(F(" AUTO="));
  Serial.print(settings.autoCycle ? F("ON") : F("OFF"));
  Serial.print(F(" PERIOD_S="));
  Serial.print(settings.autoPeriodSec);
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);

  Serial.print(F(" F01_HUE="));
  Serial.print(settings.f01Hue);
  Serial.print(F(" F01_SAT="));
  Serial.print(settings.f01Sat);

  Serial.print(F(" F02_HUE="));
  Serial.print(settings.f02Hue);
  Serial.print(F(" F02_SAT="));
  Serial.print(settings.f02Sat);
  Serial.print(F(" F02_SPEED_MS="));
  Serial.print(settings.f02SpeedMs);

  Serial.print(F(" F03_HUE="));
  Serial.print(settings.f03Hue);
  Serial.print(F(" F03_SPEED="));
  Serial.print(settings.f03Speed);
  Serial.print(F(" F03_STEP10="));
  Serial.print(settings.f03Step10);

  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

bool parseEffect(const char* text, AmbientEffect& effect) {
  if (strcmp(text, "F01") == 0) {
    effect = AmbientEffect::F01;
    return true;
  }
  if (strcmp(text, "F02") == 0) {
    effect = AmbientEffect::F02;
    return true;
  }
  if (strcmp(text, "F03") == 0) {
    effect = AmbientEffect::F03;
    return true;
  }
  return false;
}

void handleHueCommand(uint8_t value) {
  switch (settings.effect) {
    case AmbientEffect::F01:
      settings.f01Hue = value;
      break;
    case AmbientEffect::F02:
      settings.f02Hue = value;
      break;
    case AmbientEffect::F03:
      settings.f03Hue = value;
      break;
  }

  lastEffectStepMs = millis();
  prepareCurrentEffectFrame();

  Serial.print(F("OK HUE="));
  Serial.println(value);
}

void handleSatCommand(uint8_t value) {
  switch (settings.effect) {
    case AmbientEffect::F01:
      settings.f01Sat = value;
      break;
    case AmbientEffect::F02:
      settings.f02Sat = value;
      break;
    case AmbientEffect::F03:
      Serial.println(F("ERR SAT_NOT_APPLICABLE"));
      return;
  }

  prepareCurrentEffectFrame();

  Serial.print(F("OK SAT="));
  Serial.println(value);
}

void handleSpeedCommand(uint8_t value) {
  switch (settings.effect) {
    case AmbientEffect::F01:
      Serial.println(F("ERR SPEED_NOT_APPLICABLE"));
      return;

    case AmbientEffect::F02:
      if (value == 0) {
        Serial.println(F("ERR BAD_SPEED"));
        return;
      }
      settings.f02SpeedMs = value;
      lastEffectStepMs = millis();
      Serial.print(F("OK F02_SPEED_MS="));
      Serial.println(settings.f02SpeedMs);
      return;

    case AmbientEffect::F03:
      if (value < 1 || value > 20) {
        Serial.println(F("ERR BAD_SPEED"));
        return;
      }
      settings.f03Speed = value;
      Serial.print(F("OK F03_SPEED="));
      Serial.println(settings.f03Speed);
      return;
  }
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
    lastEffectStepMs = millis();
    lastAutoSwitchMs = millis();
    prepareCurrentEffectFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    prepareCurrentEffectFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "EFFECT ", 7) == 0) {
    AmbientEffect effect;
    if (!parseEffect(command + 7, effect)) {
      Serial.println(F("ERR BAD_EFFECT"));
      return;
    }

    selectEffect(effect, true);
    Serial.print(F("OK EFFECT="));
    Serial.println(effectName());
    return;
  }

  if (strcmp(command, "NEXT") == 0) {
    nextEffect(true);
    Serial.print(F("OK EFFECT="));
    Serial.println(effectName());
    return;
  }

  if (strcmp(command, "PREV") == 0) {
    previousEffect(true);
    Serial.print(F("OK EFFECT="));
    Serial.println(effectName());
    return;
  }

  if (strcmp(command, "AUTO ON") == 0) {
    settings.autoCycle = true;
    lastAutoSwitchMs = millis();
    Serial.println(F("OK AUTO=ON"));
    return;
  }

  if (strcmp(command, "AUTO OFF") == 0) {
    settings.autoCycle = false;
    Serial.println(F("OK AUTO=OFF"));
    return;
  }

  if (strncmp(command, "PERIOD ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value) || value == 0) {
      Serial.println(F("ERR BAD_PERIOD"));
      return;
    }

    settings.autoPeriodSec = value;
    lastAutoSwitchMs = millis();
    Serial.print(F("OK PERIOD_S="));
    Serial.println(settings.autoPeriodSec);
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    settings.brightness = value;
    prepareCurrentEffectFrame();
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

    handleHueCommand(value);
    return;
  }

  if (strncmp(command, "SAT ", 4) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 4, value)) {
      Serial.println(F("ERR BAD_SAT"));
      return;
    }

    handleSatCommand(value);
    return;
  }

  if (strncmp(command, "SPEED ", 6) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 6, value)) {
      Serial.println(F("ERR BAD_SPEED"));
      return;
    }

    handleSpeedCommand(value);
    return;
  }

  if (strncmp(command, "STEP10 ", 7) == 0) {
    if (settings.effect != AmbientEffect::F03) {
      Serial.println(F("ERR STEP_NOT_APPLICABLE"));
      return;
    }

    uint8_t value = 0;
    if (!parseByte(command + 7, value) || value < 5 || value > 100) {
      Serial.println(F("ERR BAD_STEP10"));
      return;
    }

    settings.f03Step10 = value;
    prepareCurrentEffectFrame();
    Serial.print(F("OK F03_STEP10="));
    Serial.println(settings.f03Step10);
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF EFFECT NEXT PREV AUTO PERIOD BRIGHT HUE SAT SPEED STEP10 HELP"));
    Serial.println(F("FORMAT EFFECT <F01|F02|F03> | AUTO <ON|OFF> | PERIOD <1..255 sec>"));
    Serial.println(F("CURRENT EFFECT PARAMS: HUE/SAT/SPEED; STEP10 only F03"));
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

  lastEffectStepMs = millis();
  lastAutoSwitchMs = millis();

  prepareCurrentEffectFrame();
  showFrameIfSafe();

  Serial.println(F("ARDU NANO AMBIENT R1 READY"));
  printStatus();
}

void loop() {
  pollSerial();
  updateAmbient();
}
