/*
  ARDU L01 R3 — ordinary light with approximate Kelvin + custom RGB + EEPROM startup profile.

  R2 adds:
  - KELVIN 1800..6500 (approximate visual CCT on RGB WS2812B, not calibrated CCT);
  - PRESET 2700 / 4000 / 6000;
  - RGB r g b custom color;
  - BRIGHT 0..255;
  - SAVE / LOAD persistence in ATmega328P internal EEPROM;
  - startup always POWER=ON and restores the saved L01 color profile + brightness.

  Hardware layer uses the updated physical D6 / 44-LED ring with D7 held LOW.
  Clap detection is intentionally NOT included in R2.
*/

#include <Arduino.h>
#include <EEPROM.h>

#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

namespace ArduPins {
constexpr uint8_t RING_A = 6;
constexpr uint8_t RING_B = 7;
}

namespace ArduConfig {
constexpr unsigned long SERIAL_BAUD = 115200UL;
constexpr uint8_t FW_REV = 3;
constexpr uint16_t LED_COUNT = 44;
constexpr uint16_t MAX_MILLIAMPS = 500;
constexpr uint8_t DEFAULT_BRIGHTNESS = 64;
constexpr uint16_t DEFAULT_KELVIN = 4000;
constexpr uint16_t MIN_KELVIN = 1800;
constexpr uint16_t MAX_KELVIN = 6500;
constexpr unsigned long SERIAL_RX_GUARD_MS = 5UL;
constexpr size_t RX_BUFFER_SIZE = 80;

constexpr uint16_t EEPROM_MAGIC = 0x4C32;  // "L2"
constexpr uint8_t EEPROM_VERSION = 2;
constexpr int EEPROM_BASE = 128;
}

enum class ColorMode : uint8_t {
  KELVIN = 0,
  RGB = 1
};

struct PersistedL01 {
  uint16_t magic;
  uint8_t version;
  uint8_t colorMode;
  uint8_t brightness;
  uint16_t kelvin;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t checksum;
};

struct ArduSettings {
  bool power = true;
  ColorMode colorMode = ColorMode::KELVIN;
  uint8_t brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  uint16_t kelvin = ArduConfig::DEFAULT_KELVIN;
  CRGB customRgb = CRGB::White;
};

struct TempAnchor {
  uint16_t kelvin;
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Project visual mapping for WS2812B RGB.
// Exact perceived CCT depends on the actual LEDs/diffuser and will be tuned later if needed.
const TempAnchor TEMP_ANCHORS[] = {
  {1800, 255, 147,  41},
  {2200, 255, 157,  61},
  {2700, 255, 170,  87},
  {3000, 255, 183, 114},
  {4000, 255, 228, 206},
  {5000, 255, 244, 234},
  {6000, 245, 249, 255},
  {6500, 232, 241, 255}
};

constexpr uint8_t TEMP_ANCHOR_COUNT =
    sizeof(TEMP_ANCHORS) / sizeof(TEMP_ANCHORS[0]);

ArduSettings settings;
CRGB leds[ArduConfig::LED_COUNT];

char rxBuffer[ArduConfig::RX_BUFFER_SIZE];
size_t rxLength = 0;
unsigned long lastSerialRxMs = 0;
bool renderDirty = true;
bool persistedValid = false;
bool dirtySettings = false;

uint8_t checksumFor(const PersistedL01& data) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
  uint8_t sum = 0x5A;
  for (size_t i = 0; i < sizeof(PersistedL01) - 1; ++i) {
    sum = static_cast<uint8_t>((sum << 1) | (sum >> 7));
    sum ^= bytes[i];
  }
  return sum;
}

bool validatePersisted(const PersistedL01& data) {
  if (data.magic != ArduConfig::EEPROM_MAGIC) return false;
  if (data.version != ArduConfig::EEPROM_VERSION) return false;
  if (data.colorMode > static_cast<uint8_t>(ColorMode::RGB)) return false;
  if (data.kelvin < ArduConfig::MIN_KELVIN ||
      data.kelvin > ArduConfig::MAX_KELVIN) return false;
  return data.checksum == checksumFor(data);
}

