#include <Arduino.h>
#include <FastLED.h>
#include <math.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RTC_SDA = A4;
constexpr uint8_t RTC_SCL = A5;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;
constexpr uint8_t BACKGROUND_BRIGHTNESS = 6;

constexpr uint8_t VU_SAMPLES = 100;
constexpr uint16_t LOW_PASS_ADD = 13;
constexpr float EXPONENT = 1.4f;
constexpr float SMOOTH = 0.30f;
constexpr float AVER_K = 0.006f;
constexpr float MAX_COEF = 1.8f;
constexpr unsigned long FRAME_INTERVAL_MS = 5UL;

constexpr size_t RX_BUFFER_SIZE = 64;
constexpr uint8_t HALF_RING = LED_COUNT / 2;
}

DEFINE_GRADIENT_PALETTE(arduVuGradient) {
  0,   0,   255, 0,
  96,  255, 255, 0,
  160, 255, 80,  0,
  255, 255, 0,   0
};

CRGBPalette32 vuPalette = arduVuGradient;
CRGB leds[ArduConfig::LED_COUNT];

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t backgroundBrightness = ArduConfig::BACKGROUND_BRIGHTNESS;
};

ArduSettings settings;

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t lowPass = 300;
uint16_t lastPeak = 0;
float soundLevel = 0.0f;
float soundLevelFiltered = 0.0f;
float averageLevel = 50.0f;
float maxLevel = 100.0f;
uint8_t vuPairs = 0;
unsigned long lastFrameMs = 0;

void clearRing() {
  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  FastLED.show();
}

uint16_t readPositivePeak() {
  uint16_t peak = 0;

  for (uint8_t i = 0; i < ArduConfig::VU_SAMPLES; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample > peak) {
      peak = sample;
    }
  }

  return peak;
}

void calibrateNoise() {
  const bool wasOn = settings.power;
  settings.power = false;
  clearRing();

  delay(100);

  uint16_t quietMax = 0;

  for (uint16_t i = 0; i < 200; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample > quietMax) {
      quietMax = sample;
    }
    delay(4);
  }

  lowPass = quietMax + ArduConfig::LOW_PASS_ADD;
  if (lowPass > 1000) {
    lowPass = 1000;
  }

  soundLevel = 0.0f;
  soundLevelFiltered = 0.0f;
  averageLevel = 50.0f;
  maxLevel = 100.0f;
  vuPairs = 0;

  settings.power = wasOn;

  Serial.print(F("CAL QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" LOW_PASS="));
  Serial.println(lowPass);
}

void renderM01() {
  if (!settings.power) {
    clearRing();
    return;
  }

  const CRGB background = settings.backgroundBrightness > 0
      ? CHSV(HUE_PURPLE, 255, settings.backgroundBrightness)
      : CRGB::Black;

  fill_solid(leds, ArduConfig::LED_COUNT, background);

  if (vuPairs > 0) {
    leds[0] = ColorFromPalette(vuPalette, 0);

    for (uint8_t step = 1; step <= vuPairs; ++step) {
      const uint8_t paletteIndex =
          static_cast<uint8_t>((static_cast<uint16_t>(step) * 255U) /
                               ArduConfig::HALF_RING);
      const CRGB color = ColorFromPalette(vuPalette, paletteIndex);

      leds[step] = color;
      leds[ArduConfig::LED_COUNT - step] = color;
    }
  }

  FastLED.setBrightness(settings.brightness);
  FastLED.show();
}

void updateM01() {
  if (millis() - lastFrameMs < ArduConfig::FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = millis();

  lastPeak = readPositivePeak();

  if (lastPeak <= lowPass) {
    soundLevel = 0.0f;
  } else {
    const float mapped =
        (static_cast<float>(lastPeak - lowPass) * 500.0f) /
        static_cast<float>(1023U - lowPass);

    const float constrained =
        mapped < 0.0f ? 0.0f : (mapped > 500.0f ? 500.0f : mapped);

    soundLevel = pow(constrained, ArduConfig::EXPONENT);
  }

  soundLevelFiltered =
      soundLevel * ArduConfig::SMOOTH +
      soundLevelFiltered * (1.0f - ArduConfig::SMOOTH);

  if (soundLevelFiltered > 15.0f) {
    averageLevel =
        soundLevelFiltered * ArduConfig::AVER_K +
        averageLevel * (1.0f - ArduConfig::AVER_K);

    maxLevel = averageLevel * ArduConfig::MAX_COEF;
    if (maxLevel < 1.0f) {
      maxLevel = 1.0f;
    }

    float ratio = soundLevelFiltered / maxLevel;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    vuPairs = static_cast<uint8_t>(
        ratio * static_cast<float>(ArduConfig::HALF_RING)
    );
  } else {
    vuPairs = 0;
  }

  renderM01();
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
  Serial.print(F("STATUS FW=M01 MODE=M01 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" BACKGROUND="));
  Serial.print(settings.backgroundBrightness);
  Serial.print(F(" LOW_PASS="));
  Serial.print(lowPass);
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
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

void handleCommand(char* command) {
  if (strcmp(command, "PING") == 0) {
    Serial.println(F("PONG"));
    return;
  }

  if (strcmp(command, "STATUS") == 0) {
    printStatus();
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
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    settings.brightness = value;
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BACKGROUND ", 11) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 11, value)) {
      Serial.println(F("ERR BAD_BACKGROUND"));
      return;
    }

    settings.backgroundBrightness = value;
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS AUDIO CAL ON OFF BRIGHT BACKGROUND"));
    Serial.println(F("FORMAT BRIGHT <0..255> | BACKGROUND <0..255>"));
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
  FastLED.clear(true);

  Serial.println(F("ARDU NANO M01 READY"));
  Serial.println(F("RUN CAL IN QUIET ROOM BEFORE MUSIC TEST"));
  printStatus();
}

void loop() {
  pollSerial();
  updateM01();
}
