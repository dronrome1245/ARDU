/*
  ARDU M02 R1 — rainbow VU music mode.

  VU dynamics and rainbow rendering are adapted from
  AlexGyver ColorMusic v2.10 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU adaptation:
  - MAX9814 Out -> A0 directly;
  - Gain -> Vdd (40 dB), AR floating;
  - ADC reference DEFAULT;
  - one WS2812B ring on D6, 43 LEDs;
  - D7 kept LOW until the second ring is intentionally tested;
  - default music background = 0;
  - inherited FastLED/UART protection from the passed M05 R4 baseline.
*/

#include <Arduino.h>

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>
#include <math.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;

constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;
constexpr uint8_t DEFAULT_BACKGROUND = 0;

constexpr uint8_t VU_SAMPLES = 100;
constexpr uint16_t LOW_PASS_ADD = 13;
constexpr float EXPONENT = 1.4f;
constexpr float AVER_K = 0.006f;
constexpr float MAX_COEF = 1.8f;
constexpr unsigned long FRAME_INTERVAL_MS = 5UL;
constexpr unsigned long RAINBOW_INTERVAL_MS = 30UL;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;

constexpr int16_t MIC_DC_MIN = 120;
constexpr int16_t MIC_DC_MAX = 400;
constexpr uint8_t DEFAULT_SMOOTH_PERCENT = 30;
constexpr uint16_t DEFAULT_RAINBOW_STEP10 = 50;

constexpr size_t RX_BUFFER_SIZE = 80;
constexpr uint8_t HALF_RING = LED_COUNT / 2;
}

struct AdcStats {
  uint16_t avg = 0;
  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
};

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t backgroundBrightness = ArduConfig::DEFAULT_BACKGROUND;
  uint8_t smoothPercent = ArduConfig::DEFAULT_SMOOTH_PERCENT;
  uint16_t rainbowStep10 = ArduConfig::DEFAULT_RAINBOW_STEP10;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t lowPass = 300;
uint16_t lastPeak = 0;
int16_t micDcOffset = 250;
bool calibrated = false;

float soundLevel = 0.0f;
float soundLevelFiltered = 0.0f;
float averageLevel = 50.0f;
float maxLevel = 100.0f;
uint8_t vuPairs = 0;

uint16_t rainbowHue10 = 0;
unsigned long lastFrameMs = 0;
unsigned long lastRainbowMs = 0;
unsigned long lastSerialRxMs = 0;

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

void clearRing() {
  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  FastLED.show();
}

AdcStats measureAdcStats(uint16_t sampleCount) {
  AdcStats stats;
  uint32_t sum = 0;

  for (uint16_t i = 0; i < sampleCount; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample < stats.minValue) stats.minValue = sample;
    if (sample > stats.maxValue) stats.maxValue = sample;
    sum += sample;
  }

  stats.avg = static_cast<uint16_t>(sum / sampleCount);
  return stats;
}

bool micDcValid() {
  return micDcOffset >= ArduConfig::MIC_DC_MIN &&
         micDcOffset <= ArduConfig::MIC_DC_MAX;
}

void resetVuState() {
  soundLevel = 0.0f;
  soundLevelFiltered = 0.0f;
  averageLevel = 50.0f;
  maxLevel = 100.0f;
  vuPairs = 0;
}

uint16_t readPositivePeak() {
  uint16_t peak = 0;

  for (uint8_t i = 0; i < ArduConfig::VU_SAMPLES; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample > peak) peak = sample;
  }

  return peak;
}

void calibrateNoise() {
  const bool wasOn = settings.power;
  settings.power = false;
  clearRing();
  delay(100);

  const AdcStats dcStats = measureAdcStats(256);
  micDcOffset = static_cast<int16_t>(dcStats.avg);

  if (!micDcValid()) {
    calibrated = false;
    resetVuState();
    settings.power = wasOn;

    Serial.print(F("ERR CAL_BAD_DC DC="));
    Serial.print(micDcOffset);
    Serial.print(F(" EXPECTED="));
    Serial.print(ArduConfig::MIC_DC_MIN);
    Serial.print(F(".."));
    Serial.println(ArduConfig::MIC_DC_MAX);
    return;
  }

  uint16_t quietMax = 0;
  for (uint16_t i = 0; i < 200; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample > quietMax) quietMax = sample;
    delay(4);
  }

  const uint32_t proposed =
      static_cast<uint32_t>(quietMax) + ArduConfig::LOW_PASS_ADD;
  lowPass =
      proposed > 1000UL ? 1000U : static_cast<uint16_t>(proposed);

  calibrated = true;
  resetVuState();
  settings.power = wasOn;

  Serial.print(F("CAL DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" LOW_PASS="));
  Serial.println(lowPass);
}

void updateRainbowHue() {
  const unsigned long now = millis();
  if (now - lastRainbowMs < ArduConfig::RAINBOW_INTERVAL_MS) return;
  lastRainbowMs = now;

  rainbowHue10 = static_cast<uint16_t>(
      (rainbowHue10 + settings.rainbowStep10) % 2560U
  );
}

void renderM02() {
  if (!settings.power) return;

  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      CHSV(HUE_PURPLE, 255, settings.backgroundBrightness)
  );

  if (vuPairs > 0) {
    const uint8_t hueOffset =
        static_cast<uint8_t>(rainbowHue10 / 10U);

    for (uint8_t step = 0; step <= vuPairs; ++step) {
      const uint16_t halfPalette =
          (static_cast<uint16_t>(step) * 255U) /
          (static_cast<uint16_t>(ArduConfig::HALF_RING) * 2U);
      const uint8_t paletteIndex =
          static_cast<uint8_t>(halfPalette - hueOffset);
      const CRGB color =
          ColorFromPalette(RainbowColors_p, paletteIndex);

      if (step == 0) {
        leds[0] = color;
      } else {
        leds[step] = color;
        leds[ArduConfig::LED_COUNT - step] = color;
      }
    }
  }

  FastLED.setBrightness(settings.brightness);
  if (!serialRxGuardActive()) FastLED.show();
}

