/*
  ARDU M05 R3 — one-strip frequency music mode with selectable band submodes.

  Frequency grouping, noise gate and flash logic
  are adapted from AlexGyver ColorMusic v2.10 (MIT):
  https://github.com/AlexGyver/ColorMusic

  ARDU adaptation:
  - MAX9814 Out -> A0 directly;
  - Gain -> Vdd (40 dB), AR floating;
  - ADC reference DEFAULT;
  - software DC subtraction before FHT;
  - one WS2812B ring on D6, 43 LEDs;
  - D7 kept LOW until the second ring is intentionally tested.
*/

#include <Arduino.h>

#define FHT_N 64
#define LOG_OUT 1
#include <FHT.h>

#ifdef SCALE
#undef SCALE
#endif

#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 3;

constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;
constexpr uint8_t DEFAULT_BACKGROUND = 0;

constexpr uint8_t LOW_BIN_FIRST = 2;
constexpr uint8_t LOW_BIN_LAST = 5;
constexpr uint8_t MID_BIN_FIRST = 6;
constexpr uint8_t MID_BIN_LAST = 10;
constexpr uint8_t HIGH_BIN_FIRST = 11;
constexpr uint8_t HIGH_BIN_LAST = 31;

constexpr uint8_t SPECTR_LOW_PASS_ADD = 3;
constexpr uint8_t CALIBRATION_FRAMES = 100;
constexpr int16_t MIC_DC_MIN = 120;
constexpr int16_t MIC_DC_MAX = 400;

// ColorMusic v2.10 frequency-mode defaults.
constexpr float AVER_K = 0.006f;
constexpr float SMOOTH_FREQ = 0.8f;
constexpr float MAX_COEF_FREQ = 1.2f;
constexpr uint8_t SMOOTH_STEP = 20;
constexpr unsigned long FRAME_INTERVAL_MS = 5UL;

constexpr size_t RX_BUFFER_SIZE = 64;
}

enum BandIndex : uint8_t {
  BAND_LOW = 0,
  BAND_MID = 1,
  BAND_HIGH = 2,
  BAND_COUNT = 3
};

enum class Submode : uint8_t {
  THREE,
  LOW_ONLY,
  MID_ONLY,
  HIGH_ONLY
};

struct Bands {
  uint8_t low = 0;
  uint8_t mid = 0;
  uint8_t high = 0;
  uint8_t peak = 0;
};

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t backgroundBrightness = ArduConfig::DEFAULT_BACKGROUND;
  Submode submode = Submode::THREE;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint8_t spectrumLowPass = 40;
int16_t micDcOffset = 250;
bool calibrated = false;

Bands lastBands;
float bandFiltered[BAND_COUNT] = {0.0f, 0.0f, 0.0f};
float bandAverage[BAND_COUNT] = {0.0f, 0.0f, 0.0f};
uint8_t bandBrightness[BAND_COUNT] = {
    ArduConfig::DEFAULT_BACKGROUND,
    ArduConfig::DEFAULT_BACKGROUND,
    ArduConfig::DEFAULT_BACKGROUND
};
bool bandFlash[BAND_COUNT] = {false, false, false};

unsigned long lastFrameMs = 0;

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

Bands readBands(bool applyNoiseGate) {
  analyzeAudio();

  Bands bands;

  for (uint8_t i = 2; i < 32; ++i) {
    uint8_t value = fht_log_out[i];

    if (applyNoiseGate && value < spectrumLowPass) {
      value = 0;
    }

    if (value > bands.peak) {
      bands.peak = value;
    }

    if (i >= ArduConfig::LOW_BIN_FIRST && i <= ArduConfig::LOW_BIN_LAST) {
      if (value > bands.low) bands.low = value;
    } else if (i >= ArduConfig::MID_BIN_FIRST && i <= ArduConfig::MID_BIN_LAST) {
      if (value > bands.mid) bands.mid = value;
    } else if (i >= ArduConfig::HIGH_BIN_FIRST && i <= ArduConfig::HIGH_BIN_LAST) {
      if (value > bands.high) bands.high = value;
    }
  }

  return bands;
}

