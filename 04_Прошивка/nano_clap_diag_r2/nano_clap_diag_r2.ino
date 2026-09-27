/*
  ARDU CLAP DIAG R2 — threshold-free impulse capture + dual-metric event diagnostic.

  Changes from R1:
  - adds CAPTURE command: 1 s arming delay + 3 s raw measurement window;
  - records max P2P and max absolute deviation from calibrated DC independently;
  - event detector can trigger from P2P OR peak deviation;
  - shorter 16-sample blocks improve timing visibility;
  - CALCLAP now calibrates quiet P2P and quiet peak-deviation separately.

  This sketch does NOT control the LEDs.
*/

#include <Arduino.h>

namespace ArduPins {
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 2;

constexpr uint8_t BLOCK_SAMPLES = 16;
constexpr uint16_t MIC_DC_MIN = 120;
constexpr uint16_t MIC_DC_MAX = 400;

constexpr uint16_t DEFAULT_THRESH_P2P = 120;
constexpr uint16_t DEFAULT_THRESH_DEV = 90;
constexpr uint16_t MIN_THRESHOLD = 20;
constexpr uint16_t MAX_THRESHOLD = 900;

constexpr unsigned long EVENT_LOCKOUT_MS = 120UL;
constexpr unsigned long MONITOR_PERIOD_MS = 500UL;

constexpr uint16_t CAL_BLOCKS = 400;
constexpr unsigned long CAPTURE_DELAY_MS = 1000UL;
constexpr unsigned long CAPTURE_WINDOW_MS = 3000UL;

constexpr size_t RX_BUFFER_SIZE = 80;
}

struct BlockStats {
  uint16_t avg = 0;
  uint16_t minValue = 1023;
  uint16_t maxValue = 0;
  uint16_t p2p = 0;
  uint16_t peakDeviation = 0;
};

struct CaptureStats {
  uint16_t maxP2P = 0;
  uint16_t maxDeviation = 0;
  uint16_t globalMin = 1023;
  uint16_t globalMax = 0;
  uint16_t maxAvgShift = 0;
  uint32_t blocks = 0;
};

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

uint16_t micDcOffset = 250;
uint16_t thresholdP2P = ArduConfig::DEFAULT_THRESH_P2P;
uint16_t thresholdDev = ArduConfig::DEFAULT_THRESH_DEV;

bool armed = true;
bool monitorEnabled = false;
bool calibrated = false;

BlockStats lastBlock;

uint32_t candidateCount = 0;
uint16_t maxBlockP2P = 0;
uint16_t maxBlockDev = 0;
uint16_t maxEventP2P = 0;
uint16_t maxEventDev = 0;
uint16_t lastEventP2P = 0;
uint16_t lastEventDev = 0;
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
  maxBlockDev = 0;
  maxEventP2P = 0;
  maxEventDev = 0;
  lastEventP2P = 0;
  lastEventDev = 0;
  lastEventMs = 0;
  previousEventMs = 0;
}

void printEvent(const BlockStats& s, unsigned long now) {
  const unsigned long gap =
      lastEventMs == 0 ? 0UL : now - lastEventMs;

  previousEventMs = lastEventMs;
  lastEventMs = now;

  ++candidateCount;
  lastEventP2P = s.p2p;
  lastEventDev = s.peakDeviation;

  if (s.p2p > maxEventP2P) maxEventP2P = s.p2p;
  if (s.peakDeviation > maxEventDev) maxEventDev = s.peakDeviation;

  Serial.print(F("EVENT IMPULSE_CANDIDATE MS="));
  Serial.print(now);
  Serial.print(F(" P2P="));
  Serial.print(s.p2p);
  Serial.print(F(" DEV="));
  Serial.print(s.peakDeviation);
  Serial.print(F(" AVG="));
  Serial.print(s.avg);
  Serial.print(F(" MIN="));
  Serial.print(s.minValue);
  Serial.print(F(" MAX="));
  Serial.print(s.maxValue);
  Serial.print(F(" GAP_MS="));
  Serial.println(gap);
}