void updateM02() {
  const unsigned long now = millis();
  if (now - lastFrameMs < ArduConfig::FRAME_INTERVAL_MS) return;
  lastFrameMs = now;

  if (!settings.power) return;

  updateRainbowHue();
  lastPeak = readPositivePeak();

  if (lastPeak <= lowPass) {
    soundLevel = 0.0f;
  } else {
    const float mapped =
        (static_cast<float>(lastPeak - lowPass) * 500.0f) /
        static_cast<float>(1023U - lowPass);
    const float constrained =
        mapped < 0.0f ? 0.0f :
        (mapped > 500.0f ? 500.0f : mapped);
    soundLevel = pow(constrained, ArduConfig::EXPONENT);
  }

  const float smooth =
      static_cast<float>(settings.smoothPercent) / 100.0f;
  soundLevelFiltered =
      soundLevel * smooth +
      soundLevelFiltered * (1.0f - smooth);

  if (soundLevelFiltered > 15.0f) {
    averageLevel =
        soundLevelFiltered * ArduConfig::AVER_K +
        averageLevel * (1.0f - ArduConfig::AVER_K);

    maxLevel = averageLevel * ArduConfig::MAX_COEF;
    if (maxLevel < 1.0f) maxLevel = 1.0f;

    float ratio = soundLevelFiltered / maxLevel;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    vuPairs = static_cast<uint8_t>(
        ratio * static_cast<float>(ArduConfig::HALF_RING)
    );
  } else {
    vuPairs = 0;
  }

  renderM02();
}

void printAdc() {
  const AdcStats stats = measureAdcStats(256);
  Serial.print(F("ADC AVG="));
  Serial.print(stats.avg);
  Serial.print(F(" MIN="));
  Serial.print(stats.minValue);
  Serial.print(F(" MAX="));
  Serial.print(stats.maxValue);
  Serial.print(F(" P2P="));
  Serial.println(stats.maxValue - stats.minValue);
}

void printAudio() {
  Serial.print(F("AUDIO PEAK="));
  Serial.print(lastPeak);
  Serial.print(F(" LOW_PASS="));
  Serial.print(lowPass);
  Serial.print(F(" LEVEL="));
  Serial.print(soundLevel, 1);
  Serial.print(F(" FILTERED="));
  Serial.print(soundLevelFiltered, 1);
  Serial.print(F(" AVG="));
  Serial.print(averageLevel, 1);
  Serial.print(F(" MAX="));
  Serial.print(maxLevel, 1);
  Serial.print(F(" PAIRS="));
  Serial.println(vuPairs);
}

void printStatus() {
  Serial.print(F("STATUS FW=M02 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=M02 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" BACKGROUND="));
  Serial.print(settings.backgroundBrightness);
  Serial.print(F(" SMOOTH="));
  Serial.print(settings.smoothPercent);
  Serial.print(F(" RAINSTEP10="));
  Serial.print(settings.rainbowStep10);
  Serial.print(
      F(" MIC=MAX9814 MIC_PIN=A0 MIC_GAIN=40DB AR=FLOAT ADC_REF=DEFAULT")
  );
  Serial.print(F(" DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" DC_OK="));
  Serial.print(micDcValid() ? F("YES") : F("NO"));
  Serial.print(F(" CAL="));
  Serial.print(calibrated ? F("YES") : F("NO"));
  Serial.print(F(" LOW_PASS="));
  Serial.print(lowPass);
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

  if (strcmp(command, "ADC") == 0) {
    printAdc();
    return;
  }

  if (strcmp(command, "AUDIO") == 0) {
    printAudio();
    return;
  }

  if (strcmp(command, "CAL") == 0) {
    calibrateNoise();
    return;
  }

  if (strcmp(command, "ON") == 0) {
    settings.power = true;
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    clearRing();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(command + 7, 0, 255, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }
    settings.brightness = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BACKGROUND ", 11) == 0) {
    long value = 0;
    if (!parseLongRange(command + 11, 0, 255, value)) {
      Serial.println(F("ERR BAD_BACKGROUND"));
      return;
    }
    settings.backgroundBrightness = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "SMOOTH ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(command + 7, 5, 100, value)) {
      Serial.println(F("ERR BAD_SMOOTH"));
      return;
    }
    settings.smoothPercent = static_cast<uint8_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "RAINSTEP10 ", 11) == 0) {
    long value = 0;
    if (!parseLongRange(command + 11, 5, 200, value)) {
      Serial.println(F("ERR BAD_RAINSTEP10"));
      return;
    }
    settings.rainbowStep10 = static_cast<uint16_t>(value);
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(
        F("CMDS PING STATUS ADC AUDIO CAL ON OFF BRIGHT BACKGROUND SMOOTH RAINSTEP10")
    );
    Serial.println(
        F("BRIGHT/BACKGROUND 0..255 | SMOOTH 5..100 | RAINSTEP10 5..200")
    );
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
  analogReference(DEFAULT);

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

  delay(20);
  micDcOffset =
      static_cast<int16_t>(measureAdcStats(256).avg);

  Serial.println(F("ARDU NANO M02 R1 READY"));
  Serial.println(
      F("RUN ADC THEN CAL IN QUIET ROOM BEFORE MUSIC TEST")
  );
  printStatus();
}

void loop() {
  pollSerial();
  updateM02();
}
