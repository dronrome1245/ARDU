#include <Arduino.h>
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
constexpr uint8_t MIC_IN = A0;
constexpr uint8_t RTC_SDA = A4;
constexpr uint8_t RTC_SCL = A5;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint16_t LED_COUNT = 43;
constexpr uint8_t DEFAULT_BRIGHTNESS = 32;
constexpr uint16_t MAX_MILLIAMPS_TOTAL = 1000;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t red = 32;
  uint8_t green = 0;
  uint8_t blue = 0;
};

enum class RingView : uint8_t {
  BOTH,
  A_ONLY,
  B_ONLY
};

ArduSettings settings;
RingView ringView = RingView::BOTH;

CRGB ledsA[ArduConfig::LED_COUNT];
CRGB ledsB[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

void renderFrame() {
  const CRGB color = settings.power
      ? CRGB(settings.red, settings.green, settings.blue)
      : CRGB::Black;

  fill_solid(ledsA, ArduConfig::LED_COUNT, CRGB::Black);
  fill_solid(ledsB, ArduConfig::LED_COUNT, CRGB::Black);

  if (settings.power) {
    if (ringView == RingView::BOTH || ringView == RingView::A_ONLY) {
      fill_solid(ledsA, ArduConfig::LED_COUNT, color);
    }
    if (ringView == RingView::BOTH || ringView == RingView::B_ONLY) {
      fill_solid(ledsB, ArduConfig::LED_COUNT, color);
    }
  }

  FastLED.setBrightness(settings.brightness);
  FastLED.show();
}

const __FlashStringHelper* ringViewName() {
  switch (ringView) {
    case RingView::A_ONLY:
      return F("A");
    case RingView::B_ONLY:
      return F("B");
    default:
      return F("BOTH");
  }
}

void printStatus() {
  Serial.print(F("STATUS FW=FW2 MODE=F01 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" RGB="));
  Serial.print(settings.red);
  Serial.print(',');
  Serial.print(settings.green);
  Serial.print(',');
  Serial.print(settings.blue);
  Serial.print(F(" LEDS_PER_RING="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" VIEW="));
  Serial.print(ringViewName());
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

  if (strcmp(command, "ON") == 0) {
    settings.power = true;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "BOTH") == 0) {
    ringView = RingView::BOTH;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "RING A") == 0) {
    ringView = RingView::A_ONLY;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "RING B") == 0) {
    ringView = RingView::B_ONLY;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    uint8_t value = 0;
    if (!parseByte(command + 7, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }

    settings.brightness = value;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "COLOR ", 6) == 0) {
    char* save = nullptr;
    char* token = strtok_r(command + 6, " ", &save);

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    if (!parseByte(token, r)) {
      Serial.println(F("ERR BAD_COLOR"));
      return;
    }

    token = strtok_r(nullptr, " ", &save);
    if (!parseByte(token, g)) {
      Serial.println(F("ERR BAD_COLOR"));
      return;
    }

    token = strtok_r(nullptr, " ", &save);
    if (!parseByte(token, b)) {
      Serial.println(F("ERR BAD_COLOR"));
      return;
    }

    if (strtok_r(nullptr, " ", &save) != nullptr) {
      Serial.println(F("ERR BAD_COLOR"));
      return;
    }

    settings.red = r;
    settings.green = g;
    settings.blue = b;
    settings.power = true;
    renderFrame();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BOTH RING_A RING_B"));
    Serial.println(F("FORMAT RING A | RING B | BRIGHT <n> | COLOR <r> <g> <b>"));
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

  FastLED.addLeds<WS2812B, ArduPins::RING_A, GRB>(ledsA, ArduConfig::LED_COUNT);
  FastLED.addLeds<WS2812B, ArduPins::RING_B, GRB>(ledsB, ArduConfig::LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, ArduConfig::MAX_MILLIAMPS_TOTAL);

  FastLED.clear(true);
  renderFrame();

  Serial.println(F("ARDU NANO FW2 READY"));
  printStatus();
}

void loop() {
  pollSerial();
}