uint8_t interpolate8(
    uint8_t a,
    uint8_t b,
    uint16_t numerator,
    uint16_t denominator) {
  if (denominator == 0) return a;
  const int16_t delta = static_cast<int16_t>(b) - static_cast<int16_t>(a);
  const int32_t value =
      static_cast<int32_t>(a) +
      (static_cast<int32_t>(delta) * numerator) / denominator;
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

CRGB kelvinToRgb(uint16_t kelvin) {
  if (kelvin <= TEMP_ANCHORS[0].kelvin) {
    return CRGB(TEMP_ANCHORS[0].r, TEMP_ANCHORS[0].g, TEMP_ANCHORS[0].b);
  }

  for (uint8_t i = 0; i < TEMP_ANCHOR_COUNT - 1; ++i) {
    const TempAnchor& a = TEMP_ANCHORS[i];
    const TempAnchor& b = TEMP_ANCHORS[i + 1];

    if (kelvin <= b.kelvin) {
      const uint16_t numerator = kelvin - a.kelvin;
      const uint16_t denominator = b.kelvin - a.kelvin;

      return CRGB(
          interpolate8(a.r, b.r, numerator, denominator),
          interpolate8(a.g, b.g, numerator, denominator),
          interpolate8(a.b, b.b, numerator, denominator)
      );
    }
  }

  const TempAnchor& last = TEMP_ANCHORS[TEMP_ANCHOR_COUNT - 1];
  return CRGB(last.r, last.g, last.b);
}

CRGB activeColor() {
  if (settings.colorMode == ColorMode::RGB) return settings.customRgb;
  return kelvinToRgb(settings.kelvin);
}

const __FlashStringHelper* colorModeName() {
  return settings.colorMode == ColorMode::KELVIN ? F("KELVIN") : F("RGB");
}

bool serialRxGuardActive() {
  return (millis() - lastSerialRxMs) < ArduConfig::SERIAL_RX_GUARD_MS;
}

void renderLightNow() {
  fill_solid(
      leds,
      ArduConfig::LED_COUNT,
      settings.power ? activeColor() : CRGB::Black
  );
  FastLED.setBrightness(settings.brightness);
  FastLED.show();
  renderDirty = false;
}

void requestRender() {
  renderDirty = true;
}

void renderIfReady() {
  if (!renderDirty || serialRxGuardActive()) return;
  renderLightNow();
}

void loadDefaults() {
  settings.power = true;
  settings.colorMode = ColorMode::KELVIN;
  settings.brightness = ArduConfig::DEFAULT_BRIGHTNESS;
  settings.kelvin = ArduConfig::DEFAULT_KELVIN;
  settings.customRgb = CRGB::White;
  persistedValid = false;
  dirtySettings = false;
}

void loadSettings() {
  PersistedL01 data;
  EEPROM.get(ArduConfig::EEPROM_BASE, data);

  if (!validatePersisted(data)) {
    loadDefaults();
    return;
  }

  settings.power = true;
  settings.colorMode = static_cast<ColorMode>(data.colorMode);
  settings.brightness = data.brightness;
  settings.kelvin = data.kelvin;
  settings.customRgb = CRGB(data.red, data.green, data.blue);
  persistedValid = true;
  dirtySettings = false;
}

void saveSettings() {
  PersistedL01 data;
  data.magic = ArduConfig::EEPROM_MAGIC;
  data.version = ArduConfig::EEPROM_VERSION;
  data.colorMode = static_cast<uint8_t>(settings.colorMode);
  data.brightness = settings.brightness;
  data.kelvin = settings.kelvin;
  data.red = settings.customRgb.r;
  data.green = settings.customRgb.g;
  data.blue = settings.customRgb.b;
  data.checksum = 0;
  data.checksum = checksumFor(data);

  EEPROM.put(ArduConfig::EEPROM_BASE, data);
  persistedValid = true;
  dirtySettings = false;
}

void printStatus() {
  const CRGB shown = activeColor();

  Serial.print(F("STATUS FW=L01 REV="));
  Serial.print(ArduConfig::FW_REV);
  Serial.print(F(" MODE=L01 POWER="));
  Serial.print(settings.power ? F("ON") : F("OFF"));
  Serial.print(F(" COLOR_MODE="));
  Serial.print(colorModeName());
  Serial.print(F(" BRIGHTNESS="));
  Serial.print(settings.brightness);
  Serial.print(F(" KELVIN="));
  Serial.print(settings.kelvin);
  Serial.print(F(" RGB="));
  Serial.print(shown.r);
  Serial.print(',');
  Serial.print(shown.g);
  Serial.print(',');
  Serial.print(shown.b);
  Serial.print(F(" SAVED="));
  Serial.print(persistedValid ? F("YES") : F("NO"));
  Serial.print(F(" DIRTY="));
  Serial.print(dirtySettings ? F("YES") : F("NO"));
  Serial.print(F(" LIMIT_MA="));
  Serial.print(ArduConfig::MAX_MILLIAMPS);
  Serial.print(F(" STARTUP_PROFILE=EEPROM"));
  Serial.print(F(" CLAP=NOT_IN_R2"));
  Serial.print(F(" LEDS="));
  Serial.print(ArduConfig::LED_COUNT);
  Serial.print(F(" UPTIME_MS="));
  Serial.println(millis());
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

bool parseRgb(const char* text, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (text == nullptr) return false;

  char* end = nullptr;
  long rv = strtol(text, &end, 10);
  if (end == text || rv < 0 || rv > 255) return false;

  while (*end == ' ') ++end;
  char* end2 = nullptr;
  long gv = strtol(end, &end2, 10);
  if (end2 == end || gv < 0 || gv > 255) return false;

  while (*end2 == ' ') ++end2;
  char* end3 = nullptr;
  long bv = strtol(end2, &end3, 10);
  if (end3 == end2 || bv < 0 || bv > 255) return false;

  while (*end3 == ' ') ++end3;
  if (*end3 != '\0') return false;

  r = static_cast<uint8_t>(rv);
  g = static_cast<uint8_t>(gv);
  b = static_cast<uint8_t>(bv);
  return true;
}

void setKelvin(uint16_t kelvin) {
  settings.colorMode = ColorMode::KELVIN;
  settings.kelvin = kelvin;
  dirtySettings = true;
  requestRender();
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
    requestRender();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "OFF") == 0) {
    settings.power = false;
    renderLightNow();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "BRIGHT ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(command + 7, 0, 255, value)) {
      Serial.println(F("ERR BAD_BRIGHTNESS"));
      return;
    }
    settings.brightness = static_cast<uint8_t>(value);
    dirtySettings = true;
    requestRender();
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "KELVIN ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(
            command + 7,
            ArduConfig::MIN_KELVIN,
            ArduConfig::MAX_KELVIN,
            value)) {
      Serial.println(F("ERR BAD_KELVIN RANGE=1800..6500"));
      return;
    }
    setKelvin(static_cast<uint16_t>(value));
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "PRESET ", 7) == 0) {
    long value = 0;
    if (!parseLongRange(command + 7, 0, 10000, value) ||
        (value != 2700 && value != 4000 && value != 6000)) {
      Serial.println(F("ERR BAD_PRESET VALUES=2700,4000,6000"));
      return;
    }
    setKelvin(static_cast<uint16_t>(value));
    Serial.println(F("OK"));
    return;
  }

  if (strncmp(command, "RGB ", 4) == 0) {
    uint8_t r = 0, g = 0, b = 0;
    if (!parseRgb(command + 4, r, g, b)) {
      Serial.println(F("ERR BAD_RGB FORMAT=RGB R G B"));
      return;
    }
    settings.colorMode = ColorMode::RGB;
    settings.customRgb = CRGB(r, g, b);
    dirtySettings = true;
    requestRender();
    Serial.println(F("OK"));
    return;
  }

  if (strcmp(command, "SAVE") == 0) {
    saveSettings();
    Serial.println(F("OK SAVED"));
    return;
  }

  if (strcmp(command, "LOAD") == 0) {
    loadSettings();
    requestRender();
    Serial.println(F("OK LOADED"));
    return;
  }

  if (strcmp(command, "DEFAULTS") == 0) {
    loadDefaults();
    dirtySettings = true;
    requestRender();
    Serial.println(F("OK DEFAULTS_NOT_SAVED"));
    return;
  }

  if (strcmp(command, "HELP") == 0) {
    Serial.println(F("CMDS PING STATUS ON OFF BRIGHT KELVIN PRESET RGB SAVE LOAD DEFAULTS"));
    Serial.println(F("KELVIN 1800..6500 | PRESET 2700|4000|6000 | RGB R G B"));
    return;
  }

  Serial.println(F("ERR UNKNOWN_COMMAND"));
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    lastSerialRxMs = millis();

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

  FastLED.addLeds<WS2812B, ArduPins::RING_A, GRB>(
      leds,
      ArduConfig::LED_COUNT
  );
  FastLED.setMaxPowerInVoltsAndMilliamps(
      5,
      ArduConfig::MAX_MILLIAMPS
  );
  FastLED.clear(true);

  loadSettings();

  Serial.println(F("ARDU NANO L01 R3 READY"));
  Serial.println(F("KELVIN+RGB+EEPROM STARTUP PROFILE; 44 LED; CLAP NOT IN R3"));
  printStatus();
}

void loop() {
  pollSerial();
  renderIfReady();
}
