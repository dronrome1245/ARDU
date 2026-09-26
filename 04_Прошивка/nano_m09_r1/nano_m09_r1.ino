/*
  ARDU M09 R1 — spectrum analyzer.

  Spectrum behavior is adapted from AlexGyver ColorMusic v2.10 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU adaptation:
  - MAX9814 Out -> A0 directly;
  - Gain -> Vdd (40 dB), AR floating;
  - ADC reference DEFAULT;
  - software DC subtraction before FHT;
  - 20 displayed spectrum bins from FHT bins 2..21, as in the original mode;
  - one WS2812B ring on D6, 43 LEDs;
  - D7 kept LOW until the second ring is intentionally tested;
  - proven M05 R4 FastLED/UART coexistence fix carried forward.
*/

#include <Arduino.h>
#include <math.h>

#define FHT_N 64
#define LOG_OUT 1
#include <FHT.h>

#ifdef SCALE
#undef SCALE
#endif

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

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

constexpr uint8_t FHT_BIN_FIRST = 2;
constexpr uint8_t FHT_BIN_LAST = 31;
constexpr uint8_t DISPLAY_BINS = 20;

constexpr uint8_t SPECTR_LOW_PASS_ADD = 3;
constexpr uint8_t CALIBRATION_FRAMES = 100;
constexpr int16_t MIC_DC_MIN = 120;
constexpr int16_t MIC_DC_MAX = 400;

// ColorMusic v2.10 spectrum-analyzer defaults.
constexpr float AVER_K = 0.006f;
constexpr uint8_t LIGHT_SMOOTH = 2;
constexpr uint8_t DEFAULT_HUE_START = 0;
constexpr uint8_t DEFAULT_HUE_STEP = 5;

constexpr unsigned long FRAME_INTERVAL_MS = 5UL;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t hueStart = ArduConfig::DEFAULT_HUE_START;
  uint8_t hueStep = ArduConfig::DEFAULT_HUE_STEP;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint8_t spectrumLowPass = 40;
int16_t micDcOffset = 250;
bool calibrated = false;

uint8_t spectrumTrail[ArduConfig::DISPLAY_BINS] = {0};
float spectrumMaxFiltered = 5.0f;

unsigned long lastFrameMs = 0;
unsigned long lastSerialRxMs = 0;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

