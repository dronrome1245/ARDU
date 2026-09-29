/*
  ARDU ESP8266 HTTP BRIDGE R1

  Purpose:
  - connect only to the home Wi-Fi network (Station mode);
  - expose a small local HTTP API;
  - bridge diagnostic/user commands to Arduino Nano over UART;
  - provide the transport needed by the Android app before full semantic API.

  Hardware UART:
    ESP8266 TX0 GPIO1 -> Nano D0/RX (3.3 V logic is safe for ATmega328P input)
    Nano D1/TX -> level divider -> ESP8266 RX0 GPIO3
    common GND is mandatory

  IMPORTANT:
  - ESP8266 is 3.3 V only.
  - Keep the local 100 uF capacitor at ESP VCC/GND.
  - Disconnect/reconfigure the Nano UART path while flashing the ESP as required
    by the already proven project programming procedure.
  - Wi-Fi credentials are NOT stored in this repository. Create wifi_secrets.h
    next to this sketch from wifi_secrets.example.h.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Fill these two values locally before compiling.
// Do not commit your real Wi-Fi password to GitHub.
const char* WIFI_SSID = "Имя_вашей_WiFi";
const char* WIFI_PASSWORD = "Пароль_вашей_WiFi";


namespace ArduConfig {
constexpr unsigned long NANO_BAUD = 115200UL;
constexpr uint16_t HTTP_PORT = 80;
constexpr char HOSTNAME[] = "ardu";

constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000UL;
constexpr unsigned long WIFI_RETRY_MS = 10000UL;

constexpr unsigned long NANO_FIRST_BYTE_TIMEOUT_MS = 700UL;
constexpr unsigned long NANO_QUIET_GAP_MS = 70UL;
constexpr unsigned long NANO_TOTAL_TIMEOUT_MS = 1200UL;

constexpr size_t MAX_COMMAND_LENGTH = 110;
constexpr size_t MAX_RESPONSE_LENGTH = 1200;
constexpr uint8_t ASYNC_LINE_COUNT = 12;
constexpr size_t ASYNC_LINE_LENGTH = 160;
}

ESP8266WebServer server(ArduConfig::HTTP_PORT);

char asyncLines[ArduConfig::ASYNC_LINE_COUNT][ArduConfig::ASYNC_LINE_LENGTH];
uint8_t asyncWriteIndex = 0;
uint8_t asyncStored = 0;

char nanoAsyncLine[ArduConfig::ASYNC_LINE_LENGTH];
size_t nanoAsyncLength = 0;

unsigned long lastWifiRetryMs = 0;
bool mdnsStarted = false;

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];

    switch (c) {
      case '\\': out += F("\\\\"); break;
      case '"': out += F("\\\""); break;
      case '\n': out += F("\\n"); break;
      case '\r': out += F("\\r"); break;
      case '\t': out += F("\\t"); break;
      default:
        if (static_cast<uint8_t>(c) >= 0x20) out += c;
        break;
    }
  }

  return out;
}

void addCommonHeaders() {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.sendHeader(
      F("Access-Control-Allow-Headers"),
      F("Content-Type")
  );
  server.sendHeader(
      F("Access-Control-Allow-Methods"),
      F("GET,POST,OPTIONS")
  );
}

void sendJson(int code, const String& body) {
  addCommonHeaders();
  server.send(code, F("application/json; charset=utf-8"), body);
}

void sendError(int code, const __FlashStringHelper* error) {
  String body = F("{\"ok\":false,\"error\":\"");
  body += error;
  body += F("\"}");
  sendJson(code, body);
}

void pushAsyncLine(const char* line) {
  if (line == nullptr || *line == '\0') return;

  strlcpy(
      asyncLines[asyncWriteIndex],
      line,
      ArduConfig::ASYNC_LINE_LENGTH
  );

  asyncWriteIndex =
      static_cast<uint8_t>(
          (asyncWriteIndex + 1) % ArduConfig::ASYNC_LINE_COUNT
      );

  if (asyncStored < ArduConfig::ASYNC_LINE_COUNT) {
    ++asyncStored;
  }
}

void drainNanoAsync() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\r') continue;

    if (c == '\n') {
      if (nanoAsyncLength > 0) {
        nanoAsyncLine[nanoAsyncLength] = '\0';
        pushAsyncLine(nanoAsyncLine);
        nanoAsyncLength = 0;
      }
      continue;
    }

    if (nanoAsyncLength < ArduConfig::ASYNC_LINE_LENGTH - 1) {
      nanoAsyncLine[nanoAsyncLength++] = c;
    } else {
      nanoAsyncLength = 0;
    }
  }
}

void clearStaleNanoInputToAsync() {
  drainNanoAsync();
}

bool commandIsSafe(const String& command) {
  if (command.length() == 0 ||
      command.length() > ArduConfig::MAX_COMMAND_LENGTH) {
    return false;
  }

  for (size_t i = 0; i < command.length(); ++i) {
    const char c = command[i];

    if (c == '\r' || c == '\n' ||
        static_cast<uint8_t>(c) < 0x20) {
      return false;
    }
  }

  return true;
}

String transactNano(
    const String& command,
    bool& gotAny,
    bool& timedOut) {
  clearStaleNanoInputToAsync();

  while (Serial.available() > 0) {
    (void)Serial.read();
  }

  Serial.print(command);
  Serial.print('\n');
  Serial.flush();

  String response;
  response.reserve(256);

  const unsigned long start = millis();
  unsigned long lastByteMs = start;
  gotAny = false;
  timedOut = false;

  while (millis() - start < ArduConfig::NANO_TOTAL_TIMEOUT_MS) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      lastByteMs = millis();
      gotAny = true;

      if (response.length() < ArduConfig::MAX_RESPONSE_LENGTH) {
        response += c;
      }
    }

    if (gotAny &&
        millis() - lastByteMs >= ArduConfig::NANO_QUIET_GAP_MS) {
      break;
    }

    if (!gotAny &&
        millis() - start >= ArduConfig::NANO_FIRST_BYTE_TIMEOUT_MS) {
      timedOut = true;
      break;
    }

    delay(1);
    yield();
  }

  if (!gotAny && millis() - start >= ArduConfig::NANO_TOTAL_TIMEOUT_MS) {
    timedOut = true;
  }

  response.trim();
  return response;
}

void handleOptions() {
  addCommonHeaders();
  server.send(204, F("text/plain"), "");
}

void handlePing() {
  String body = F("{\"ok\":true,\"device\":\"ARDU-ESP8266\"");
  body += F(",\"fw\":\"HTTP_BRIDGE_R1\"");
  body += F(",\"wifi_connected\":");
  body += WiFi.status() == WL_CONNECTED ? F("true") : F("false");

  if (WiFi.status() == WL_CONNECTED) {
    body += F(",\"ip\":\"");
    body += WiFi.localIP().toString();
    body += F("\"");
    body += F(",\"rssi\":");
    body += WiFi.RSSI();
  }

  body += F("}");
  sendJson(200, body);
}

void handleNanoCommand(const String& command) {
  if (!commandIsSafe(command)) {
    sendError(400, F("BAD_COMMAND"));
    return;
  }

  bool gotAny = false;
  bool timedOut = false;
  const String response =
      transactNano(command, gotAny, timedOut);

  String body;
  body.reserve(response.length() + command.length() + 120);

  body = F("{\"ok\":");
  body += gotAny && !timedOut ? F("true") : F("false");
  body += F(",\"command\":\"");
  body += jsonEscape(command);
  body += F("\",\"nano\":\"");
  body += jsonEscape(response);
  body += F("\",\"timed_out\":");
  body += timedOut ? F("true") : F("false");
  body += F("}");

  sendJson(gotAny ? 200 : 504, body);
}

void handleStatus() {
  handleNanoCommand(F("STATUS"));
}

void handleTime() {
  handleNanoCommand(F("TIME"));
}

void handleDevNano() {
  if (!server.hasArg(F("plain"))) {
    sendError(400, F("BODY_REQUIRED"));
    return;
  }

  String command = server.arg(F("plain"));
  command.trim();
  handleNanoCommand(command);
}

void handleEvents() {
  String body = F("{\"ok\":true,\"events\":[");

  const uint8_t count = asyncStored;
  const uint8_t first =
      static_cast<uint8_t>(
          (asyncWriteIndex + ArduConfig::ASYNC_LINE_COUNT - count) %
          ArduConfig::ASYNC_LINE_COUNT
      );

  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t index =
        static_cast<uint8_t>(
            (first + i) % ArduConfig::ASYNC_LINE_COUNT
        );

    if (i > 0) body += ',';

    body += '"';
    body += jsonEscape(String(asyncLines[index]));
    body += '"';
  }

  body += F("]}");
  sendJson(200, body);
}

void handleNotFound() {
  sendError(404, F("NOT_FOUND"));
}

void setupHttp() {
  server.on(F("/api/ping"), HTTP_GET, handlePing);
  server.on(F("/api/status"), HTTP_GET, handleStatus);
  server.on(F("/api/time"), HTTP_GET, handleTime);
  server.on(F("/api/events"), HTTP_GET, handleEvents);
  server.on(F("/api/dev/nano"), HTTP_POST, handleDevNano);

  server.on(F("/api/ping"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/status"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/time"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/events"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/dev/nano"), HTTP_OPTIONS, handleOptions);

  server.onNotFound(handleNotFound);
  server.begin();
}

bool connectWifiBlocking() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(ArduConfig::HOSTNAME);
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < ArduConfig::WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

void startMdnsIfPossible() {
  if (mdnsStarted || WiFi.status() != WL_CONNECTED) return;

  mdnsStarted = MDNS.begin(ArduConfig::HOSTNAME);

  if (mdnsStarted) {
    MDNS.addService("http", "tcp", ArduConfig::HTTP_PORT);
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    startMdnsIfPossible();
    return;
  }

  mdnsStarted = false;

  const unsigned long now = millis();

  if (now - lastWifiRetryMs < ArduConfig::WIFI_RETRY_MS) {
    return;
  }

  lastWifiRetryMs = now;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void resyncNanoAfterEspBoot() {
  // ESP8266 ROM prints boot bytes on TX before the sketch starts.
  // If Nano is already running, those bytes can leave a partial command
  // in its line buffer. A clean newline terminates/discards that partial
  // line before HTTP requests are accepted.
  Serial.print('\n');
  Serial.flush();
  delay(40);

  // Discard Nano's possible ERR UNKNOWN_COMMAND reply to boot garbage.
  while (Serial.available() > 0) {
    (void)Serial.read();
  }
}

void setup() {
  // UART0 is the Nano transport after normal boot.
  Serial.begin(ArduConfig::NANO_BAUD);
  Serial.setTimeout(50);

  delay(250);

  (void)connectWifiBlocking();

  // Nano has had enough time to leave its bootloader even on a joint cold start.
  resyncNanoAfterEspBoot();

  startMdnsIfPossible();
  setupHttp();
}

void loop() {
  maintainWifi();

  server.handleClient();

  if (mdnsStarted) {
    MDNS.update();
  }

  drainNanoAsync();

  yield();
}
