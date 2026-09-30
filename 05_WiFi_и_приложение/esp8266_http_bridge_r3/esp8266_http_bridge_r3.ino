/*
  ARDU ESP8266 HTTP BRIDGE R3

  Purpose:
  - connect only to the home Wi-Fi network (Station mode);
  - expose a small local HTTP API;
  - bridge diagnostic/user commands to Arduino Nano over UART;
  - keep the proven R1 diagnostic transport;
  - keep the R2 semantic L01 API slice;
  - enable password-protected ArduinoOTA updates over the home Wi-Fi network.

  Hardware UART:
    ESP8266 TX0 GPIO1 -> Nano D0/RX (3.3 V logic is safe for ATmega328P input)
    Nano D1/TX -> level divider -> ESP8266 RX0 GPIO3
    common GND is mandatory

  IMPORTANT:
  - ESP8266 is 3.3 V only.
  - Keep the local 100 uF capacitor at ESP VCC/GND.
  - Disconnect/reconfigure the Nano UART path while flashing the ESP as required
    by the already proven project programming procedure.
  - Before the FIRST wired upload, edit WIFI_SSID, WIFI_PASSWORD and
    OTA_PASSWORD below by hand.
  - Never commit real Wi-Fi or OTA passwords to the public repository.
  - After this R3 is installed once by wire, future ESP firmware revisions
    should normally be uploaded through ArduinoOTA over Wi-Fi.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// ---------------------------------------------------------------------------
// EDIT THESE THREE VALUES LOCALLY BEFORE THE FIRST WIRED UPLOAD.
// Do not commit your real credentials/password to the public repository.
// ---------------------------------------------------------------------------
const char* WIFI_SSID = "PUT_YOUR_WIFI_SSID_HERE";
const char* WIFI_PASSWORD = "PUT_YOUR_WIFI_PASSWORD_HERE";
const char* OTA_PASSWORD = "PUT_A_STRONG_OTA_PASSWORD_HERE";


namespace ArduConfig {
constexpr unsigned long NANO_BAUD = 115200UL;
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t OTA_PORT = 8266;
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
bool networkServicesStarted = false;
bool otaInProgress = false;

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
  body += F(",\"fw\":\"HTTP_BRIDGE_R3\"");
  body += F(",\"wifi_connected\":");
  body += WiFi.status() == WL_CONNECTED ? F("true") : F("false");

  if (WiFi.status() == WL_CONNECTED) {
    body += F(",\"ip\":\"");
    body += WiFi.localIP().toString();
    body += F("\"");
    body += F(",\"rssi\":");
    body += WiFi.RSSI();
  }

  body += F(",\"ota_ready\":");
  body += networkServicesStarted ? F("true") : F("false");
  body += F(",\"ota_hostname\":\"");
  body += ArduConfig::HOSTNAME;
  body += F("\",\"ota_port\":");
  body += ArduConfig::OTA_PORT;
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


String statusToken(const String& line, const char* key) {
  const int keyPos = line.indexOf(key);
  if (keyPos < 0) return String();

  const int start = keyPos + strlen(key);
  int end = line.indexOf(' ', start);
  if (end < 0) end = line.length();

  return line.substring(start, end);
}

bool parseLongToken(
    const String& token,
    long minValue,
    long maxValue,
    long& value) {
  if (token.length() == 0) return false;

  char* end = nullptr;
  const long parsed = strtol(token.c_str(), &end, 10);

  if (end == token.c_str() || *end != '\0' ||
      parsed < minValue || parsed > maxValue) {
    return false;
  }

  value = parsed;
  return true;
}

bool parseRgbStatus(
    const String& token,
    long& r,
    long& g,
    long& b) {
  const int comma1 = token.indexOf(',');
  if (comma1 <= 0) return false;

  const int comma2 = token.indexOf(',', comma1 + 1);
  if (comma2 <= comma1 + 1 || comma2 >= static_cast<int>(token.length()) - 1) {
    return false;
  }

  return parseLongToken(token.substring(0, comma1), 0, 255, r) &&
         parseLongToken(token.substring(comma1 + 1, comma2), 0, 255, g) &&
         parseLongToken(token.substring(comma2 + 1), 0, 255, b);
}

bool queryNanoChecked(
    const String& command,
    String& response,
    String& error) {
  bool gotAny = false;
  bool timedOut = false;

  response = transactNano(command, gotAny, timedOut);

  if (!gotAny || timedOut) {
    error = F("NANO_TIMEOUT");
    return false;
  }

  if (response.startsWith("ERR")) {
    error = F("NANO_REJECTED");
    return false;
  }

  return true;
}

void sendNanoSemanticError(
    const String& error,
    const String& nanoResponse) {
  String body = F("{\"ok\":false,\"error\":\"");
  body += jsonEscape(error);
  body += '"';

  if (nanoResponse.length() > 0) {
    body += F(",\"nano\":\"");
    body += jsonEscape(nanoResponse);
    body += '"';
  }

  body += '}';
  sendJson(error == F("NANO_TIMEOUT") ? 504 : 502, body);
}

void handleLightStatus() {
  String systemLine;
  String lightLine;
  String error;

  if (!queryNanoChecked(F("STATUS"), systemLine, error)) {
    sendNanoSemanticError(error, systemLine);
    return;
  }

  if (!queryNanoChecked(F("LIGHT STATUS"), lightLine, error)) {
    sendNanoSemanticError(error, lightLine);
    return;
  }

  const String systemMode = statusToken(systemLine, "MODE=");
  const String colorMode = statusToken(lightLine, "MODE=");
  const String kelvinToken = statusToken(lightLine, "KELVIN=");
  const String rgbToken = statusToken(lightLine, "RGB=");
  const String brightnessToken = statusToken(lightLine, "BRIGHTNESS=");
  const String savedToken = statusToken(lightLine, "SAVED=");
  const String dirtyToken = statusToken(lightLine, "DIRTY=");

  long kelvin = 0;
  long brightness = 0;
  long r = 0;
  long g = 0;
  long b = 0;

  if (systemMode.length() == 0 ||
      (colorMode != "KELVIN" && colorMode != "RGB") ||
      !parseLongToken(kelvinToken, 1800, 6500, kelvin) ||
      !parseRgbStatus(rgbToken, r, g, b) ||
      !parseLongToken(brightnessToken, 0, 255, brightness) ||
      (savedToken != "YES" && savedToken != "NO") ||
      (dirtyToken != "YES" && dirtyToken != "NO")) {
    sendError(502, F("BAD_NANO_LIGHT_STATUS"));
    return;
  }

  String body;
  body.reserve(220);

  body = F("{\"ok\":true,\"enabled\":");
  body += systemMode == "L01" ? F("true") : F("false");
  body += F(",\"color_mode\":\"");
  body += colorMode == "KELVIN" ? F("kelvin") : F("rgb");
  body += F("\",\"kelvin\":");
  body += kelvin;
  body += F(",\"rgb\":{\"r\":");
  body += r;
  body += F(",\"g\":");
  body += g;
  body += F(",\"b\":");
  body += b;
  body += F("},\"brightness\":");
  body += brightness;
  body += F(",\"saved\":");
  body += savedToken == "YES" ? F("true") : F("false");
  body += F(",\"dirty\":");
  body += dirtyToken == "YES" ? F("true") : F("false");
  body += '}';

  sendJson(200, body);
}

void handleLightSettings() {
  if (!server.hasArg(F("plain"))) {
    sendError(400, F("BODY_REQUIRED"));
    return;
  }

  String compact;
  const String input = server.arg(F("plain"));
  compact.reserve(input.length());

  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      compact += c;
    }
  }

  bool enabled = false;

  if (compact == "{\"enabled\":true}") {
    enabled = true;
  } else if (compact == "{\"enabled\":false}") {
    enabled = false;
  } else {
    sendError(400, F("R2_SUPPORTS_ENABLED_ONLY"));
    return;
  }

  String nanoResponse;
  String error;
  const String command = enabled ? F("LIGHT ON") : F("LIGHT OFF");

  if (!queryNanoChecked(command, nanoResponse, error)) {
    sendNanoSemanticError(error, nanoResponse);
    return;
  }

  const String expected = enabled ? F("OK LIGHT=ON") : F("OK LIGHT=OFF");

  if (nanoResponse != expected) {
    sendError(502, F("UNEXPECTED_NANO_ACK"));
    return;
  }

  String body = F("{\"ok\":true,\"enabled\":");
  body += enabled ? F("true") : F("false");
  body += F(",\"applied\":true}");
  sendJson(200, body);
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
  server.on(F("/api/light/status"), HTTP_GET, handleLightStatus);
  server.on(F("/api/light/settings"), HTTP_POST, handleLightSettings);
  server.on(F("/api/events"), HTTP_GET, handleEvents);
  server.on(F("/api/dev/nano"), HTTP_POST, handleDevNano);

  server.on(F("/api/ping"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/status"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/time"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/light/status"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/light/settings"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/events"), HTTP_OPTIONS, handleOptions);
  server.on(F("/api/dev/nano"), HTTP_OPTIONS, handleOptions);

  server.onNotFound(handleNotFound);
  server.begin();
}

bool credentialsLookConfigured() {
  return String(WIFI_SSID) != F("PUT_YOUR_WIFI_SSID_HERE") &&
         String(WIFI_PASSWORD) != F("PUT_YOUR_WIFI_PASSWORD_HERE") &&
         String(OTA_PASSWORD) != F("PUT_A_STRONG_OTA_PASSWORD_HERE") &&
         strlen(WIFI_SSID) > 0 &&
         strlen(WIFI_PASSWORD) > 0 &&
         strlen(OTA_PASSWORD) >= 8;
}

bool connectWifiBlocking() {
  if (!credentialsLookConfigured()) {
    return false;
  }

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

void startNetworkServicesIfPossible() {
  if (networkServicesStarted || WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setPort(ArduConfig::OTA_PORT);
  ArduinoOTA.setHostname(ArduConfig::HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // Serial is the Nano transport on ARDU. Do not print OTA progress/errors
  // to Serial, otherwise Nano would receive those lines as UART commands.
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
  });

  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
  });

  ArduinoOTA.onError([](ota_error_t) {
    otaInProgress = false;
  });

  // ArduinoOTA owns mDNS for this sketch. It advertises the OTA service
  // and ArduinoOTA.handle() also services MDNS.update().
  ArduinoOTA.begin();

  // Add our normal HTTP service to the same mDNS responder.
  MDNS.addService("http", "tcp", ArduConfig::HTTP_PORT);

  networkServicesStarted = true;
}

void stopNetworkServices() {
  if (!networkServicesStarted) return;

  ArduinoOTA.end();
  networkServicesStarted = false;
  otaInProgress = false;
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    startNetworkServicesIfPossible();
    return;
  }

  stopNetworkServices();

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

  startNetworkServicesIfPossible();
  setupHttp();
}

void loop() {
  maintainWifi();

  if (networkServicesStarted) {
    ArduinoOTA.handle();
  }

  // While an OTA image is being transferred, do not start HTTP/UART work.
  // This avoids concurrent Nano transactions and keeps the updater responsive.
  if (otaInProgress) {
    yield();
    return;
  }

  server.handleClient();
  drainNanoAsync();

  yield();
}
