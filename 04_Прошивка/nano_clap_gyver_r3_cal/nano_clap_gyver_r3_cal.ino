/*
  ARDU CLAP GYVER R3 CAL
  Gyver-style double-clap detector with app-ready family calibration.

  Goals:
  - keep AlexGyver derivative/state-machine recognition principle;
  - calibrate one shared threshold from several users with different clap strength;
  - avoid calibrating from one adult;
  - persist calibrated threshold in Nano EEPROM;
  - expose commands that ESP/app can map later.

  Calibration flow:
    CLAPCAL START <pairs>  // 3..12, measures quiet derivative percentile
    CLAPCAL SAMPLE         // user performs one double-clap after SAMPLE_GO
    ... repeat for all users/pairs ...
    CLAPCAL FINISH         // calculate + apply threshold in RAM
    CLAPCAL SAVE           // persist threshold/timeout
*/

#include <Arduino.h>
#include <EEPROM.h>

namespace ArduPins {
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 3;

constexpr uint16_t MIC_DC_MIN = 120;
constexpr uint16_t MIC_DC_MAX = 400;

constexpr uint16_t VOL_DT_US = 700;
constexpr uint16_t VOL_PERIOD_MS = 5;
constexpr uint8_t VOL_WINDOW = 20;
constexpr uint16_t AMPLI_DT_MS = 150;

constexpr int DEFAULT_CLAP_TRSH = 70;
constexpr uint16_t DEFAULT_CLAP_TIMEOUT_MS = 500;

constexpr unsigned long STARTUP_SETTLE_MS = 1200UL;
constexpr unsigned long COMMAND_IGNORE_MS = 700UL;

constexpr unsigned long CAL_PRE_DELAY_MS = 1000UL;
constexpr unsigned long QUIET_WINDOW_MS = 3000UL;
constexpr unsigned long SAMPLE_WINDOW_MS = 2200UL;

constexpr uint8_t CAL_DER_FLOOR = 15;
constexpr uint8_t CAL_MIN_PAIRS = 3;
constexpr uint8_t CAL_MAX_PAIRS = 12;
constexpr uint8_t MAX_CAL_CLAPS = CAL_MAX_PAIRS * 2;

constexpr uint16_t EEPROM_MAGIC = 0x4343;
constexpr uint8_t EEPROM_VERSION = 1;
constexpr int EEPROM_BASE = 192;

constexpr size_t RX_BUFFER_SIZE = 96;
}

struct PersistedClap {
  uint16_t magic;
  uint8_t version;
  uint16_t threshold;
  uint16_t timeoutMs;
  uint8_t checksum;
};

uint8_t checksumConfig(const PersistedClap& data) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&data);
  uint8_t sum = 0xA7;

  for (size_t i = 0; i < sizeof(PersistedClap) - 1; ++i) {
    sum = static_cast<uint8_t>((sum << 1) | (sum >> 7));
    sum ^= p[i];
  }

  return sum;
}

bool validConfig(const PersistedClap& data) {
  if (data.magic != ArduConfig::EEPROM_MAGIC) return false;
  if (data.version != ArduConfig::EEPROM_VERSION) return false;
  if (data.threshold < 20 || data.threshold > 300) return false;
  if (data.timeoutMs < 250 || data.timeoutMs > 1200) return false;
  return data.checksum == checksumConfig(data);
}

class GyverRawEnvelope {
public:
  explicit GyverRawEnvelope(uint8_t pin) : _pin(pin) {}

  void begin() {
    pinMode(_pin, INPUT);
    reset();
  }

  void reset() {
    _windowMax = 0;
    _raw = 0;
    _rawMax = 0;
    _maxs = 0;
    _count = 0;
    _tmrSample = micros();
    _tmrPeriod = millis();
    _tmrAmpli = millis();
  }