void processAudio() {
  lastBlock = readBlock();

  if (lastBlock.p2p > maxBlockP2P) maxBlockP2P = lastBlock.p2p;
  if (lastBlock.peakDeviation > maxBlockDev) {
    maxBlockDev = lastBlock.peakDeviation;
  }

  const unsigned long now = millis();

  const bool aboveThreshold =
      lastBlock.p2p >= thresholdP2P ||
      lastBlock.peakDeviation >= thresholdDev;

  if (armed &&
      aboveThreshold &&
      (lastEventMs == 0 ||
       now - lastEventMs >= ArduConfig::EVENT_LOCKOUT_MS)) {
    printEvent(lastBlock, now);
  }

  if (monitorEnabled &&
      now - lastMonitorMs >= ArduConfig::MONITOR_PERIOD_MS) {
    lastMonitorMs = now;

    Serial.print(F("MON P2P="));
    Serial.print(lastBlock.p2p);
    Serial.print(F(" DEV="));
    Serial.print(lastBlock.peakDeviation);
    Serial.print(F(" MAX_P2P="));
    Serial.print(maxBlockP2P);
    Serial.print(F(" MAX_DEV="));
    Serial.print(maxBlockDev);
    Serial.print(F(" THRESH="));
    Serial.print(thresholdP2P);
    Serial.print(',');
    Serial.print(thresholdDev);
    Serial.print(F(" EVENTS="));
    Serial.println(candidateCount);
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
  Serial.print(F(" DEV="));
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
  uint16_t quietMaxDev = 0;
  uint32_t quietSumP2P = 0;
  uint32_t quietSumDev = 0;

  for (uint16_t i = 0; i < ArduConfig::CAL_BLOCKS; ++i) {
    const BlockStats s = readBlock();

    quietSumP2P += s.p2p;
    quietSumDev += s.peakDeviation;

    if (s.p2p > quietMaxP2P) quietMaxP2P = s.p2p;
    if (s.peakDeviation > quietMaxDev) quietMaxDev = s.peakDeviation;
  }

  uint32_t p2pCandidate =
      static_cast<uint32_t>(quietMaxP2P) + 50UL;
  uint32_t devCandidate =
      static_cast<uint32_t>(quietMaxDev) + 35UL;

  const uint32_t p2pDouble =
      static_cast<uint32_t>(quietMaxP2P) * 2UL;
  const uint32_t devDouble =
      static_cast<uint32_t>(quietMaxDev) * 2UL;

  if (p2pDouble > p2pCandidate) p2pCandidate = p2pDouble;
  if (devDouble > devCandidate) devCandidate = devDouble;

  if (p2pCandidate < 80UL) p2pCandidate = 80UL;
  if (devCandidate < 60UL) devCandidate = 60UL;

  if (p2pCandidate > ArduConfig::MAX_THRESHOLD) {
    p2pCandidate = ArduConfig::MAX_THRESHOLD;
  }
  if (devCandidate > ArduConfig::MAX_THRESHOLD) {
    devCandidate = ArduConfig::MAX_THRESHOLD;
  }

  thresholdP2P = static_cast<uint16_t>(p2pCandidate);
  thresholdDev = static_cast<uint16_t>(devCandidate);

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
  Serial.print(F(" QUIET_AVG_DEV="));
  Serial.print(static_cast<uint16_t>(
      quietSumDev / ArduConfig::CAL_BLOCKS));
  Serial.print(F(" QUIET_MAX_DEV="));
  Serial.print(quietMaxDev);
  Serial.print(F(" THRESH_P2P="));
  Serial.print(thresholdP2P);
  Serial.print(F(" THRESH_DEV="));
  Serial.println(thresholdDev);
}

void captureWindow() {
  const bool wasArmed = armed;
  const bool wasMonitor = monitorEnabled;

  armed = false;
  monitorEnabled = false;

  Serial.print(F("CAPTURE_ARMING DELAY_MS="));
  Serial.print(ArduConfig::CAPTURE_DELAY_MS);
  Serial.print(F(" WINDOW_MS="));
  Serial.println(ArduConfig::CAPTURE_WINDOW_MS);

  delay(ArduConfig::CAPTURE_DELAY_MS);

  Serial.println(F("CAPTURE_GO"));

  CaptureStats cap;
  const unsigned long start = millis();

  while (millis() - start < ArduConfig::CAPTURE_WINDOW_MS) {
    const BlockStats s = readBlock();

    if (s.p2p > cap.maxP2P) cap.maxP2P = s.p2p;
    if (s.peakDeviation > cap.maxDeviation) {
      cap.maxDeviation = s.peakDeviation;
    }
    if (s.minValue < cap.globalMin) cap.globalMin = s.minValue;
    if (s.maxValue > cap.globalMax) cap.globalMax = s.maxValue;

    const uint16_t avgShift =
        s.avg >= micDcOffset
            ? s.avg - micDcOffset
            : micDcOffset - s.avg;

    if (avgShift > cap.maxAvgShift) cap.maxAvgShift = avgShift;

    ++cap.blocks;
  }

  Serial.print(F("CAPTURE RESULT MAX_P2P="));
  Serial.print(cap.maxP2P);
  Serial.print(F(" MAX_DEV="));
  Serial.print(cap.maxDeviation);
  Serial.print(F(" MIN="));
  Serial.print(cap.globalMin);
  Serial.print(F(" MAX="));
  Serial.print(cap.globalMax);
  Serial.print(F(" MAX_AVG_SHIFT="));
  Serial.print(cap.maxAvgShift);
  Serial.print(F(" BLOCKS="));
  Serial.println(cap.blocks);

  armed = wasArmed;
  monitorEnabled = wasMonitor;
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
  Serial.print(F(" THRESH_P2P="));
  Serial.print(thresholdP2P);
  Serial.print(F(" THRESH_DEV="));
  Serial.print(thresholdDev);
  Serial.print(F(" BLOCK_N="));
  Serial.print(ArduConfig::BLOCK_SAMPLES);
  Serial.print(F(" LOCKOUT_MS="));
  Serial.print(ArduConfig::EVENT_LOCKOUT_MS);
  Serial.print(F(" ARMED="));
  Serial.print(armed ? F("YES") : F("NO"));
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
  Serial.print(F(" MAX_BLOCK_DEV="));
  Serial.print(maxBlockDev);
  Serial.print(F(" MAX_EVENT_P2P="));
  Serial.print(maxEventP2P);
  Serial.print(F(" MAX_EVENT_DEV="));
  Serial.print(maxEventDev);
  Serial.print(F(" LAST_EVENT_P2P="));
  Serial.print(lastEventP2P);
  Serial.print(F(" LAST_EVENT_DEV="));
  Serial.print(lastEventDev);
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

  if (strcmp(command, "CAPTURE") == 0) {
    captureWindow();
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
    char* sep = strchr(command + 7, ' ');
    if (sep == nullptr) {
      Serial.println(F("ERR BAD_THRESH FORMAT=THRESH P2P DEV"));
      return;
    }

    *sep = '\0';

    long p2p = 0;
    long dev = 0;

    if (!parseLongRange(
            command + 7,
            ArduConfig::MIN_THRESHOLD,
            ArduConfig::MAX_THRESHOLD,
            p2p) ||
        !parseLongRange(
            sep + 1,
            ArduConfig::MIN_THRESHOLD,
            ArduConfig::MAX_THRESHOLD,
            dev)) {
      Serial.println(F("ERR BAD_THRESH RANGE=20..900"));
      return;
    }

    thresholdP2P = static_cast<uint16_t>(p2p);
    thresholdDev = static_cast<uint16_t>(dev);
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
    Serial.println(F("CMDS PING STATUS ADC CALCLAP CAPTURE THRESH ARM MON STATS CLEAR HELP"));
    Serial.println(F("THRESH <P2P 20..900> <DEV 20..900>"));
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

  Serial.println(F("ARDU NANO CLAP DIAG R2 READY"));
  Serial.println(F("USE CAPTURE TO MEASURE SILENCE/CLICK/CLAP WITHOUT THRESHOLD"));
  printStatus();
}

void loop() {
  pollSerial();
  processAudio();
}
