#include <Arduino.h>

#define FHT_N 64
#define LOG_OUT 1
#include <FHT.h>

// Legacy OpenMusicLabs/AlexGyver FHT exports a global SCALE macro.
// Modern FastLED also uses SCALE as a C++ identifier.
#ifdef SCALE
#undef SCALE
#endif

#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;

// Late official ColorMusic v2.10 logic:
// SOUND_R      -> A2 (mono / VU)
// SOUND_R_FREQ -> A3 (frequency input through 10 nF from A2)
constexpr uint8_t VU_IN = A2;
constexpr uint8_t FHT_IN = A3;
constexpr uint8_t POT_GND = A0;  // original ColorMusic convenience ground for optional AREF pot
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint16_t LED_COUNT = 43;
constexpr uint16_t MAX_MILLIAMPS = 500;

constexpr uint8_t LOW_BIN_FIRST = 2;
constexpr uint8_t LOW_BIN_LAST = 5;
constexpr uint8_t MID_BIN_FIRST = 6;
constexpr uint8_t MID_BIN_LAST = 10;
constexpr uint8_t HIGH_BIN_FIRST = 11;
constexpr uint8_t HIGH_BIN_LAST = 31;

constexpr uint8_t SPECTR_LOW_PASS_ADD = 3;
constexpr uint8_t CALIBRATION_FRAMES = 100;
constexpr size_t RX_BUFFER_SIZE = 48;
}

CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint8_t spectrumLowPass = 40;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

struct Bands {
  uint8_t low = 0;
  uint8_t mid = 0;
  uint8_t high = 0;
  uint8_t peak = 0;
};

void setDefaultAdcPrescaler() {
  // Arduino Uno/Nano default: ADC prescaler = 128.
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void setFastAdcPrescaler() {
  // ColorMusic v2.10 uses ADC prescaler = 32.
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void discardAdcReads(uint8_t pin) {
  for (uint8_t i = 0; i < 8; ++i) {
    (void)analogRead(pin);
  }
}

void analyzeAudio() {
  // Follow ColorMusic v2.10: FHT sees raw ADC samples from SOUND_R_FREQ.
  // No software DC subtraction here; the hardware coupling removes DC.
  for (uint8_t i = 0; i < FHT_N; ++i) {
    fht_input[i] = analogRead(ArduPins::FHT_IN);
  }

  fht_window();
  fht_reorder();
  fht_run();
  fht_mag_log();
}

void printAdcRaw(uint8_t pin, const __FlashStringHelper* label) {
  setDefaultAdcPrescaler();
  delayMicroseconds(200);
  discardAdcReads(pin);

  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
  uint32_t sum = 0;

  for (uint16_t i = 0; i < 256; ++i) {
    const uint16_t sample = analogRead(pin);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    sum += sample;
  }

  const uint16_t avg = static_cast<uint16_t>(sum / 256UL);

  Serial.print(label);
  Serial.print(F(" AVG="));
  Serial.print(avg);
  Serial.print(F(" MIN="));
  Serial.print(minValue);
  Serial.print(F(" MAX="));
  Serial.println(maxValue);

  setFastAdcPrescaler();
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

void calibrateSpectrumNoise() {
  FastLED.clear(true);

  // Same manual-calibration pause used by late ColorMusic.
  delay(500);
  setFastAdcPrescaler();

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

  Serial.print(F("CALF QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.println(spectrumLowPass);
}

void printFreq() {
  const Bands bands = readBands(true);

  Serial.print(F("FREQ LOW="));
  Serial.print(bands.low);
  Serial.print(F(" MID="));
  Serial.print(bands.mid);
  Serial.print(F(" HIGH="));
  Serial.print(bands.high);
  Serial.print(F(" PEAK="));
  Serial.print(bands.peak);
  Serial.print(F(" LOW_PASS="));
  Serial.println(spectrumLowPass);
}

void printRawFreq() {
  const Bands bands = readBands(false);

  Serial.print(F("FREQRAW LOW="));
  Serial.print(bands.low);
  Serial.print(F(" MID="));
  Serial.print(bands.mid);
  Serial.print(F(" HIGH="));
  Serial.print(bands.high);
  Serial.print(F(" PEAK="));
  Serial.println(bands.peak);
}

void printStatus() {
  Serial.print(F("STATUS FW=FHTV210 MIC_GAIN=40DB VU_PIN=A2 FHT_PIN=A3 A0=POT_GND_LOW A0=POT_GND_LOW"));
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.print(spectrumLowPass);
  Serial.print(F(" LIB=AlexGyver_FHT FHT_N="));
  Serial.print(FHT_N);
  Serial.print(F(" ADC_REF=INTERNAL_1V1 UPTIME_MS="));
  Serial.println(millis());
}

void handleCommand(const char* command) {
  if (strcmp(command, "PING") == 0) {
    Serial.println(F("PONG"));
    return;
  }

  if (strcmp(command, "STATUS") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "ADC2") == 0) {
    printAdcRaw(ArduPins::VU_IN, F("ADC2"));
    return;
  }

  if (strcmp(command, "ADC3") == 0) {
    printAdcRaw(ArduPins::FHT_IN, F("ADC3"));
    return;
  }

  if (strcmp(command, "CALF") == 0) {
    calibrateSpectrumNoise();
    return;
  }

  if (strcmp(command, "FREQ") == 0) {
    printFreq();
    return;
  }

  if (strcmp(command, "FREQRAW") == 0) {
    printRawFreq();
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC2 ADC3 CALF FREQ FREQRAW HELP"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

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

  // ColorMusic v2.10 always drives POT_GND (A0) LOW for the optional
  // AREF potentiometer. With POTENT=0 nothing is connected to A0 externally.
  pinMode(ArduPins::POT_GND, OUTPUT);
  digitalWrite(ArduPins::POT_GND, LOW);

  FastLED.addLeds<WS2812B, ArduPins::RING_A, GRB>(
      leds,
      ArduConfig::LED_COUNT
  );
  FastLED.setMaxPowerInVoltsAndMilliamps(
      5,
      ArduConfig::MAX_MILLIAMPS
  );
  FastLED.clear(true);

  // ColorMusic v2.10 with POTENT=0 uses the ATmega328P internal 1.1 V AREF.
  // This sketch is ONLY for the AC-coupled A2/A3 hardware described in
  // AUDIO_V210_WIRING.md. Do not feed MAX9814 OUT directly into A2/A3.
  analogReference(INTERNAL);
  delay(500);

  setFastAdcPrescaler();

  Serial.println(F("ARDU NANO FHT V210 INPUT TEST READY"));
  Serial.println(F("WIRE A2/A3 AS AUDIO_V210_WIRING.md BEFORE TESTING"));
  printStatus();
}

void loop() {
  pollSerial();
}