void setDefaultAdcPrescaler() {
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void setFastAdcPrescaler() {
  // Same ADC prescaler as ColorMusic v2.10 frequency path: 32.
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void discardMicReads() {
  for (uint8_t i = 0; i < 8; ++i) {
    (void)analogRead(ArduPins::MIC_IN);
  }
}

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

void clearRing() {
  fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  FastLED.show();
}

void analyzeAudio() {
  for (uint8_t i = 0; i < FHT_N; ++i) {
    fht_input[i] =
        static_cast<int16_t>(analogRead(ArduPins::MIC_IN)) - micDcOffset;
  }

  fht_window();
  fht_reorder();
  fht_run();
  fht_mag_log();
}

void calibrateDcOffset() {
  setDefaultAdcPrescaler();
  delayMicroseconds(200);
  discardMicReads();

  uint32_t sum = 0;
  for (uint16_t i = 0; i < 256; ++i) {
    sum += analogRead(ArduPins::MIC_IN);
  }

  micDcOffset = static_cast<int16_t>(sum / 256UL);
  setFastAdcPrescaler();
}

bool micDcValid() {
  return micDcOffset >= ArduConfig::MIC_DC_MIN &&
         micDcOffset <= ArduConfig::MIC_DC_MAX;
}

void resetSpectrumState() {
  for (uint8_t i = 0; i < ArduConfig::DISPLAY_BINS; ++i) {
    spectrumTrail[i] = 0;
  }
  spectrumMaxFiltered = 5.0f;
}

void calibrateSpectrumNoise() {
  clearRing();

  delay(500);
  calibrateDcOffset();

  if (!micDcValid()) {
    calibrated = false;
    spectrumLowPass = 40;
    resetSpectrumState();

    Serial.print(F("ERR CALF_BAD_DC DC="));
    Serial.print(micDcOffset);
    Serial.print(F(" EXPECTED="));
    Serial.print(ArduConfig::MIC_DC_MIN);
    Serial.print(F(".."));
    Serial.println(ArduConfig::MIC_DC_MAX);
    return;
  }

  uint8_t quietMax = 0;

  for (uint8_t frame = 0; frame < ArduConfig::CALIBRATION_FRAMES; ++frame) {
    analyzeAudio();

    for (uint8_t bin = ArduConfig::FHT_BIN_FIRST;
         bin <= ArduConfig::FHT_BIN_LAST;
         ++bin) {
      if (fht_log_out[bin] > quietMax) {
        quietMax = fht_log_out[bin];
      }
    }

    delay(4);
  }

  const uint16_t proposed =
      static_cast<uint16_t>(quietMax) + ArduConfig::SPECTR_LOW_PASS_ADD;

  spectrumLowPass =
      proposed > 255 ? 255 : static_cast<uint8_t>(proposed);

  calibrated = true;
  resetSpectrumState();

  Serial.print(F("CALF DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.println(spectrumLowPass);
}

void updateSpectrumState() {
  analyzeAudio();

  uint8_t frameMax = 5;

  for (uint8_t i = 0; i < 30; ++i) {
    uint8_t value = fht_log_out[i + ArduConfig::FHT_BIN_FIRST];

    if (value < spectrumLowPass) {
      value = 0;
    }

    if (value > frameMax) {
      frameMax = value;
    }

    if (i < ArduConfig::DISPLAY_BINS) {
      if (spectrumTrail[i] < value) {
        spectrumTrail[i] = value;
      }

      if (spectrumTrail[i] > ArduConfig::LIGHT_SMOOTH) {
        spectrumTrail[i] -= ArduConfig::LIGHT_SMOOTH;
      } else {
        spectrumTrail[i] = 0;
      }
    }
  }

  spectrumMaxFiltered =
      frameMax * ArduConfig::AVER_K +
      spectrumMaxFiltered * (1.0f - ArduConfig::AVER_K);

  if (spectrumMaxFiltered < 5.0f) {
    spectrumMaxFiltered = 5.0f;
  }
}

uint8_t brightnessForSpectrumBin(uint8_t bin) {
  if (bin >= ArduConfig::DISPLAY_BINS || spectrumTrail[bin] == 0) {
    return 0;
  }

  float ratio = spectrumTrail[bin] / spectrumMaxFiltered;
  if (ratio > 1.0f) ratio = 1.0f;
  if (ratio < 0.0f) ratio = 0.0f;

  return static_cast<uint8_t>(ratio * 255.0f);
}

uint8_t sourceBinForSidePosition(uint8_t position) {
  // Original ColorMusic: freq_to_stripe = NUM_LEDS / 40,
  // because the image is mirrored and uses 20 displayed frequencies.
  const float freqToStripe =
      static_cast<float>(ArduConfig::LED_COUNT) / 40.0f;
  const uint8_t half = ArduConfig::LED_COUNT / 2;

  int16_t bin = static_cast<int16_t>(
      floor((half - position) / freqToStripe)
  );

  if (bin < 0) bin = 0;
  if (bin >= ArduConfig::DISPLAY_BINS) {
    bin = ArduConfig::DISPLAY_BINS - 1;
  }

  return static_cast<uint8_t>(bin);
}

uint8_t hueForSidePosition(uint8_t position) {
  return static_cast<uint8_t>(
      settings.hueStart +
      static_cast<uint16_t>(position) * settings.hueStep
  );
}

void renderM09() {
  if (!settings.power) {
    return;
  }

  constexpr uint8_t center = ArduConfig::LED_COUNT / 2;

  // Mirror 21 pixels around the odd center: [0..20] and [42..22].
  // This keeps the original analyzer direction: lower bins near the center,
  // higher displayed bins toward the DATA-chain ends.
  for (uint8_t i = 0; i < center; ++i) {
    const uint8_t bin = sourceBinForSidePosition(i);
    const uint8_t brightness = brightnessForSpectrumBin(bin);
    const CHSV color(
        hueForSidePosition(i),
        255,
        brightness
    );

    leds[i] = color;
    leds[ArduConfig::LED_COUNT - i - 1] = color;
  }

  // Odd 43-LED adaptation: the single center pixel continues the lowest bin.
  const uint8_t centerBin = sourceBinForSidePosition(center - 1);
  leds[center] = CHSV(
      hueForSidePosition(center - 1),
      255,
      brightnessForSpectrumBin(centerBin)
  );

  FastLED.setBrightness(settings.brightness);

  if (!serialRxGuardActive()) {
    FastLED.show();
  }
}

void updateM09() {
  const unsigned long now = millis();
  if (now - lastFrameMs < ArduConfig::FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  if (!settings.power) {
    return;
  }

  if (calibrated) {
    updateSpectrumState();
  }

  renderM09();
}

void printAdc() {
  setDefaultAdcPrescaler();
  delayMicroseconds(200);
  discardMicReads();

  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
  uint32_t sum = 0;

  for (uint16_t i = 0; i < 256; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    sum += sample;
  }

  Serial.print(F("ADC AVG="));
  Serial.print(static_cast<uint16_t>(sum / 256UL));
  Serial.print(F(" MIN="));
  Serial.print(minValue);
  Serial.print(F(" MAX="));
  Serial.print(maxValue);
  Serial.print(F(" P2P="));
  Serial.println(maxValue - minValue);

  setFastAdcPrescaler();
}

void printSpectrum() {
  Serial.print(F("SPEC MAXF="));
  Serial.print(spectrumMaxFiltered, 1);
  Serial.print(F(" BINS="));

  for (uint8_t i = 0; i < ArduConfig::DISPLAY_BINS; ++i) {
    if (i > 0) Serial.print(',');
    Serial.print(spectrumTrail[i]);
  }

  Serial.println();
}

void printStatus() {
  Serial.print(F("STATUS FW=M09 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=M09 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" HUE_START="));
  Serial.print(settings.hueStart);
  Serial.print(F(" HUE_STEP="));
  Serial.print(settings.hueStep);
  Serial.print(F(" LIGHT_SMOOTH="));
  Serial.print(ArduConfig::LIGHT_SMOOTH);
  Serial.print(F(" MIC=MAX9814 MIC_PIN=A0 MIC_GAIN=40DB AR=FLOAT"));
  Serial.print(F(" ADC_REF=DEFAULT DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" DC_OK="));
  Serial.print(micDcValid() ? F("YES") : F("NO"));
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.print(spectrumLowPass);
  Serial.print(F(" CAL="));
  Serial.print(calibrated ? F("YES") : F("NO"));
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" DISPLAY_BINS="));
  Serial.print(ArduConfig::DISPLAY_BINS);
  Serial.print(F(" FHT_N="));
  Serial.print(FHT_N);
  Serial.print(F(" FASTLED_IRQ=ON RX_GUARD_MS="));
  Serial.print(ArduConfig::SERIAL_RX_GUARD_MS);
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

  if (strcmp(command, "ADC") == 0) {
    printAdc();
    return;
  }

  if (strcmp(command, "CALF") == 0) {
    calibrateSpectrumNoise();
    return;
  }

  if (strcmp(command, "SPEC") == 0) {
    printSpectrum();
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

  if (strncmp(command, "HUESTART ", 9) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 9, value)) {
      Serial.println(F("ERR BAD_HUE_START"));
      return;
    }

    settings.hueStart = value;
    Serial.print(F("OK HUE_START="));
    Serial.println(settings.hueStart);
    return;
  }

  if (strncmp(command, "HUESTEP ", 8) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 8, value) || value == 0) {
      Serial.println(F("ERR BAD_HUE_STEP"));
      return;
    }

    settings.hueStep = value;
    Serial.print(F("OK HUE_STEP="));
    Serial.println(settings.hueStep);
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

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC CALF SPEC ON OFF HUESTART HUESTEP BRIGHT HELP"));
    Serial.println(F("FORMAT HUESTART <0..255> | HUESTEP <1..255> | BRIGHT <0..255>"));
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
  FastLED.setBrightness(settings.brightness);
  FastLED.clear(true);

  analogReference(DEFAULT);
  delay(500);
  calibrateDcOffset();
  resetSpectrumState();

  Serial.println(F("ARDU NANO M09 R1 READY"));
  Serial.println(F("RUN CALF IN QUIET ROOM BEFORE MUSIC TEST"));
  printStatus();
}

void loop() {
  pollSerial();
  updateM09();
}
