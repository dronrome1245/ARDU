/*
  ARDU CLAP DIAG R1 — MAX9814 impulse diagnostic.

  Purpose:
  - characterize clap-like impulses on the confirmed MAX9814 -> A0 frontend;
  - measure quiet baseline and choose an initial P2P threshold;
  - log impulse timing so single/double-clap behavior can be chosen from hardware data;
  - test false positives from speech, music and household impulse sounds.

  This sketch DOES NOT switch the light yet.
  It intentionally leaves D6/D7 LOW.

  Audio frontend:
  - MAX9814 Out -> A0 directly;
  - Gain -> Vdd (40 dB);
  - AR floating;
  - DEFAULT ADC reference.
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

constexpr uint8_t BLOCK_SAMPLES = 64;
constexpr uint16_t DEFAULT_THRESHOLD_P2P = 180;
constexpr uint16_t MIN_THRESHOLD_P2P = 60;
constexpr uint16_t MAX_THRESHOLD_P2P = 900;
constexpr uint16_t MIC_DC_MIN = 120;
constexpr uint16_t MIC_DC_MAX = 400;

constexpr unsigned long EVENT_LOCKOUT_MS = 120UL;
constexpr unsigned long MONITOR_PERIOD_MS = 500UL;
constexpr uint16_t CAL_BLOCKS = 200;

constexpr size_t RX_BUFFER_SIZE = 80;
}

struct BlockStats {
  uint16_t avg = 0;
  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
  uint16_t p2p = 0;
  uint16_t peakDeviation = 0;
};

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t micDcOffset = 250;
uint16_t thresholdP2P = ArduConfig::DEFAULT_THRESHOLD_P2P;
bool armed = true;
bool monitorEnabled = false;
bool calibrated = false;

BlockStats lastBlock;

uint32_t candidateCount = 0;
uint16_t maxBlockP2P = 0;
uint16_t maxEventP2P = 0;
uint16_t lastEventP2P = 0;
unsigned long lastEventMs = 0;
unsigned long previousEventMs = 0;
unsigned long lastMonitorMs = 0;

bool micDcValid() {
  return micDcOffset >= ArduConfig::MIC_DC_MIN &&
         micDcOffset <= ArduConfig::MIC_DC_MAX;
}

BlockStats readBlock() {
  BlockStats s;
  uint32_t sum = 0;

  for (uint8_t i = 0; i < ArduConfig::BLOCK_SAMPLES; ++i) {
    const uint16_t sample = analogRead(ArduPins::MIC_IN);

    if (sample < s.minValue) s.minValue = sample;
    if (sample > s.maxValue) s.maxValue = sample;
    sum += sample;

    const uint16_t dev =
        sample >= micDcOffset
            ? sample - micDcOffset
            : micDcOffset - sample;
    if (dev > s.peakDeviation) s.peakDeviation = dev;
  }

  s.avg = static_cast<uint16_t>(sum / ArduConfig::BLOCK_SAMPLES);
  s.p2p = s.maxValue - s.minValue;
  return s;
}

uint16_t measureDc() {
  uint32_t sum = 0;
  for (uint16_t i = 0; i < 256; ++i) {
    sum += analogRead(ArduPins::MIC_IN);
  }
  return static_cast<uint16_t>(sum / 256UL);
}

void resetStats() {
  candidateCount = 0;
  maxBlockP2P = 0;
  maxEventP2P = 0;
  lastEventP2P = 0;
  lastEventMs = 0;
  previousEventMs = 0;
}

void printEvent(const BlockStats& s, unsigned long now) {
  unsigned long gap = 0;
  if (lastEventMs != 0) gap = now - lastEventMs;

  previousEventMs = lastEventMs;
  lastEventMs = now;

  ++candidateCount;
  lastEventP2P = s.p2p;
  if (s.p2p > maxEventP2P) maxEventP2P = s.p2p;

  Serial.print(F("EVENT IMPULSE_CANDIDATE MS="));
  Serial.print(now);
  Serial.print(F(" P2P="));
  Serial.print(s.p2p);
  Serial.print(F(" PEAK_DEV="));
  Serial.print(s.peakDeviation);
  Serial.print(F(" AVG="));
  Serial.print(s.avg);
  Serial.print(F(" GAP_MS="));
  Serial.println(gap);
}

void processAudio() {
  lastBlock = readBlock();

  if (lastBlock.p2p > maxBlockP2P) {
    maxBlockP2P = lastBlock.p2p;
  }

  const unsigned long now = millis();

  if (armed &&
      lastBlock.p2p >= thresholdP2P &&
      (lastEventMs == 0 || now - lastEventMs >= ArduConfig::EVENT_LOCKOUT_MS)) {
    printEvent(lastBlock, now);
  }

  if (monitorEnabled &&
      now - lastMonitorMs >= ArduConfig::MONITOR_PERIOD_MS) {
    lastMonitorMs = now;

    Serial.print(F("MON P2P="));
    Serial.print(lastBlock.p2p);
    Serial.print(F(" MAX_BLOCK="));
    Serial.print(maxBlockP2P);
    Serial.print(F(" THRESH="));
    Serial.print(thresholdP2P);
    Serial.print(F(" EVENTS="));
    Serial.print(candidateCount);
    Serial.print(F(" DC="));
    Serial.println(micDcOffset);
  }
}

void printAdc() {
  const BlockStats s = readBlock();

  Serial.print(F("ADC AVG="));
  Serial.print(s.avg);
  Serial.print(F(" MIN="));
  Serial.print(s.minValue);
  Serial.print(F(" MAX="));
  Serial.print(s.maxValue);
  Serial.print(F(" P2P="));
  Serial.print(s.p2p);
  Serial.print(F(" PEAK_DEV="));
  Serial.println(s.peakDeviation);
}

void calibrateClap() {
  const bool wasArmed = armed;
  armed = false;

  micDcOffset = measureDc();

  if (!micDcValid()) {
    calibrated = false;
    armed = wasArmed;

    Serial.print(F("ERR CALCLAP_BAD_DC DC="));
    Serial.print(micDcOffset);
    Serial.print(F(" EXPECTED="));
    Serial.print(ArduConfig::MIC_DC_MIN);
    Serial.print(F(".."));
    Serial.println(ArduConfig::MIC_DC_MAX);
    return;
  }

  uint16_t quietMaxP2P = 0;
  uint32_t quietSumP2P = 0;

  for (uint16_t i = 0; i < ArduConfig::CAL_BLOCKS; ++i) {
    const BlockStats s = readBlock();
    quietSumP2P += s.p2p;
    if (s.p2p > quietMaxP2P) quietMaxP2P = s.p2p;
  }

  uint32_t proposedA = static_cast<uint32_t>(quietMaxP2P) * 3UL;
  uint32_t proposedB = static_cast<uint32_t>(quietMaxP2P) + 80UL;
  uint32_t proposed = proposedA > proposedB ? proposedA : proposedB;

  if (proposed < 120UL) proposed = 120UL;
  if (proposed > ArduConfig::MAX_THRESHOLD_P2P) {
    proposed = ArduConfig::MAX_THRESHOLD_P2P;
  }

  thresholdP2P = static_cast<uint16_t>(proposed);
  calibrated = true;
  resetStats();
  armed = wasArmed;

  Serial.print(F("CALCLAP DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_AVG_P2P="));
  Serial.print(static_cast<uint16_t>(
      quietSumP2P / ArduConfig::CAL_BLOCKS));
  Serial.print(F(" QUIET_MAX_P2P="));
  Serial.print(quietMaxP2P);
  Serial.print(F(" THRESH="));
  Serial.println(thresholdP2P);
}

void printStatus() {
  Serial.print(F("STATUS FW=CLAP_DIAG REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MIC=MAX9814 MIC_PIN=A0 MIC_GAIN=40DB AR=FLOAT ADC_REF=DEFAULT"));
  Serial.print(F(" DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" DC_OK="));
  Serial.print(micDcValid() ? F("YES") : F("NO"));
  Serial.print(F(" CAL="));
  Serial.print(calibrated ? F("YES") : F("NO"));
  Serial.print(F(" THRESH="));
  Serial.print(thresholdP2P);
  Serial.print(F(" LOCKOUT_MS="));
  Serial.print(ArduConfig::EVENT_LOCKOUT_MS);
  Serial.print(F(" ARMED="));
  Serial.print(armed ? F("YES") : F("NO"));
  Serial.print(F(" MON="));
  Serial.print(monitorEnabled ? F("YES") : F("NO"));
  Serial.print(F(" EVENTS="));
  Serial.print(candidateCount);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
}

void printStats() {
  Serial.print(F("STATS EVENTS="));
  Serial.print(candidateCount);
  Serial.print(F(" MAX_BLOCK_P2P="));
  Serial.print(maxBlockP2P);
  Serial.print(F(" MAX_EVENT_P2P="));
  Serial.print(maxEventP2P);
  Serial.print(F(" LAST_EVENT_P2P="));
  Serial.print(lastEventP2P);
  Serial.print(F(" LAST_EVENT_MS="));
  Serial.print(lastEventMs);
  Serial.print(F(" PREV_EVENT_MS="));
  Serial.println(previousEventMs);
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

  if (strcmp(command, "CALCLAP") == 0) {
    calibrateClap();
    return;
  }

  if (strcmp(command, "STATS") == 0) {
    printStats();
    return;
  }

  if (strcmp(command, "CLEAR") == 0) {
    resetStats();
    Serial.println(F("OK CLEARED"));
    return;
  }

  if (strncmp(command, "THRESH ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(
            command + 7,
            ArduConfig::MIN_THRESHOLD_P2P,
            ArduConfig::MAX_THRESHOLD_P2P,
            value)) {
      Serial.println(F("ERR BAD_THRESH RANGE=60..900"));
      return;
    }

    thresholdP2P = static_cast<uint16_t>(value);
    resetStats();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "ARM ON") == 0) {
    armed = true;
    Serial.println(F("OK ARMED=YES"));
    return;
  }

  if (strcmp(command, "ARM OFF") == 0) {
    armed = false;
    Serial.println(F("OK ARMED=NO"));
    return;
  }

  if (strcmp(command, "MON ON") == 0) {
    monitorEnabled = true;
    Serial.println(F("OK MON=YES"));
    return;
  }

  if (strcmp(command, "MON OFF") == 0) {
    monitorEnabled = false;
    Serial.println(F("OK MON=NO"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ADC CALCLAP THRESH ARM MON STATS CLEAR HELP"));
    Serial.println(F("THRESH 60..900 | ARM ON|OFF | MON ON|OFF"));
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

  delay(200);
  micDcOffset = measureDc();

  Serial.println(F("ARDU NANO CLAP DIAG R1 READY"));
  Serial.println(F("NO LED CONTROL; LOG IMPULSE CANDIDATES ONLY"));
  printStatus();
}

void loop() {
  pollSerial();
  processAudio();
}