void resetFrequencyState() {
  lastBands = Bands();

  for (uint8_t i = 0; i < BAND_COUNT; ++i) {
    bandFiltered[i] = 0.0f;
    bandAverage[i] = 0.0f;
    bandBrightness[i] = settings.backgroundBrightness;
    bandFlash[i] = false;
  }
}

void calibrateSpectrumNoise() {
  clearRing();

  // ColorMusic principle: maximum quiet spectrum across 100 frames + 3.
  // ARDU additionally recalibrates MAX9814 DC because A0 carries ~1.25 V bias.
  delay(500);
  calibrateDcOffset();

  if (!micDcValid()) {
    calibrated = false;
    spectrumLowPass = 40;
    resetFrequencyState();

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

    for (uint8_t bin = 2; bin < 32; ++bin) {
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
  resetFrequencyState();

  Serial.print(F("CALF DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.println(spectrumLowPass);
}

uint8_t valueForBand(const Bands& bands, BandIndex band) {
  if (band == BAND_LOW) return bands.low;
  if (band == BAND_MID) return bands.mid;
  return bands.high;
}

void updateFrequencyState(const Bands& bands) {
  for (uint8_t i = 0; i < BAND_COUNT; ++i) {
    const uint8_t value =
        valueForBand(bands, static_cast<BandIndex>(i));

    bandAverage[i] =
        value * ArduConfig::AVER_K +
        bandAverage[i] * (1.0f - ArduConfig::AVER_K);

    bandFiltered[i] =
        value * ArduConfig::SMOOTH_FREQ +
        bandFiltered[i] * (1.0f - ArduConfig::SMOOTH_FREQ);

    if (value > 0 &&
        bandFiltered[i] > bandAverage[i] * ArduConfig::MAX_COEF_FREQ) {
      bandBrightness[i] = 255;
      bandFlash[i] = true;
    } else {
      bandFlash[i] = false;
    }

    if (bandBrightness[i] >= ArduConfig::SMOOTH_STEP) {
      bandBrightness[i] -= ArduConfig::SMOOTH_STEP;
    } else {
      bandBrightness[i] = 0;
    }

    if (bandBrightness[i] < settings.backgroundBrightness) {
      bandBrightness[i] = settings.backgroundBrightness;
    }
  }
}

const __FlashStringHelper* submodeName() {
  switch (settings.submode) {
    case Submode::THREE: return F("THREE");
    case Submode::LOW_ONLY:  return F("LOW");
    case Submode::MID_ONLY:  return F("MID");
    case Submode::HIGH_ONLY: return F("HIGH");
  }
  return F("UNKNOWN");
}

uint8_t hueForBand(BandIndex band) {
  if (band == BAND_LOW) return HUE_RED;
  if (band == BAND_MID) return HUE_GREEN;
  return HUE_YELLOW;
}

void showBackground() {
  // Original ColorMusic uses EMPTY_COLOR=HUE_PURPLE for silence.
  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      CHSV(HUE_PURPLE, 255, settings.backgroundBrightness)
  );
}

void showBand(BandIndex band) {
  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      CHSV(hueForBand(band), 255, bandBrightness[band])
  );
}

void renderM05() {
  if (!settings.power) {
    return;
  }

  FastLED.setBrightness(settings.brightness);

  bool active = false;

  switch (settings.submode) {
    case Submode::THREE:
      // Original ColorMusic mode 5.1 priority: HIGH -> MID -> LOW.
      if (bandFlash[BAND_HIGH]) {
        showBand(BAND_HIGH);
        active = true;
      } else if (bandFlash[BAND_MID]) {
        showBand(BAND_MID);
        active = true;
      } else if (bandFlash[BAND_LOW]) {
        showBand(BAND_LOW);
        active = true;
      }
      break;

    case Submode::LOW_ONLY:
      if (bandFlash[BAND_LOW]) {
        showBand(BAND_LOW);
        active = true;
      }
      break;

    case Submode::MID_ONLY:
      if (bandFlash[BAND_MID]) {
        showBand(BAND_MID);
        active = true;
      }
      break;

    case Submode::HIGH_ONLY:
      if (bandFlash[BAND_HIGH]) {
        showBand(BAND_HIGH);
        active = true;
      }
      break;
  }

  if (!active) {
    showBackground();
  }

  FastLED.show();
}

void updateM05() {
  const unsigned long now = millis();
  if (now - lastFrameMs < ArduConfig::FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  if (!settings.power) {
    return;
  }

  if (calibrated) {
    lastBands = readBands(true);
    updateFrequencyState(lastBands);
  }

  renderM05();
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

void printBands() {
  Serial.print(F("BANDS LOW="));
  Serial.print(lastBands.low);
  Serial.print(F(" MID="));
  Serial.print(lastBands.mid);
  Serial.print(F(" HIGH="));
  Serial.print(lastBands.high);
  Serial.print(F(" LP="));
  Serial.print(spectrumLowPass);

  Serial.print(F(" BR="));
  Serial.print(bandBrightness[BAND_LOW]);
  Serial.print(',');
  Serial.print(bandBrightness[BAND_MID]);
  Serial.print(',');
  Serial.print(bandBrightness[BAND_HIGH]);

  Serial.print(F(" FLASH="));
  Serial.print(bandFlash[BAND_LOW] ? 1 : 0);
  Serial.print(',');
  Serial.print(bandFlash[BAND_MID] ? 1 : 0);
  Serial.print(',');
  Serial.println(bandFlash[BAND_HIGH] ? 1 : 0);
}

void printStatus() {
  Serial.print(F("STATUS FW=M05 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=M05 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" BACKGROUND="));
  Serial.print(settings.backgroundBrightness);
  Serial.print(F(" SUBMODE="));
  Serial.print(submodeName());
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
  Serial.print(F(" FHT_N="));
  Serial.print(FHT_N);
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

  if (strcmp(command, "BANDS") == 0) {
    printBands();
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

  if (strncmp(command, "SUBMODE ", 8) == 0) {
    const char* value = command + 8;

    if (strcmp(value, "THREE") == 0) {
      settings.submode = Submode::THREE;
    } else if (strcmp(value, "LOW") == 0) {
      settings.submode = Submode::LOW_ONLY;
    } else if (strcmp(value, "MID") == 0) {
      settings.submode = Submode::MID_ONLY;
    } else if (strcmp(value, "HIGH") == 0) {
      settings.submode = Submode::HIGH_ONLY;
    } else {
      Serial.println(F("ERR BAD_SUBMODE"));
      return;
    }

    resetFrequencyState();
    clearRing();
    Serial.print(F("OK SUBMODE="));
    Serial.println(submodeName());
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
    for (uint8_t i = 0; i < BAND_COUNT; ++i) {
      if (bandBrightness[i] < settings.backgroundBrightness) {
        bandBrightness[i] = settings.backgroundBrightness;
      }
    }

    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC CALF BANDS ON OFF SUBMODE BRIGHT BACKGROUND HELP"));
    Serial.println(F("FORMAT SUBMODE <THREE|LOW|MID|HIGH> | BRIGHT <0..255> | BACKGROUND <0..255>"));
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
  FastLED.setBrightness(settings.brightness);
  FastLED.clear(true);

  analogReference(DEFAULT);
  delay(500);
  calibrateDcOffset();
  resetFrequencyState();

  Serial.println(F("ARDU NANO M05 R3 READY"));
  Serial.println(F("RUN CALF IN QUIET ROOM BEFORE MUSIC TEST"));
  printStatus();
}

void loop() {
  pollSerial();
  updateM05();
}
