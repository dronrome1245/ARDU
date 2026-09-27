/*
  ARDU CLAP DIAG R3 — robust quiet calibration + delayed arming.

  Fixes found on hardware:
  - mouse click used to press Serial Monitor Send is itself a strong impulse;
  - R1/R2 calibration started immediately and could count that click as "quiet max";
  - absolute max over a long quiet window is too sensitive to one accidental impulse.

  R3:
  - CALCLAP waits 1500 ms before measuring;
  - thresholds are derived from quiet AVERAGES, not quiet maxima;
  - CLEAR and ARM ON schedule arming after 1500 ms, excluding command-click noise;
  - event candidate requires BOTH P2P and absolute deviation thresholds;
  - 16-sample blocks retained;
  - no LED control.
*/

#include <Arduino.h>

namespace ArduPins {
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 3;

constexpr uint8_t BLOCK_SAMPLES = 16;
constexpr uint16_t MIC_DC_MIN = 120;
constexpr uint16_t MIC_DC_MAX = 400;

constexpr uint16_t DEFAULT_THRESH_P2P = 120;
constexpr uint16_t DEFAULT_THRESH_DEV = 90;

constexpr unsigned long COMMAND_SETTLE_MS = 1500UL;
constexpr unsigned long EVENT_LOCKOUT_MS = 250UL;
constexpr unsigned long MONITOR_PERIOD_MS = 500UL;

constexpr uint16_t CAL_BLOCKS = 400;
constexpr unsigned long CAPTURE_DELAY_MS = 1500UL;
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

bool armed = false;
bool monitorEnabled = false;
bool calibrated = false;

bool armPending = false;
unsigned long armAtMs = 0;

BlockStats lastBlock;

uint32_t candidateCount = 0;
uint16_t maxBlockP2P = 0;
uint16_t maxBlockDev = 0;
uint16_t maxEventP2P = 0;
uint16_t maxEventDev = 0;
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
  lastEventMs = 0;
  previousEventMs = 0;
}

void scheduleArm() {
  armed = false;
  armPending = true;
  armAtMs = millis() + ArduConfig::COMMAND_SETTLE_MS;

  Serial.print(F("OK ARMING DELAY_MS="));
  Serial.println(ArduConfig::COMMAND_SETTLE_MS);
}

void serviceArm() {
  if (!armPending) return;

  if (static_cast<long>(millis() - armAtMs) >= 0) {
    armPending = false;
    armed = true;
    lastEventMs = 0;
    previousEventMs = 0;
    Serial.println(F("EVENT ARMED"));
  }
}

void printEvent(const BlockStats& s, unsigned long now) {
  const unsigned long gap =
      lastEventMs == 0 ? 0UL : now - lastEventMs;

  previousEventMs = lastEventMs;
  lastEventMs = now;

  ++candidateCount;

  if (s.p2p > maxEventP2P) maxEventP2P = s.p2p;
  if (s.peakDeviation > maxEventDev) maxEventDev = s.peakDeviation;

  Serial.print(F("EVENT IMPULSE MS="));
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
      lastBlock.p2p >= thresholdP2P &&
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
  armed = false;
  armPending = false;

  Serial.print(F("CALCLAP_ARMING DELAY_MS="));
  Serial.println(ArduConfig::COMMAND_SETTLE_MS);
  delay(ArduConfig::COMMAND_SETTLE_MS);

  micDcOffset = measureDc();

  if (!micDcValid()) {
    calibrated = false;
    Serial.print(F("ERR CALCLAP_BAD_DC DC="));
    Serial.println(micDcOffset);
    return;
  }

  uint32_t sumP2P = 0;
  uint32_t sumDev = 0;
  uint16_t maxP2P = 0;
  uint16_t maxDev = 0;

  for (uint16_t i = 0; i < ArduConfig::CAL_BLOCKS; ++i) {
    const BlockStats s = readBlock();

    sumP2P += s.p2p;
    sumDev += s.peakDeviation;

    if (s.p2p > maxP2P) maxP2P = s.p2p;
    if (s.peakDeviation > maxDev) maxDev = s.peakDeviation;
  }

  const uint16_t avgP2P =
      static_cast<uint16_t>(sumP2P / ArduConfig::CAL_BLOCKS);
  const uint16_t avgDev =
      static_cast<uint16_t>(sumDev / ArduConfig::CAL_BLOCKS);

  uint32_t p2p =
      static_cast<uint32_t>(avgP2P) * 4UL + 40UL;
  uint32_t dev =
      static_cast<uint32_t>(avgDev) * 4UL + 30UL;

  if (p2p < 100UL) p2p = 100UL;
  if (dev < 80UL) dev = 80UL;
  if (p2p > 500UL) p2p = 500UL;
  if (dev > 350UL) dev = 350UL;

  thresholdP2P = static_cast<uint16_t>(p2p);
  thresholdDev = static_cast<uint16_t>(dev);
  calibrated = true;
  resetStats();

  Serial.print(F("CALCLAP DC="));
  Serial.print(micDcOffset);
  Serial.print(F(" QUIET_AVG_P2P="));
  Serial.print(avgP2P);
  Serial.print(F(" QUIET_MAX_P2P_INFO="));
  Serial.print(maxP2P);
  Serial.print(F(" QUIET_AVG_DEV="));
  Serial.print(avgDev);
  Serial.print(F(" QUIET_MAX_DEV_INFO="));
  Serial.print(maxDev);
  Serial.print(F(" THRESH_P2P="));
  Serial.print(thresholdP2P);
  Serial.print(F(" THRESH_DEV="));
  Serial.println(thresholdDev);

  scheduleArm();
}

void captureWindow() {
  const bool wasMonitor = monitorEnabled;

  armed = false;
  armPending = false;
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

  monitorEnabled = wasMonitor;
  scheduleArm();
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
  Serial.print(F(" ARM_PENDING="));
  Serial.print(armPending ? F("YES") : F("NO"));
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
  Serial.print(F(" LAST_EVENT_MS="));
  Serial.print(lastEventMs);
  Serial.print(F(" PREV_EVENT_MS="));
  Serial.println(previousEventMs);
}

bool parsePair(char* text, long& a, long& b) {
  char* sep = strchr(text, ' ');
  if (sep == nullptr) return false;
  *sep = '\0';

  char* endA = nullptr;
  char* endB = nullptr;

  a = strtol(text, &endA, 10);
  b = strtol(sep + 1, &endB, 10);

  if (*endA != '\0' || *endB != '\0') return false;
  if (a < 20 || a > 900 || b < 20 || b > 900) return false;
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
    scheduleArm();
    return;
  }

  if (strncmp(command, "THRESH ", 7) == 0) {
    long p2p = 0;
    long dev = 0;

    if (!parsePair(command + 7, p2p, dev)) {
      Serial.println(F("ERR BAD_THRESH FORMAT=THRESH P2P DEV RANGE=20..900"));
      return;
    }

    thresholdP2P = static_cast<uint16_t>(p2p);
    thresholdDev = static_cast<uint16_t>(dev);
    resetStats();
    Serial.println(F("OK"));
    scheduleArm();
    return;
  }

  if (strcmp(command, "ARM ON") == 0) {
    scheduleArm();
    return;
  }

  if (strcmp(command, "ARM OFF") == 0) {
    armPending = false;
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

  Serial.println(F("ARDU NANO CLAP DIAG R3 READY"));
  Serial.println(F("DELAYED ARMING EXCLUDES SERIAL-MONITOR CLICK"));
  printStatus();
}

void loop() {
  pollSerial();
  serviceArm();
  processAudio();
}
