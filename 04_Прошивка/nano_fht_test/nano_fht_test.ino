#include <Arduino.h>

#define FHT_SAMPLES_N 64
#include <AvrFHT.h>
#include <fht_window.h>
#include <fht_reorder.h>
#include <fht_mag_log.h>

#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
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
int16_t micDcOffset = 250;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

struct Bands {
  uint8_t low = 0;
  uint8_t mid = 0;
  uint8_t high = 0;
  uint8_t peak = 0;
};

void setFastAdcPrescaler() {
  // Как в ColorMusic v2.10: ADC prescaler = 32.
  // 16 MHz / 32 = 500 kHz ADC clock.
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);
}

void analyzeAudio() {
  for (uint8_t i = 0; i < FHT_SAMPLES_N; ++i) {
    fht_input[i] =
        static_cast<int16_t>(analogRead(ArduPins::MIC_IN)) - micDcOffset;
  }

  fht_window();
  fht_reorder();
  fht_transform();
  fht_mag_log();
}

void calibrateDcOffset() {
  uint32_t sum = 0;

  for (uint16_t i = 0; i < 256; ++i) {
    sum += analogRead(ArduPins::MIC_IN);
  }

  micDcOffset = static_cast<int16_t>(sum / 256UL);
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
  delay(100);

  calibrateDcOffset();

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

  spectrumLowPass = proposed > 255 ? 255 : static_cast<uint8_t>(proposed);

  Serial.print(F("CALF DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_MAX="));
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
  Serial.print(F("STATUS FW=FHTTEST MIC_GAIN=40DB SPECTR_LOW_PASS="));
  Serial.print(spectrumLowPass);
  Serial.print(F(" LIB=AvrFHT FHT_N="));
  Serial.print(FHT_SAMPLES_N);
  Serial.print(F(" DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" ADC_REF=DEFAULT UPTIME_MS="));
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
    Serial.println(F("CMDS PING STATUS CALF FREQ FREQRAW HELP"));
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

  // В ColorMusic использована старая ArduinoFHT и другая входная схема.
  // В ARDU применяем AvrFHT и вычитаем DC offset MAX9814 программно.
  // Опору ADC оставляем DEFAULT (~5 V), потому что физический OUT MAX9814
  // имеет DC offset около 1.25 V.
  analogReference(DEFAULT);
  setFastAdcPrescaler();

  Serial.println(F("ARDU NANO FHT TEST READY"));
  Serial.println(F("RUN CALF IN QUIET ROOM"));
  printStatus();
}

void loop() {
  pollSerial();
}
