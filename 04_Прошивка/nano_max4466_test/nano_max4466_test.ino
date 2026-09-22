#include <Arduino.h>

#define FHT_N 64
#define LOG_OUT 1
#include <FHT.h>

namespace ArduPins {
constexpr uint8_t VU_IN = A2;
constexpr uint8_t FHT_IN = A3;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
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

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

int16_t fhtDcOffset = 512;
uint8_t spectrumLowPass = 40;

struct Bands {
  uint8_t low = 0;
  uint8_t mid = 0;
  uint8_t high = 0;
  uint8_t peak = 0;
};

void setDefaultAdcPrescaler() {
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void setFastAdcPrescaler() {
  // Same prescaler as ColorMusic v2.10.
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void discardAdcReads(uint8_t pin) {
  for (uint8_t i = 0; i < 8; ++i) {
    (void)analogRead(pin);
  }
}

int16_t measureDcOffset(uint8_t pin) {
  setDefaultAdcPrescaler();
  delayMicroseconds(200);
  discardAdcReads(pin);

  uint32_t sum = 0;
  for (uint16_t i = 0; i < 256; ++i) {
    sum += analogRead(pin);
  }

  setFastAdcPrescaler();
  return static_cast<int16_t>(sum / 256UL);
}

void printAdc(uint8_t pin, const __FlashStringHelper* label, bool fast) {
  if (fast) setFastAdcPrescaler();
  else setDefaultAdcPrescaler();

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

  Serial.print(label);
  Serial.print(F(" AVG="));
  Serial.print(static_cast<uint16_t>(sum / 256UL));
  Serial.print(F(" MIN="));
  Serial.print(minValue);
  Serial.print(F(" MAX="));
  Serial.println(maxValue);

  setFastAdcPrescaler();
}

void analyzeAudio() {
  for (uint8_t i = 0; i < FHT_N; ++i) {
    fht_input[i] =
        static_cast<int16_t>(analogRead(ArduPins::FHT_IN)) - fhtDcOffset;
  }

  fht_window();
  fht_reorder();
  fht_run();
  fht_mag_log();
}

Bands readBands(bool applyNoiseGate) {
  analyzeAudio();

  Bands bands;
  for (uint8_t i = 2; i < 32; ++i) {
    uint8_t value = fht_log_out[i];

    if (applyNoiseGate && value < spectrumLowPass) value = 0;

    if (value > bands.peak) bands.peak = value;

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
  delay(500);
  fhtDcOffset = measureDcOffset(ArduPins::FHT_IN);
  setFastAdcPrescaler();

  uint8_t quietMax = 0;

  for (uint8_t frame = 0; frame < ArduConfig::CALIBRATION_FRAMES; ++frame) {
    analyzeAudio();

    for (uint8_t bin = 2; bin < 32; ++bin) {
      if (fht_log_out[bin] > quietMax) quietMax = fht_log_out[bin];
    }
    delay(4);
  }

  const uint16_t proposed =
      static_cast<uint16_t>(quietMax) + ArduConfig::SPECTR_LOW_PASS_ADD;
  spectrumLowPass = proposed > 255 ? 255 : static_cast<uint8_t>(proposed);

  Serial.print(F("CALF DC="));
  Serial.print(fhtDcOffset);
  Serial.print(F(" QUIET_MAX="));
  Serial.print(quietMax);
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.println(spectrumLowPass);
}

void printFreq(bool gated) {
  const Bands bands = readBands(gated);

  Serial.print(gated ? F("FREQ LOW=") : F("FREQRAW LOW="));
  Serial.print(bands.low);
  Serial.print(F(" MID="));
  Serial.print(bands.mid);
  Serial.print(F(" HIGH="));
  Serial.print(bands.high);
  Serial.print(F(" PEAK="));
  Serial.print(bands.peak);

  if (gated) {
    Serial.print(F(" LOW_PASS="));
    Serial.print(spectrumLowPass);
  }

  Serial.println();
}

void printStatus() {
  Serial.print(F("STATUS FW=MAX4466TEST MIC=MAX4466 VU_PIN=A2 FHT_PIN=A3"));
  Serial.print(F(" ADC_REF=DEFAULT_5V FHT_N="));
  Serial.print(FHT_N);
  Serial.print(F(" DC="));
  Serial.print(fhtDcOffset);
  Serial.print(F(" SPECTR_LOW_PASS="));
  Serial.print(spectrumLowPass);
  Serial.print(F(" UPTIME_MS="));
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
    printAdc(ArduPins::VU_IN, F("ADC2"), false);
    return;
  }
  if (strcmp(command, "ADC3") == 0) {
    printAdc(ArduPins::FHT_IN, F("ADC3"), false);
    return;
  }
  if (strcmp(command, "ADC2FAST") == 0) {
    printAdc(ArduPins::VU_IN, F("ADC2FAST"), true);
    return;
  }
  if (strcmp(command, "ADC3FAST") == 0) {
    printAdc(ArduPins::FHT_IN, F("ADC3FAST"), true);
    return;
  }
  if (strcmp(command, "CALF") == 0) {
    calibrateSpectrumNoise();
    return;
  }
  if (strcmp(command, "FREQRAW") == 0) {
    printFreq(false);
    return;
  }
  if (strcmp(command, "FREQ") == 0) {
    printFreq(true);
    return;
  }
  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC2 ADC3 ADC2FAST ADC3FAST CALF FREQRAW FREQ HELP"));
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

  // MAX4466 comparative stand:
  // VCC=5V, OUT -> ~9.4k -> A2 and OUT -> ~9.4k -> A3.
  // The module provides the DC bias. Use DEFAULT (~5V) AREF.
  analogReference(DEFAULT);
  delay(500);

  fhtDcOffset = measureDcOffset(ArduPins::FHT_IN);
  setFastAdcPrescaler();

  Serial.println(F("ARDU MAX4466 AUDIO TEST READY"));
  printStatus();
}

void loop() {
  pollSerial();
}
