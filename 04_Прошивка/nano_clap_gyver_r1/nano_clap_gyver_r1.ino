/*
  ARDU CLAP GYVER R1
  Diagnostic based on AlexGyver GyverLamp2 bundled VolAnalyzer + Clap logic.

  Reference behavior from GyverLamp2:
  - vol.setDt(700)
  - vol.setPeriod(5)
  - vol.setWindow(20) for MAX_LEDS=300
  - clap.setTrsh(250)
  - clap.setTimeout(500)
  - clap.tick(vol.getRawMax())
  - action only on clap.hasClaps(2)

  This ARDU sketch:
  - MAX9814 Out -> A0, Gain -> Vdd (40 dB), AR floating
  - does NOT control LEDs yet
  - prints detected individual claps and completed clap sequences
  - default target gesture is exactly 2 claps
*/

#include <Arduino.h>

namespace ArduPins {
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 1;

constexpr uint16_t MIC_DC_MIN = 120;
constexpr uint16_t MIC_DC_MAX = 400;

constexpr uint16_t VOL_DT_US = 700;
constexpr uint16_t VOL_PERIOD_MS = 5;
constexpr uint8_t VOL_WINDOW = 20;
constexpr uint16_t AMPLI_DT_MS = 150;

constexpr int DEFAULT_CLAP_TRSH = 250;
constexpr uint16_t DEFAULT_CLAP_TIMEOUT_MS = 500;

constexpr size_t RX_BUFFER_SIZE = 64;
}

// Minimal raw-envelope part of the VolAnalyzer version bundled in GyverLamp2.
// Only getRawMax() behavior needed by Clap is retained.
class GyverRawEnvelope {
public:
  explicit GyverRawEnvelope(uint8_t pin) : _pin(pin) {}

  void begin() {
    pinMode(_pin, INPUT);
    _tmrSample = micros();
    _tmrPeriod = millis();
    _tmrAmpli = millis();
  }

  bool tick() {
    const unsigned long nowMs = millis();

    // Same 150 ms amplitude-frame reset principle as GyverLamp2 VolAnalyzer.
    if (nowMs - _tmrAmpli >= ArduConfig::AMPLI_DT_MS) {
      _tmrAmpli = nowMs;
      _maxs = 0;
    }

    if (nowMs - _tmrPeriod < ArduConfig::VOL_PERIOD_MS) {
      return false;
    }

    const unsigned long nowUs = micros();
    if (nowUs - _tmrSample < ArduConfig::VOL_DT_US) {
      return false;
    }

    _tmrSample = nowUs;

    const uint16_t sample = analogRead(_pin);
    if (sample > _windowMax) _windowMax = sample;

    if (++_count >= ArduConfig::VOL_WINDOW) {
      _tmrPeriod = nowMs;

      _raw = _windowMax;
      if (_windowMax > _maxs) _maxs = _windowMax;
      _rawMax = _maxs;

      _windowMax = 0;
      _count = 0;
      return true;
    }

    return false;
  }

  uint16_t getRaw() const {
    return _raw;
  }

  uint16_t getRawMax() const {
    return _rawMax;
  }

private:
  uint8_t _pin;

  uint16_t _windowMax = 0;
  uint16_t _raw = 0;
  uint16_t _rawMax = 0;
  uint16_t _maxs = 0;
  uint8_t _count = 0;

  unsigned long _tmrSample = 0;
  unsigned long _tmrPeriod = 0;
  unsigned long _tmrAmpli = 0;
};

// State machine adapted from AlexGyver Clap.h.
// The recognition logic is intentionally kept equivalent:
// derivative positive front -> negative front within 200 ms -> return to zero
// -> one clap; sequence closes after timeout.
class GyverClapDetector {
public:
  void setTrsh(int trsh) {
    _trsh = trsh;
  }

  void setTimeout(uint16_t tout) {
    _tout = tout;
  }

  void reset() {
    _tmr = millis();
    _tmr2 = millis();
    _prevVal = 0;
    _prevSignal = 0;
    _state = 0;
    _claps = 0;
    _ready = false;
    _clap = false;
    _startClap = false;
    _lastDerivative = 0;
    _lastFront = 0;
  }

