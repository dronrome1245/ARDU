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
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr size_t RX_BUFFER_SIZE = 64;
}

struct ArduSettings {
  bool power = true;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint8_t red = 32;
  uint8_t green = 0;
  uint8_t blue = 0;
};

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;

void applyF01() {
  FastLED.setBrightness(settings.brightness);

  if (settings.power) {
    fill_solid(leds, ArduConfig::LED_COUNT,
               CRGB(settings.red, settings.green, settings.blue));
  } else {
    fill_solid(leds, ArduConfig::LED_COUNT, CRGB::Black);
  }

  FastLED.show();
}

void printStatus() {
  Serial.print(F("STATUS FW=FW1 MODE=F01 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" RGB="));
  Serial.print(settings.red);
  Serial.print(',');
  Serial.print(settings.green);
  Serial.print(',');
  Serial.print(settings.blue);
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
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
    applyF01();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    applyF01();
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
    applyF01();
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
    applyF01();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BRIGHT:0..255 COLOR:R:G:B"));
    Serial.println(F("FORMAT BRIGHT <n> | COLOR <r> <g> <b>"));
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

  FastLED.addLeds<WS2812B, ArduPins::RING_A, GRB>(leds, ArduConfig::LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, ArduConfig::MAX_MILLIAMPS);
  FastLED.clear(true);
  applyF01();

  Serial.println(F("ARDU NANO FW1 READY"));
  printStatus();
}

void loop() {
  pollSerial();
}