  bool tick() {
    const unsigned long nowMs = millis();

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

  uint16_t getRaw() const { return _raw; }
  uint16_t getRawMax() const { return _rawMax; }

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

class GyverClapDetector {
public:
  void setTrsh(int trsh) { _trsh = trsh; }
  void setTimeout(uint16_t tout) { _tout = tout; }

  void reset() {
    _tmr = millis();
    _tmr2 = millis();
    _prevVal = 0;
    _primed = false;
    _prevSignal = 0;
    _state = 0;
    _claps = 0;
    _ready = false;
    _clap = false;
    _startClap = false;
    _lastDerivative = 0;
    _lastFront = 0;
    _lastStrength = 0;
    _posPeak = 0;
    _negPeak = 0;
  }

  void tick(int val) {
    if (millis() - _tmr < 10) return;
    _tmr = millis();

    if (!_primed) {
      _prevVal = val;
      _primed = true;
      return;
    }

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

    if (_state == 1 && der > _posPeak) _posPeak = der;
    if (_state == 2 && -der > _negPeak) _negPeak = -der;

    if (front == 1 && _state == 0) {
      _state = 1;
      _posPeak = der;
      _negPeak = 0;

      if (!_startClap) {
        _claps = 0;
        _ready = false;
      }

      _startClap = true;
      _clap = false;
      _tmr2 = millis();

    } else if (front == -1 && _state == 1 && deb <= 200) {
      _state = 2;
      _negPeak = -der;
      _tmr2 = millis();

    } else if (front == 0 && _state == 2 && deb <= 200) {
      _state = 0;
      ++_claps;
      _clap = true;

      const int weaker = _posPeak < _negPeak ? _posPeak : _negPeak;
      _lastStrength = weaker > 0 ? static_cast<uint16_t>(weaker) : 0;

      _tmr2 = millis();

    } else if (_state != 0 && deb > 200) {
      _state = 0;
      _posPeak = 0;
      _negPeak = 0;

    } else if (_startClap && deb > _tout) {
      _state = 0;
      _startClap = false;

      if (_claps != 0) _ready = true;
    }
  }

  bool takeClap(uint16_t& strength) {
    if (!_clap) return false;
    _clap = false;
    strength = _lastStrength;
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
  bool _primed = false;
  int _lastDerivative = 0;
  int _lastFront = 0;
  int _posPeak = 0;
  int _negPeak = 0;
  uint16_t _lastStrength = 0;
};

GyverRawEnvelope vol(ArduPins::MIC_IN);
GyverClapDetector clap;
GyverClapDetector calShape;

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t micDc = 250;
bool configSaved = false;
bool configDirty = false;

uint32_t singleClaps = 0;
uint32_t sequences = 0;
uint32_t doubleClaps = 0;

unsigned long ignoreUntilMs = 0;
bool readyAnnounced = false;

bool calActive = false;
bool calFinished = false;
uint8_t calTargetPairs = 0;
uint8_t calGoodPairs = 0;
uint8_t calClapCount = 0;
uint16_t calStrengths[ArduConfig::MAX_CAL_CLAPS];
uint16_t quietP99Der = 0;
uint16_t suggestedThreshold = 0;

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

void loadConfig() {
  PersistedClap data;
  EEPROM.get(ArduConfig::EEPROM_BASE, data);

  if (!validConfig(data)) {
    clap.setTrsh(ArduConfig::DEFAULT_CLAP_TRSH);
    clap.setTimeout(ArduConfig::DEFAULT_CLAP_TIMEOUT_MS);
    configSaved = false;
    configDirty = false;
    return;
  }

  clap.setTrsh(data.threshold);
  clap.setTimeout(data.timeoutMs);
  configSaved = true;
  configDirty = false;
}

void saveConfig() {
  PersistedClap data;
  data.magic = ArduConfig::EEPROM_MAGIC;
  data.version = ArduConfig::EEPROM_VERSION;
  data.threshold = static_cast<uint16_t>(clap.threshold());
  data.timeoutMs = clap.timeoutMs();
  data.checksum = 0;
  data.checksum = checksumConfig(data);

  EEPROM.put(ArduConfig::EEPROM_BASE, data);

  configSaved = true;
  configDirty = false;
}

void beginIgnoreWindow() {
  ignoreUntilMs = millis() + ArduConfig::COMMAND_IGNORE_MS;
  readyAnnounced = false;
  clap.reset();
}

bool detectorReady() {
  return static_cast<long>(millis() - ignoreUntilMs) >= 0;
}

void clearStats() {
  singleClaps = 0;
  sequences = 0;
  doubleClaps = 0;
  beginIgnoreWindow();

  Serial.print(F("OK CLEARED IGNORE_MS="));
  Serial.println(ArduConfig::COMMAND_IGNORE_MS);
}

void processNormalDetector() {
  const bool sampleReady = vol.tick();
  (void)sampleReady;

  if (calActive) return;
  if (!detectorReady()) return;

  if (!readyAnnounced) {
    readyAnnounced = true;
    clap.reset();
    Serial.println(F("EVENT CLAP_READY"));
    return;
  }

  clap.tick(vol.getRawMax());

  uint16_t strength = 0;
  if (clap.takeClap(strength)) {
    ++singleClaps;

    Serial.print(F("EVENT CLAP MS="));
    Serial.print(millis());
    Serial.print(F(" STRENGTH="));
    Serial.print(strength);
    Serial.print(F(" RAWMAX="));
    Serial.println(vol.getRawMax());
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

void sortAscending(uint16_t* values, uint8_t count) {
  for (uint8_t i = 1; i < count; ++i) {
    const uint16_t key = values[i];
    int8_t j = static_cast<int8_t>(i) - 1;

    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      --j;
    }

    values[j + 1] = key;
  }
}

uint16_t measureQuietP99Derivative() {
  uint16_t hist[64];
  for (uint8_t i = 0; i < 64; ++i) hist[i] = 0;

  Serial.print(F("EVENT CLAPCAL QUIET_ARMING DELAY_MS="));
  Serial.println(ArduConfig::CAL_PRE_DELAY_MS);
  delay(ArduConfig::CAL_PRE_DELAY_MS);

  vol.reset();

  uint16_t prev = 0;
  bool primed = false;
  uint32_t derivativeSamples = 0;

  Serial.println(F("EVENT CLAPCAL QUIET_GO"));

  const unsigned long start = millis();

  while (millis() - start < ArduConfig::QUIET_WINDOW_MS) {
    vol.tick();

    static unsigned long lastDerMs = 0;
    if (millis() - lastDerMs < 10) continue;
    lastDerMs = millis();

    const uint16_t current = vol.getRawMax();

    if (!primed) {
      prev = current;
      primed = true;
      continue;
    }

    int der = static_cast<int>(current) - static_cast<int>(prev);
    prev = current;

    uint16_t absDer = der < 0 ? static_cast<uint16_t>(-der)
                              : static_cast<uint16_t>(der);

    uint8_t bin = absDer >= 252 ? 63 : static_cast<uint8_t>(absDer / 4);
    if (hist[bin] < 65535U) ++hist[bin];
    ++derivativeSamples;
  }

  if (derivativeSamples == 0) return 0;

  const uint32_t target = (derivativeSamples * 99UL + 99UL) / 100UL;
  uint32_t cumulative = 0;

  for (uint8_t bin = 0; bin < 64; ++bin) {
    cumulative += hist[bin];

    if (cumulative >= target) {
      return static_cast<uint16_t>(bin * 4U + 3U);
    }
  }

  return 255;
}

void resetCalibration() {
  calActive = false;
  calFinished = false;
  calTargetPairs = 0;
  calGoodPairs = 0;
  calClapCount = 0;
  quietP99Der = 0;
  suggestedThreshold = 0;

  for (uint8_t i = 0; i < ArduConfig::MAX_CAL_CLAPS; ++i) {
    calStrengths[i] = 0;
  }
}

void startCalibration(uint8_t targetPairs) {
  resetCalibration();
  calActive = true;
  calTargetPairs = targetPairs;

  quietP99Der = measureQuietP99Derivative();

  Serial.print(F("EVENT CLAPCAL QUIET_DONE P99_DER="));
  Serial.print(quietP99Der);
  Serial.print(F(" TARGET_PAIRS="));
  Serial.println(calTargetPairs);

  Serial.println(F("EVENT CLAPCAL WAIT_SAMPLE"));
}

void captureCalibrationSample() {
  if (!calActive) {
    Serial.println(F("ERR CLAPCAL_NOT_ACTIVE"));
    return;
  }

  if (calGoodPairs >= calTargetPairs) {
    Serial.println(F("ERR CLAPCAL_TARGET_REACHED"));
    return;
  }

  Serial.print(F("EVENT CLAPCAL SAMPLE_ARMING DELAY_MS="));
  Serial.print(ArduConfig::CAL_PRE_DELAY_MS);
  Serial.print(F(" PAIR_INDEX="));
  Serial.println(calGoodPairs + 1);

  delay(ArduConfig::CAL_PRE_DELAY_MS);

  vol.reset();
  calShape.setTrsh(ArduConfig::CAL_DER_FLOOR);
  calShape.setTimeout(1000);
  calShape.reset();

  uint16_t strengths[8];
  uint8_t count = 0;
  for (uint8_t i = 0; i < 8; ++i) strengths[i] = 0;

  Serial.println(F("EVENT CLAPCAL SAMPLE_GO"));

  const unsigned long start = millis();

  while (millis() - start < ArduConfig::SAMPLE_WINDOW_MS) {
    vol.tick();
    calShape.tick(vol.getRawMax());

    uint16_t strength = 0;

    if (calShape.takeClap(strength)) {
      if (strength >= ArduConfig::CAL_DER_FLOOR && count < 8) {
        strengths[count++] = strength;
      }
    }

    uint8_t ignored = 0;
    (void)calShape.takeSequence(ignored);
  }

  if (count < 2) {
    Serial.print(F("EVENT CLAPCAL SAMPLE_FAIL CLAPS="));
    Serial.println(count);
    Serial.println(F("EVENT CLAPCAL WAIT_SAMPLE"));
    return;
  }

  sortAscending(strengths, count);

  const uint16_t weak = strengths[count - 2];
  const uint16_t strong = strengths[count - 1];

  if (calClapCount + 2 <= ArduConfig::MAX_CAL_CLAPS) {
    calStrengths[calClapCount++] = weak;
    calStrengths[calClapCount++] = strong;
  }

  ++calGoodPairs;

  Serial.print(F("EVENT CLAPCAL SAMPLE_OK PAIR="));
  Serial.print(calGoodPairs);
  Serial.print('/');
  Serial.print(calTargetPairs);
  Serial.print(F(" WEAK="));
  Serial.print(weak);
  Serial.print(F(" STRONG="));
  Serial.print(strong);
  Serial.print(F(" RAW_CANDIDATES="));
  Serial.println(count);

  if (calGoodPairs < calTargetPairs) {
    Serial.println(F("EVENT CLAPCAL WAIT_SAMPLE"));
  } else {
    Serial.println(F("EVENT CLAPCAL READY_TO_FINISH"));
  }
}

void finishCalibration() {
  if (!calActive) {
    Serial.println(F("ERR CLAPCAL_NOT_ACTIVE"));
    return;
  }

  if (calGoodPairs < calTargetPairs || calClapCount < 6) {
    Serial.print(F("ERR CLAPCAL_NEED_PAIRS HAVE="));
    Serial.print(calGoodPairs);
    Serial.print(F(" NEED="));
    Serial.println(calTargetPairs);
    return;
  }

  uint16_t sorted[ArduConfig::MAX_CAL_CLAPS];

  for (uint8_t i = 0; i < calClapCount; ++i) {
    sorted[i] = calStrengths[i];
  }

  sortAscending(sorted, calClapCount);

  const uint8_t p20Index =
      static_cast<uint8_t>((static_cast<uint16_t>(calClapCount - 1) * 20U) / 100U);

  const uint16_t clapP20 = sorted[p20Index];

  uint16_t fromClap =
      static_cast<uint16_t>((static_cast<uint32_t>(clapP20) * 70UL) / 100UL);

  uint16_t fromNoise = quietP99Der + 10U;

  uint16_t candidate = fromClap > fromNoise ? fromClap : fromNoise;

  if (candidate < 35U) candidate = 35U;
  if (candidate > 150U) candidate = 150U;

  if (fromNoise >= clapP20 || candidate >= clapP20) {
    Serial.print(F("ERR CLAPCAL_LOW_SEPARATION QUIET_P99="));
    Serial.print(quietP99Der);
    Serial.print(F(" CLAP_P20="));
    Serial.println(clapP20);
    return;
  }

  suggestedThreshold = candidate;
  clap.setTrsh(suggestedThreshold);
  configDirty = true;
  calFinished = true;
  calActive = false;

  Serial.print(F("EVENT CLAPCAL RESULT QUIET_P99="));
  Serial.print(quietP99Der);
  Serial.print(F(" CLAP_P20="));
  Serial.print(clapP20);
  Serial.print(F(" THRESH="));
  Serial.print(suggestedThreshold);
  Serial.print(F(" CLAPS="));
  Serial.print(calClapCount);
  Serial.print(F(" PAIRS="));
  Serial.println(calGoodPairs);

  beginIgnoreWindow();
}

void printCalStatus() {
  Serial.print(F("CLAPCAL STATUS ACTIVE="));
  Serial.print(calActive ? F("YES") : F("NO"));
  Serial.print(F(" FINISHED="));
  Serial.print(calFinished ? F("YES") : F("NO"));
  Serial.print(F(" TARGET_PAIRS="));
  Serial.print(calTargetPairs);
  Serial.print(F(" GOOD_PAIRS="));
  Serial.print(calGoodPairs);
  Serial.print(F(" CLAPS="));
  Serial.print(calClapCount);
  Serial.print(F(" QUIET_P99="));
  Serial.print(quietP99Der);
  Serial.print(F(" SUGGESTED="));
  Serial.println(suggestedThreshold);
}

void printStatus() {
  Serial.print(F("STATUS FW=CLAP_GYVER REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MIC=MAX9814 MIC_PIN=A0 MIC_GAIN=40DB AR=FLOAT ADC_REF=DEFAULT"));
  Serial.print(F(" DC="));
  Serial.print(micDc);
  Serial.print(F(" DC_OK="));
  Serial.print(dcValid() ? F("YES") : F("NO"));
  Serial.print(F(" CLAP_TRSH="));
  Serial.print(clap.threshold());
  Serial.print(F(" CLAP_TIMEOUT_MS="));
  Serial.print(clap.timeoutMs());
  Serial.print(F(" SAVED="));
  Serial.print(configSaved ? F("YES") : F("NO"));
  Serial.print(F(" DIRTY="));
  Serial.print(configDirty ? F("YES") : F("NO"));
  Serial.print(F(" CAL_ACTIVE="));
  Serial.print(calActive ? F("YES") : F("NO"));
  Serial.print(F(" TARGET=DOUBLE"));
  Serial.print(F(" SINGLE_EVENTS="));
  Serial.print(singleClaps);
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

bool parseLongRange(
    const char* text,
    long minValue,
    long maxValue,
    long& value) {
  if (text == nullptr || *text == '\0') return false;

  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);

  if (*end != '\0' || parsed < minValue || parsed > maxValue) return false;

  value = parsed;
  return true;
}

void handleClapCal(char* args) {
  if (strncmp(args, "START ", 6) == 0) {
    long pairs = 0;

    if (!parseLongRange(
            args + 6,
            ArduConfig::CAL_MIN_PAIRS,
            ArduConfig::CAL_MAX_PAIRS,
            pairs)) {
      Serial.println(F("ERR CLAPCAL_BAD_PAIRS RANGE=3..12"));
      return;
    }

    startCalibration(static_cast<uint8_t>(pairs));
    return;
  }

  if (strcmp(args, "SAMPLE") == 0) {
    captureCalibrationSample();
    return;
  }

  if (strcmp(args, "FINISH") == 0) {
    finishCalibration();
    return;
  }

  if (strcmp(args, "SAVE") == 0) {
    if (!calFinished && !configDirty) {
      Serial.println(F("ERR CLAPCAL_NOTHING_TO_SAVE"));
      return;
    }

    saveConfig();
    Serial.println(F("OK CLAPCAL_SAVED"));
    return;
  }

  if (strcmp(args, "STATUS") == 0) {
    printCalStatus();
    return;
  }

  if (strcmp(args, "CANCEL") == 0) {
    resetCalibration();
    loadConfig();
    beginIgnoreWindow();
    Serial.println(F("OK CLAPCAL_CANCELLED"));
    return;
  }

  Serial.println(F("ERR CLAPCAL_COMMAND"));
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

    if (!parseLongRange(command + 5, 20, 300, value)) {
      Serial.println(F("ERR BAD_TRSH RANGE=20..300"));
      return;
    }

    clap.setTrsh(static_cast<int>(value));
    configDirty = true;
    beginIgnoreWindow();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "TIMEOUT ", 8) == 0) {
    long value = 0;

    if (!parseLongRange(command + 8, 250, 1200, value)) {
      Serial.println(F("ERR BAD_TIMEOUT RANGE=250..1200"));
      return;
    }

    clap.setTimeout(static_cast<uint16_t>(value));
    configDirty = true;
    beginIgnoreWindow();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "CLAPCAL ", 8) == 0) {
    handleClapCal(command + 8);
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS STATUS ADC STATS CLEAR TRSH TIMEOUT CLAPCAL HELP"));
    Serial.println(F("CLAPCAL START <3..12>|SAMPLE|FINISH|SAVE|STATUS|CANCEL"));
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
  analogReference(DEFAULT);

  pinMode(ArduPins::RING_A, OUTPUT);
  pinMode(ArduPins::RING_B, OUTPUT);
  digitalWrite(ArduPins::RING_A, LOW);
  digitalWrite(ArduPins::RING_B, LOW);

  delay(ArduConfig::STARTUP_SETTLE_MS);

  for (uint8_t i = 0; i < 16; ++i) {
    (void)analogRead(ArduPins::MIC_IN);
  }

  micDc = measureDc();

  vol.begin();
  loadConfig();
  clap.reset();
  beginIgnoreWindow();

  Serial.println(F("ARDU NANO CLAP GYVER R3 CAL READY"));
  Serial.println(F("APP-READY FAMILY CALIBRATION; TARGET DOUBLE CLAP"));
  printStatus();
}

void loop() {
  pollSerial();
  processNormalDetector();
}