  void tick(int val) {
    if (millis() - _tmr < 10) return;

    _tmr = millis();

    const int der = val - _prevVal;
    _lastDerivative = der;
    _prevVal = val;

    int signal = 0;
    int front = 0;

    if (der > _trsh) signal = 1;
    if (der < -_trsh) signal = -1;

    if (_prevSignal == 0 && signal == 1) front = 1;
    if (_prevSignal == 0 && signal == -1) front = -1;

    _prevSignal = signal;
    _lastFront = front;

    const uint32_t deb = millis() - _tmr2;

    if (front == 1 && _state == 0) {
      _state = 1;

      if (!_startClap) {
        _claps = 0;
        _ready = false;
      }

      _startClap = true;
      _clap = false;
      _tmr2 = millis();

    } else if (front == -1 && _state == 1 && deb <= 200) {
      _state = 2;
      _tmr2 = millis();

    } else if (front == 0 && _state == 2 && deb <= 200) {
      _state = 0;
      ++_claps;
      _clap = true;
      _tmr2 = millis();

    } else if (_startClap && deb > _tout) {
      _state = 0;
      _startClap = false;

      if (_claps != 0) {
        _ready = true;
      }
    }
  }

  bool takeClap() {
    if (!_clap) return false;
    _clap = false;
    return true;
  }

  bool takeSequence(uint8_t& count) {
    if (!_ready) return false;

    _ready = false;
    count = _claps;
    _claps = 0;
    return true;
  }

  int threshold() const { return _trsh; }
  uint16_t timeoutMs() const { return _tout; }
  int lastDerivative() const { return _lastDerivative; }
  int lastFront() const { return _lastFront; }
  uint8_t state() const { return _state; }

private:
  uint32_t _tmr = 0;
  uint32_t _tmr2 = 0;

  int _prevVal = 0;
  int _trsh = ArduConfig::DEFAULT_CLAP_TRSH;

  uint8_t _state = 0;
  int8_t _prevSignal = 0;

  uint16_t _tout = ArduConfig::DEFAULT_CLAP_TIMEOUT_MS;
  uint8_t _claps = 0;

  bool _ready = false;
  bool _clap = false;
  bool _startClap = false;

  int _lastDerivative = 0;
  int _lastFront = 0;
};

GyverRawEnvelope vol(ArduPins::MIC_IN);
GyverClapDetector clap;

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t micDc = 250;
uint32_t singleClaps = 0;
uint32_t sequences = 0;
uint32_t doubleClaps = 0;
bool traceEnabled = false;
unsigned long lastTraceMs = 0;

uint16_t measureDc() {
  uint32_t sum = 0;

  for (uint16_t i = 0; i < 256; ++i) {
    sum += analogRead(ArduPins::MIC_IN);
  }

  return static_cast<uint16_t>(sum / 256UL);
}

bool dcValid() {
  return micDc >= ArduConfig::MIC_DC_MIN &&
         micDc <= ArduConfig::MIC_DC_MAX;
}

void printAdc() {
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
}

void printStatus() {
  Serial.print(F("STATUS FW=CLAP_GYVER REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MIC=MAX9814 MIC_PIN=A0 MIC_GAIN=40DB AR=FLOAT ADC_REF=DEFAULT"));
  Serial.print(F(" DC="));
  Serial.print(micDc);
  Serial.print(F(" DC_OK="));
  Serial.print(dcValid() ? F("YES") : F("NO"));
  Serial.print(F(" VOL_DT_US="));
  Serial.print(ArduConfig::VOL_DT_US);
  Serial.print(F(" VOL_PERIOD_MS="));
  Serial.print(ArduConfig::VOL_PERIOD_MS);
  Serial.print(F(" VOL_WINDOW="));
  Serial.print(ArduConfig::VOL_WINDOW);
  Serial.print(F(" AMPLI_DT_MS="));
  Serial.print(ArduConfig::AMPLI_DT_MS);
  Serial.print(F(" CLAP_TRSH="));
  Serial.print(clap.threshold());
  Serial.print(F(" CLAP_TIMEOUT_MS="));
  Serial.print(clap.timeoutMs());
  Serial.print(F(" TARGET=DOUBLE"));
  Serial.print(F(" SINGLE_EVENTS="));
  Serial.print(singleClaps);
  Serial.print(F(" SEQUENCES="));
  Serial.print(sequences);
  Serial.print(F(" DOUBLE_EVENTS="));
  Serial.print(doubleClaps);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

void printStats() {
  Serial.print(F("STATS SINGLE_EVENTS="));
  Serial.print(singleClaps);
  Serial.print(F(" SEQUENCES="));
  Serial.print(sequences);
  Serial.print(F(" DOUBLE_EVENTS="));
  Serial.println(doubleClaps);
}

void clearStats() {
  singleClaps = 0;
  sequences = 0;
  doubleClaps = 0;
  clap.reset();
  Serial.println(F("OK CLEARED"));
}

bool parseLongRange(
    const char* text,
    long minValue,
    long maxValue,
    long& value) {
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

  if (strcmp(command, "STATS") == 0) {
    printStats();
    return;
  }

  if (strcmp(command, "CLEAR") == 0) {
    clearStats();
    return;
  }

  if (strncmp(command, "TRSH ", 5) == 0) {
    long value = 0;

    if (!parseLongRange(command + 5, 50, 500, value)) {
      Serial.println(F("ERR BAD_TRSH RANGE=50..500"));
      return;
    }

    clap.setTrsh(static_cast<int>(value));
    clap.reset();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "TIMEOUT ", 8) == 0) {
    long value = 0;

    if (!parseLongRange(command + 8, 250, 1500, value)) {
      Serial.println(F("ERR BAD_TIMEOUT RANGE=250..1500"));
      return;
    }

    clap.setTimeout(static_cast<uint16_t>(value));
    clap.reset();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "TRACE ON") == 0) {
    traceEnabled = true;
    Serial.println(F("OK TRACE=ON"));
    return;
  }

  if (strcmp(command, "TRACE OFF") == 0) {
    traceEnabled = false;
    Serial.println(F("OK TRACE=OFF"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC STATS CLEAR TRSH TIMEOUT TRACE HELP"));
    Serial.println(F("DEFAULTS TRSH=250 TIMEOUT=500; TARGET DOUBLE CLAP"));
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

void processGyverClap() {
  const bool sampleReady = vol.tick();

  clap.tick(vol.getRawMax());

  if (clap.takeClap()) {
    ++singleClaps;

    Serial.print(F("EVENT CLAP MS="));
    Serial.print(millis());
    Serial.print(F(" RAW="));
    Serial.print(vol.getRaw());
    Serial.print(F(" RAWMAX="));
    Serial.print(vol.getRawMax());
    Serial.print(F(" DER="));
    Serial.println(clap.lastDerivative());
  }

  uint8_t count = 0;
  if (clap.takeSequence(count)) {
    ++sequences;

    Serial.print(F("EVENT CLAP_SEQUENCE COUNT="));
    Serial.print(count);
    Serial.print(F(" MS="));
    Serial.println(millis());

    if (count == 2) {
      ++doubleClaps;
      Serial.println(F("EVENT DOUBLE_CLAP"));
    }
  }

  if (traceEnabled &&
      sampleReady &&
      millis() - lastTraceMs >= 50) {
    lastTraceMs = millis();

    Serial.print(F("TRACE RAW="));
    Serial.print(vol.getRaw());
    Serial.print(F(" RAWMAX="));
    Serial.print(vol.getRawMax());
    Serial.print(F(" DER="));
    Serial.print(clap.lastDerivative());
    Serial.print(F(" FRONT="));
    Serial.print(clap.lastFront());
    Serial.print(F(" STATE="));
    Serial.println(clap.state());
  }
}

void setup() {
  Serial.begin(ArduConfig::SERIAL_BAUD);
  analogReference(DEFAULT);

  pinMode(ArduPins::RING_A, OUTPUT);
  pinMode(ArduPins::RING_B, OUTPUT);
  digitalWrite(ArduPins::RING_A, LOW);
  digitalWrite(ArduPins::RING_B, LOW);

  delay(200);
  micDc = measureDc();

  vol.begin();
  clap.setTrsh(ArduConfig::DEFAULT_CLAP_TRSH);
  clap.setTimeout(ArduConfig::DEFAULT_CLAP_TIMEOUT_MS);
  clap.reset();

  Serial.println(F("ARDU NANO CLAP GYVER R1 READY"));
  Serial.println(F("GYVERLAMP2 STYLE: VOL RAWMAX DERIVATIVE + DOUBLE CLAP"));
  printStatus();
}

void loop() {
  pollSerial();
  processGyverClap();
}
