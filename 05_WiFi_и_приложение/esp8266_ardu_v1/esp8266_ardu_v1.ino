/*
  ARDU ESP8266 v1 release candidate.

  External contract:
    Android / LAN -> semantic HTTP API -> ESP8266 -> compact numeric UART v1 -> Nano

  Nano compact UART v1:
    request:  <opcode> [arg0 ... arg5]\n
    ACK:      O <opcode>
    error:    E <code>
    data:     D <group> ...
    event:    V <event> [args...]

  This sketch preserves the proven ARDU Station-only Wi-Fi + ArduinoOTA transport.
  Real WIFI_SSID / WIFI_PASSWORD / OTA_PASSWORD must remain local and uncommitted.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ardu_esp_v1_types.h"

// ---------------------------------------------------------------------------
// LOCAL VALUES ONLY. Never commit real credentials to the public repository.
// ---------------------------------------------------------------------------
const char* WIFI_SSID = "PUT_YOUR_WIFI_SSID_HERE";
const char* WIFI_PASSWORD = "PUT_YOUR_WIFI_PASSWORD_HERE";
const char* OTA_PASSWORD = "PUT_A_STRONG_OTA_PASSWORD_HERE";

namespace Cfg {
constexpr unsigned long NANO_BAUD = 115200UL;
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t OTA_PORT = 8266;
constexpr char HOSTNAME[] = "ardu";
constexpr char FW_NAME[] = "ARDU_ESP_V1";
constexpr uint8_t UART_PROTOCOL = 1;

constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000UL;
constexpr unsigned long WIFI_RETRY_MS = 10000UL;

constexpr unsigned long NANO_NORMAL_TIMEOUT_MS = 1800UL;
constexpr unsigned long NANO_LONG_TIMEOUT_MS = 8000UL;
constexpr unsigned long NANO_DEV_TIMEOUT_MS = 8000UL;

constexpr size_t MAX_COMMAND_LENGTH = 80;
constexpr size_t MAX_NANO_RESPONSE = 1800;
constexpr size_t MAX_NANO_LINE = 192;

constexpr uint8_t EVENT_COUNT = 16;
constexpr size_t EVENT_LINE_LENGTH = 80;
}

ESP8266WebServer server(Cfg::HTTP_PORT);

char eventLines[Cfg::EVENT_COUNT][Cfg::EVENT_LINE_LENGTH];
uint8_t eventWrite = 0;
uint8_t eventStored = 0;

char asyncLine[Cfg::MAX_NANO_LINE];
size_t asyncLength = 0;

unsigned long lastWifiRetryMs = 0;
bool networkServicesStarted = false;
bool otaInProgress = false;

// ---------------------------------------------------------------------------
// HTTP / JSON helpers
// ---------------------------------------------------------------------------

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 12);
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
  server.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
  server.sendHeader(F("Access-Control-Allow-Methods"), F("GET,POST,OPTIONS"));
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

void handleOptions() {
  addCommonHeaders();
  server.send(204, F("text/plain"), "");
}

int jsonValueStart(const String& input, const char* key) {
  String needle;
  needle.reserve(strlen(key) + 3);
  needle += '"';
  needle += key;
  needle += '"';

  int p = input.indexOf(needle);
  if (p < 0) return -1;
  p = input.indexOf(':', p + needle.length());
  if (p < 0) return -2;
  ++p;
  while (p < static_cast<int>(input.length()) &&
         (input[p] == ' ' || input[p] == '\t' ||
          input[p] == '\r' || input[p] == '\n')) {
    ++p;
  }
  return p;
}

JsonRead jsonLong(
    const String& input,
    const char* key,
    long minValue,
    long maxValue,
    long& value) {
  const int start = jsonValueStart(input, key);
  if (start == -1) return JSON_MISSING;
  if (start < 0 || start >= static_cast<int>(input.length())) return JSON_BAD;

  char* end = nullptr;
  const long v = strtol(input.c_str() + start, &end, 10);
  if (end == input.c_str() + start || v < minValue || v > maxValue) {
    return JSON_BAD;
  }
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
  if (*end != ',' && *end != '}' && *end != '\0') return JSON_BAD;
  value = v;
  return JSON_OK;
}

JsonRead jsonDouble(
    const String& input,
    const char* key,
    double minValue,
    double maxValue,
    double& value) {
  const int start = jsonValueStart(input, key);
  if (start == -1) return JSON_MISSING;
  if (start < 0 || start >= static_cast<int>(input.length())) return JSON_BAD;

  char* end = nullptr;
  const double v = strtod(input.c_str() + start, &end);
  if (end == input.c_str() + start || v < minValue || v > maxValue) {
    return JSON_BAD;
  }
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
  if (*end != ',' && *end != '}' && *end != '\0') return JSON_BAD;
  value = v;
  return JSON_OK;
}

JsonRead jsonBool(const String& input, const char* key, bool& value) {
  const int start = jsonValueStart(input, key);
  if (start == -1) return JSON_MISSING;
  if (start < 0) return JSON_BAD;

  if (input.substring(start, start + 4) == F("true")) {
    value = true;
    return JSON_OK;
  }
  if (input.substring(start, start + 5) == F("false")) {
    value = false;
    return JSON_OK;
  }
  return JSON_BAD;
}

JsonRead jsonString(
    const String& input,
    const char* key,
    String& value) {
  const int start = jsonValueStart(input, key);
  if (start == -1) return JSON_MISSING;
  if (start < 0 || start >= static_cast<int>(input.length()) ||
      input[start] != '"') {
    return JSON_BAD;
  }

  const int end = input.indexOf('"', start + 1);
  if (end < 0) return JSON_BAD;
  value = input.substring(start + 1, end);
  return JSON_OK;
}

JsonRead jsonRgb(
    const String& input,
    long& r,
    long& g,
    long& b) {
  const int start = jsonValueStart(input, "rgb");
  if (start == -1) return JSON_MISSING;
  if (start < 0 || start >= static_cast<int>(input.length()) ||
      input[start] != '{') {
    return JSON_BAD;
  }
  const int end = input.indexOf('}', start + 1);
  if (end < 0) return JSON_BAD;
  const String inner = input.substring(start, end + 1);

  if (jsonLong(inner, "r", 0, 255, r) != JSON_OK ||
      jsonLong(inner, "g", 0, 255, g) != JSON_OK ||
      jsonLong(inner, "b", 0, 255, b) != JSON_OK) {
    return JSON_BAD;
  }
  return JSON_OK;
}

bool parseClock(const String& text, uint8_t& h, uint8_t& m) {
  if (text.length() != 5 || text[2] != ':') return false;
  if (!isDigit(text[0]) || !isDigit(text[1]) ||
      !isDigit(text[3]) || !isDigit(text[4])) return false;
  h = static_cast<uint8_t>((text[0] - '0') * 10 + (text[1] - '0'));
  m = static_cast<uint8_t>((text[3] - '0') * 10 + (text[4] - '0'));
  return h <= 23 && m <= 59;
}

String twoDigits(uint32_t v) {
  String s;
  if (v < 10) s += '0';
  s += v;
  return s;
}

// ---------------------------------------------------------------------------
// Async event buffer
// ---------------------------------------------------------------------------

void pushEventLine(const char* line) {
  if (!line || !*line) return;
  strlcpy(eventLines[eventWrite], line, Cfg::EVENT_LINE_LENGTH);
  eventWrite = static_cast<uint8_t>((eventWrite + 1) % Cfg::EVENT_COUNT);
  if (eventStored < Cfg::EVENT_COUNT) ++eventStored;
}

bool isAsyncNanoLine(const String& line) {
  return line.startsWith(F("V ")) || line.startsWith(F("ARDU1 "));
}

void drainNanoAsync() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;

    if (c == '\n') {
      if (asyncLength) {
        asyncLine[asyncLength] = '\0';
        pushEventLine(asyncLine);
        asyncLength = 0;
      }
      continue;
    }

    if (asyncLength < Cfg::MAX_NANO_LINE - 1) {
      asyncLine[asyncLength++] = c;
    } else {
      asyncLength = 0;
    }
  }
}

// ---------------------------------------------------------------------------
// Compact UART v1
// ---------------------------------------------------------------------------

bool safeRawCommand(const String& command) {
  if (!command.length() || command.length() > Cfg::MAX_COMMAND_LENGTH) return false;
  for (size_t i = 0; i < command.length(); ++i) {
    const char c = command[i];
    if (c == '\r' || c == '\n' || static_cast<uint8_t>(c) < 0x20) return false;
  }
  return true;
}

bool parseLeadingU32(const String& line, uint32_t& value, int& nextPos) {
  int p = 0;
  while (p < static_cast<int>(line.length()) && line[p] == ' ') ++p;
  if (p >= static_cast<int>(line.length()) || !isDigit(line[p])) return false;

  uint32_t v = 0;
  while (p < static_cast<int>(line.length()) && isDigit(line[p])) {
    v = v * 10UL + static_cast<uint8_t>(line[p] - '0');
    ++p;
  }
  value = v;
  nextPos = p;
  return true;
}

bool lineAckMatches(const String& line, uint16_t op) {
  if (!line.startsWith(F("O "))) return false;
  uint32_t v = 0;
  int next = 0;
  return parseLeadingU32(line.substring(2), v, next) && v == op;
}

bool lineDataMatches(const String& line, uint16_t op) {
  if (!line.startsWith(F("D "))) return false;
  uint32_t v = 0;
  int next = 0;
  return parseLeadingU32(line.substring(2), v, next) && v == op;
}

uint8_t nanoErrorCode(const String& line) {
  if (!line.startsWith(F("E "))) return 0;
  uint32_t v = 0;
  int next = 0;
  if (!parseLeadingU32(line.substring(2), v, next) || v > 255) return 1;
  return static_cast<uint8_t>(v);
}

void appendResponseLine(String& response, const String& line) {
  if (response.length()) response += '\n';
  if (response.length() + line.length() <= Cfg::MAX_NANO_RESPONSE) {
    response += line;
  }
}

NanoResult transactNano(
    const String& command,
    uint16_t expectedOp,
    bool expectAck,
    unsigned long totalTimeoutMs) {
  NanoResult result;
  result.response.reserve(900);

  drainNanoAsync();
  while (Serial.available() > 0) (void)Serial.read();

  Serial.print(command);
  Serial.print('\n');
  Serial.flush();

  String line;
  line.reserve(Cfg::MAX_NANO_LINE);

  const unsigned long start = millis();

  while (millis() - start < totalTimeoutMs) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\r') continue;

      if (c != '\n') {
        if (line.length() < Cfg::MAX_NANO_LINE - 1) line += c;
        continue;
      }

      line.trim();
      if (!line.length()) {
        line = "";
        continue;
      }

      if (isAsyncNanoLine(line)) {
        pushEventLine(line.c_str());
        line = "";
        continue;
      }

      appendResponseLine(result.response, line);

      const uint8_t err = nanoErrorCode(line);
      if (err) {
        result.errorCode = err;
        return result;
      }

      if (expectAck) {
        if (lineAckMatches(line, expectedOp)) {
          result.ok = true;
          return result;
        }
      } else if (lineDataMatches(line, expectedOp)) {
        result.ok = true;
        return result;
      }

      line = "";
    }

    delay(1);
    yield();
  }

  result.timedOut = true;
  return result;
}

String makeCommand(uint16_t op) {
  return String(op);
}

String makeCommand(uint16_t op, long a) {
  String c(op);
  c += ' ';
  c += a;
  return c;
}

String makeCommand(uint16_t op, long a, long b) {
  String c(op);
  c += ' '; c += a;
  c += ' '; c += b;
  return c;
}

String makeCommand(uint16_t op, long a, long b, long c0) {
  String c(op);
  c += ' '; c += a;
  c += ' '; c += b;
  c += ' '; c += c0;
  return c;
}

String makeCommand(
    uint16_t op,
    long a,
    long b,
    long c0,
    long d) {
  String c(op);
  c += ' '; c += a;
  c += ' '; c += b;
  c += ' '; c += c0;
  c += ' '; c += d;
  return c;
}

String makeCommand6(
    uint16_t op,
    long a,
    long b,
    long c0,
    long d,
    long e,
    long f) {
  String c(op);
  c += ' '; c += a;
  c += ' '; c += b;
  c += ' '; c += c0;
  c += ' '; c += d;
  c += ' '; c += e;
  c += ' '; c += f;
  return c;
}

NanoResult nanoAck(uint16_t op, const String& command) {
  return transactNano(command, op, true, Cfg::NANO_NORMAL_TIMEOUT_MS);
}

NanoResult nanoData(
    uint16_t op,
    const String& command,
    unsigned long timeoutMs = Cfg::NANO_NORMAL_TIMEOUT_MS) {
  return transactNano(command, op, false, timeoutMs);
}

NanoResult nanoSettings() {
  return transactNano(F("3"), 3, true, Cfg::NANO_NORMAL_TIMEOUT_MS);
}

int nanoHttpStatus(const NanoResult& r) {
  if (r.timedOut) return 504;
  if (!r.errorCode) return 502;
  switch (r.errorCode) {
    case 1: return 400; // parse
    case 2: return 400; // range
    case 3: return 503; // RTC
    case 4: return 409; // state
    case 5: return 422; // applicability
    default: return 502;
  }
}

void sendNanoFailure(const NanoResult& r) {
  String body = F("{\"ok\":false,\"error\":\"");
  if (r.timedOut) body += F("NANO_TIMEOUT");
  else if (r.errorCode) body += F("NANO_REJECTED");
  else body += F("NANO_BAD_RESPONSE");
  body += '"';
  body += F(",\"nano_error_code\":");
  body += r.errorCode;
  body += F(",\"nano\":\"");
  body += jsonEscape(r.response);
  body += F("\"}");
  sendJson(nanoHttpStatus(r), body);
}

bool runAck(uint16_t op, const String& command) {
  const NanoResult r = nanoAck(op, command);
  if (!r.ok) {
    sendNanoFailure(r);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Compact data parsing
// ---------------------------------------------------------------------------

bool nextU32Token(const String& line, int& pos, uint32_t& out) {
  while (pos < static_cast<int>(line.length()) && line[pos] == ' ') ++pos;
  if (pos >= static_cast<int>(line.length()) || !isDigit(line[pos])) return false;
  uint32_t v = 0;
  while (pos < static_cast<int>(line.length()) && isDigit(line[pos])) {
    v = v * 10UL + static_cast<uint8_t>(line[pos] - '0');
    ++pos;
  }
  out = v;
  return true;
}

bool parseDataLine(
    const String& response,
    uint16_t group,
    int item,
    uint32_t* values,
    uint8_t maxValues,
    uint8_t& count) {
  int start = 0;

  while (start <= static_cast<int>(response.length())) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();

    String line = response.substring(start, end);
    line.trim();

    if (line.startsWith(F("D "))) {
      int pos = 2;
      uint32_t g = 0;
      if (nextU32Token(line, pos, g) && g == group) {
        if (item >= 0) {
          uint32_t it = 0;
          if (!nextU32Token(line, pos, it) ||
              it != static_cast<uint32_t>(item)) {
            start = end + 1;
            continue;
          }
        }

        count = 0;
        uint32_t v = 0;
        while (count < maxValues && nextU32Token(line, pos, v)) {
          values[count++] = v;
        }
        return true;
      }
    }

    if (end >= static_cast<int>(response.length())) break;
    start = end + 1;
  }

  return false;
}

const char* modeName(uint32_t mode) {
  switch (mode) {
    case 0: return "off";
    case 1: return "light";
    case 2: return "night";
    case 3: return "dawn";
    case 4: return "ambient";
    case 5: return "music";
    default: return "unknown";
  }
}

const char* musicId(uint32_t id) {
  static const char* ids[] = {"M01","M02","M03","M04","M05","M08","M09"};
  return id < 7 ? ids[id] : "M01";
}

const char* ambientId(uint32_t id) {
  static const char* ids[] = {"F01","F02","F03"};
  return id < 3 ? ids[id] : "F01";
}

const char* dawnPhase(uint32_t p) {
  switch (p) {
    case 0: return "idle";
    case 1: return "running";
    case 2: return "hold";
    default: return "unknown";
  }
}

bool parseStatusValues(
    const NanoResult& r,
    uint32_t* v,
    uint8_t& n) {
  return r.ok && parseDataLine(r.response, 2, -1, v, 12, n) && n >= 10;
}

bool parseTimeValues(
    const NanoResult& r,
    uint32_t* v,
    uint8_t& n) {
  return r.ok && parseDataLine(r.response, 4, -1, v, 8, n) && n >= 1;
}

// ---------------------------------------------------------------------------
// Base endpoints
// ---------------------------------------------------------------------------

void handlePing() {
  String body = F("{\"ok\":true,\"device\":\"ARDU-ESP8266\",\"fw\":\"");
  body += Cfg::FW_NAME;
  body += F("\",\"uart_protocol\":");
  body += Cfg::UART_PROTOCOL;
  body += F(",\"wifi_connected\":");
  body += WiFi.status() == WL_CONNECTED ? F("true") : F("false");

  if (WiFi.status() == WL_CONNECTED) {
    body += F(",\"ip\":\"");
    body += WiFi.localIP().toString();
    body += F("\",\"rssi\":");
    body += WiFi.RSSI();
  }

  body += F(",\"ota_ready\":");
  body += networkServicesStarted ? F("true") : F("false");
  body += F(",\"ota_hostname\":\"");
  body += Cfg::HOSTNAME;
  body += F("\",\"ota_port\":");
  body += Cfg::OTA_PORT;
  body += '}';
  sendJson(200, body);
}

void handleStatus() {
  const NanoResult sr = nanoData(2, F("2"));
  if (!sr.ok) {
    sendNanoFailure(sr);
    return;
  }

  uint32_t s[12];
  uint8_t sn = 0;
  if (!parseStatusValues(sr, s, sn)) {
    sendError(502, F("BAD_NANO_STATUS"));
    return;
  }

  const NanoResult tr = nanoData(4, F("4"));
  uint32_t t[8];
  uint8_t tn = 0;
  const bool timeParsed = parseTimeValues(tr, t, tn);

  String body;
  body.reserve(520);
  body = F("{\"ok\":true,\"nano_fw\":\"ARDU_V1\",\"uart_protocol\":1");
  body += F(",\"mode\":\""); body += modeName(s[0]); body += '"';
  body += F(",\"mode_id\":"); body += s[0];
  body += F(",\"rtc_valid\":"); body += s[1] ? F("true") : F("false");
  body += F(",\"clap_enabled\":"); body += s[2] ? F("true") : F("false");
  body += F(",\"clap_threshold\":"); body += s[3];
  body += F(",\"clap_timeout_ms\":"); body += s[4];
  body += F(",\"current_limit_ma\":"); body += s[5];
  body += F(",\"music_mode\":\""); body += musicId(s[6]); body += '"';
  body += F(",\"ambient_effect\":\""); body += ambientId(s[7]); body += '"';
  body += F(",\"reset_flags\":"); body += s[8];
  body += F(",\"uptime_ms\":"); body += s[9];

  if (timeParsed && t[0] && tn >= 7) {
    body += F(",\"rtc_time\":\"");
    body += t[1]; body += '-'; body += twoDigits(t[2]); body += '-'; body += twoDigits(t[3]);
    body += ' '; body += twoDigits(t[4]); body += ':'; body += twoDigits(t[5]); body += ':'; body += twoDigits(t[6]);
    body += '"';
  }

  body += F(",\"nano\":\"");
  body += jsonEscape(sr.response);
  body += F("\"}");
  sendJson(200, body);
}

void handleTime() {
  const NanoResult r = nanoData(4, F("4"));
  if (!r.ok) {
    sendNanoFailure(r);
    return;
  }

  uint32_t v[8];
  uint8_t n = 0;
  if (!parseTimeValues(r, v, n)) {
    sendError(502, F("BAD_NANO_TIME"));
    return;
  }

  String body = F("{\"ok\":true,\"valid\":");
  body += v[0] ? F("true") : F("false");

  if (v[0] && n >= 7) {
    body += F(",\"date\":\"");
    body += v[1]; body += '-'; body += twoDigits(v[2]); body += '-'; body += twoDigits(v[3]);
    body += F("\",\"time\":\"");
    body += twoDigits(v[4]); body += ':'; body += twoDigits(v[5]); body += ':'; body += twoDigits(v[6]);
    body += '"';
  }

  body += F(",\"nano\":\"");
  body += jsonEscape(r.response);
  body += F("\"}");
  sendJson(200, body);
}

void handleTimeSync() {
  if (!server.hasArg(F("plain"))) {
    sendError(400, F("BODY_REQUIRED"));
    return;
  }
  const String body = server.arg(F("plain"));

  long y,m,d,hh,mm,ss;
  if (jsonLong(body,"year",2000,2099,y)!=JSON_OK ||
      jsonLong(body,"month",1,12,m)!=JSON_OK ||
      jsonLong(body,"day",1,31,d)!=JSON_OK ||
      jsonLong(body,"hour",0,23,hh)!=JSON_OK ||
      jsonLong(body,"minute",0,59,mm)!=JSON_OK ||
      jsonLong(body,"second",0,59,ss)!=JSON_OK) {
    sendError(400, F("BAD_TIME"));
    return;
  }

  const String cmd = makeCommand6(5,y,m,d,hh,mm,ss);
  if (!runAck(5,cmd)) return;
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handlePower() {
  if (!server.hasArg(F("plain"))) {
    sendError(400,F("BODY_REQUIRED"));
    return;
  }
  bool on = false;
  if (jsonBool(server.arg(F("plain")),"on",on)!=JSON_OK) {
    sendError(400,F("BAD_POWER"));
    return;
  }
  if (!runAck(20,makeCommand(20,on?1:0))) return;
  String body=F("{\"ok\":true,\"on\":");
  body+=on?F("true"):F("false");
  body+=F(",\"applied\":true}");
  sendJson(200,body);
}

void handleMode() {
  if (!server.hasArg(F("plain"))) {
    sendError(400,F("BODY_REQUIRED"));
    return;
  }
  String mode;
  if (jsonString(server.arg(F("plain")),"mode",mode)!=JSON_OK) {
    sendError(400,F("BAD_MODE"));
    return;
  }

  int code=-1;
  if(mode=="off")code=0;
  else if(mode=="light")code=1;
  else if(mode=="night")code=2;
  else if(mode=="ambient")code=3;
  else if(mode=="music")code=4;

  if(code<0){
    sendError(400,F("BAD_MODE"));
    return;
  }
  if(!runAck(10,makeCommand(10,code)))return;
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

// ---------------------------------------------------------------------------
// Full settings JSON
// ---------------------------------------------------------------------------

bool getSettingLine(
    const NanoResult& settings,
    int item,
    uint32_t* v,
    uint8_t max,
    uint8_t& n) {
  return parseDataLine(settings.response,3,item,v,max,n);
}

void appendMusicObject(
    String& body,
    const char* id,
    const uint32_t* v) {
  body += '"'; body += id; body += F("\":{\"brightness\":"); body += v[0];
  body += F(",\"background_brightness\":"); body += v[1];
  body += F(",\"smoothing\":"); body += v[2];
  body += F(",\"sensitivity\":"); body += v[3];
  body += F(",\"speed\":"); body += v[4];
  body += F(",\"aux\":"); body += v[5];
  body += F(",\"submode\":"); body += v[6];
  body += '}';
}

void handleSettings() {
  const NanoResult sr = nanoSettings();
  if (!sr.ok) {
    sendNanoFailure(sr);
    return;
  }
  const NanoResult status = nanoData(2,F("2"));
  if (!status.ok) {
    sendNanoFailure(status);
    return;
  }

  uint32_t sv[12];
  uint8_t sn=0;
  if(!parseStatusValues(status,sv,sn)){
    sendError(502,F("BAD_NANO_STATUS"));
    return;
  }

  uint32_t l[12],c[12],n[12],a[12],am[18],sys[8],music[7][8];
  uint8_t ln=0,cn=0,nn=0,an=0,amn=0,sysn=0,mn[7]={0};

  if(!getSettingLine(sr,0,l,12,ln)||ln<8 ||
     !getSettingLine(sr,1,c,12,cn)||cn<10 ||
     !getSettingLine(sr,2,n,12,nn)||nn<9 ||
     !getSettingLine(sr,3,a,12,an)||an<9 ||
     !getSettingLine(sr,4,am,18,amn)||amn<14 ||
     !getSettingLine(sr,20,sys,8,sysn)||sysn<5){
    sendError(502,F("BAD_NANO_SETTINGS"));
    return;
  }

  for(uint8_t i=0;i<7;++i){
    if(!getSettingLine(sr,10+i,music[i],8,mn[i])||mn[i]<7){
      sendError(502,F("BAD_NANO_MUSIC_SETTINGS"));
      return;
    }
  }

  String body;
  body.reserve(3000);
  body=F("{\"ok\":true,\"schema\":1,\"mode\":\"");
  body+=modeName(sv[0]);
  body+=F("\",\"light\":{\"color_mode\":\"");
  body+=l[0]==0?F("kelvin"):F("rgb");
  body+=F("\",\"kelvin\":");body+=l[1];
  body+=F(",\"rgb\":{\"r\":");body+=l[2];body+=F(",\"g\":");body+=l[3];body+=F(",\"b\":");body+=l[4];
  body+=F("},\"brightness\":");body+=l[5];
  body+=F(",\"saved\":");body+=l[6]?F("true"):F("false");
  body+=F(",\"dirty\":");body+=l[7]?F("true"):F("false");
  body+=F("},\"clap\":{\"enabled\":");body+=c[0]?F("true"):F("false");
  body+=F(",\"threshold\":");body+=c[1];body+=F(",\"timeout_ms\":");body+=c[2];
  body+=F(",\"saved\":");body+=c[3]?F("true"):F("false");
  body+=F(",\"calibration\":{\"active\":");body+=c[4]?F("true"):F("false");
  body+=F(",\"finished\":");body+=c[5]?F("true"):F("false");
  body+=F(",\"good_pairs\":");body+=c[6];body+=F(",\"target_pairs\":");body+=c[7];
  body+=F(",\"quiet_p99_derivative\":");body+=c[8];body+=F(",\"suggested_threshold\":");body+=c[9];
  body+=F("}},\"night\":{\"enabled\":");body+=n[0]?F("true"):F("false");
  body+=F(",\"hue\":");body+=n[1];body+=F(",\"saturation\":");body+=n[2];body+=F(",\"brightness\":");body+=n[3];
  body+=F(",\"schedule_enabled\":");body+=n[4]?F("true"):F("false");
  body+=F(",\"schedule_on\":\"");body+=twoDigits(n[5]);body+=':';body+=twoDigits(n[6]);
  body+=F("\",\"schedule_off\":\"");body+=twoDigits(n[7]);body+=':';body+=twoDigits(n[8]);body+='"';
  body+=F("},\"alarm\":{\"enabled\":");body+=a[0]?F("true"):F("false");
  body+=F(",\"hour\":");body+=a[1];body+=F(",\"minute\":");body+=a[2];
  body+=F(",\"fade_minutes\":");body+=a[3];body+=F(",\"max_brightness\":");body+=a[4];
  body+=F(",\"start_hue\":");body+=a[5];body+=F(",\"end_hue\":");body+=a[6];
  body+=F(",\"dawn_phase\":\"");body+=dawnPhase(a[7]);body+=F("\",\"recovered\":");body+=a[8]?F("true"):F("false");
  body+=F("},\"ambient\":{\"effect\":\"");body+=ambientId(am[0]);body+='"';
  body+=F(",\"auto_cycle\":");body+=am[1]?F("true"):F("false");body+=F(",\"auto_period_s\":");body+=am[2];
  body+=F(",\"F01\":{\"hue\":");body+=am[3];body+=F(",\"saturation\":");body+=am[4];body+=F(",\"brightness\":");body+=am[5];body+='}';
  body+=F(",\"F02\":{\"hue\":");body+=am[6];body+=F(",\"saturation\":");body+=am[7];body+=F(",\"brightness\":");body+=am[8];body+=F(",\"speed\":");body+=am[9];body+='}';
  body+=F(",\"F03\":{\"hue\":");body+=am[10];body+=F(",\"brightness\":");body+=am[11];body+=F(",\"speed\":");body+=am[12];
  body+=F(",\"rainbow_step\":");body+=String(static_cast<double>(am[13])/10.0,1);body+=F("}},\"music\":{\"selected\":\"");body+=musicId(sv[6]);body+=F("\",\"modes\":{");

  for(uint8_t i=0;i<7;++i){
    if(i)body+=',';
    appendMusicObject(body,musicId(i),music[i]);
  }

  body+=F("}},\"system\":{\"current_limit_ma\":");body+=sys[0];
  body+=F(",\"audio_calibrated\":");body+=sys[1]?F("true"):F("false");
  body+=F(",\"mic_dc\":");body+=sys[2];body+=F(",\"vu_low_pass\":");body+=sys[3];
  body+=F(",\"spectrum_low_pass\":");body+=sys[4];body+=F("}}");

  sendJson(200,body);
}

// ---------------------------------------------------------------------------
// Light + clap
// ---------------------------------------------------------------------------

void handleLightStatus() {
  const NanoResult st=nanoData(2,F("2"));
  const NanoResult sr=nanoSettings();
  if(!st.ok){sendNanoFailure(st);return;}
  if(!sr.ok){sendNanoFailure(sr);return;}

  uint32_t s[12],l[12],c[12];
  uint8_t sn=0,ln=0,cn=0;
  if(!parseStatusValues(st,s,sn) ||
     !getSettingLine(sr,0,l,12,ln)||ln<8 ||
     !getSettingLine(sr,1,c,12,cn)||cn<10){
    sendError(502,F("BAD_NANO_LIGHT_STATUS"));
    return;
  }

  String body=F("{\"ok\":true,\"enabled\":");
  body+=(s[0]==1)?F("true"):F("false");
  body+=F(",\"color_mode\":\"");body+=l[0]==0?F("kelvin"):F("rgb");
  body+=F("\",\"kelvin\":");body+=l[1];
  body+=F(",\"rgb\":{\"r\":");body+=l[2];body+=F(",\"g\":");body+=l[3];body+=F(",\"b\":");body+=l[4];
  body+=F("},\"brightness\":");body+=l[5];
  body+=F(",\"saved\":");body+=l[6]?F("true"):F("false");
  body+=F(",\"dirty\":");body+=l[7]?F("true"):F("false");
  body+=F(",\"clap_enabled\":");body+=c[0]?F("true"):F("false");
  body+='}';
  sendJson(200,body);
}

void handleLightSettings() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  const String body=server.arg(F("plain"));
  bool any=false;

  bool enabled=false;
  JsonRead jr=jsonBool(body,"enabled",enabled);
  if(jr==JSON_BAD){sendError(400,F("BAD_ENABLED"));return;}
  if(jr==JSON_OK){if(!runAck(20,makeCommand(20,enabled?1:0)))return;any=true;}

  long v=0;
  jr=jsonLong(body,"brightness",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_BRIGHTNESS"));return;}
  if(jr==JSON_OK){if(!runAck(21,makeCommand(21,v)))return;any=true;}

  jr=jsonLong(body,"kelvin",1800,6500,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_KELVIN"));return;}
  if(jr==JSON_OK){if(!runAck(22,makeCommand(22,v)))return;any=true;}

  long r=0,g=0,b=0;
  jr=jsonRgb(body,r,g,b);
  if(jr==JSON_BAD){sendError(400,F("BAD_RGB"));return;}
  if(jr==JSON_OK){if(!runAck(23,makeCommand(23,r,g,b)))return;any=true;}

  bool clap=false;
  jr=jsonBool(body,"clap_enabled",clap);
  if(jr==JSON_BAD){sendError(400,F("BAD_CLAP_ENABLED"));return;}
  if(jr==JSON_OK){if(!runAck(26,makeCommand(26,clap?1:0)))return;any=true;}

  bool persist=false;
  jr=jsonBool(body,"persist_startup_profile",persist);
  if(jr==JSON_BAD){sendError(400,F("BAD_PERSIST"));return;}
  if(jr==JSON_OK && persist){if(!runAck(24,F("24")))return;any=true;}

  if(!any){sendError(400,F("NO_SUPPORTED_FIELDS"));return;}
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handleClapCalibrationStatus() {
  const NanoResult r=nanoData(45,F("45"));
  if(!r.ok){sendNanoFailure(r);return;}
  uint32_t v[8];uint8_t n=0;
  if(!parseDataLine(r.response,45,0,v,8,n)||n<6){
    sendError(502,F("BAD_CLAPCAL_STATUS"));return;
  }
  String body=F("{\"ok\":true,\"active\":");body+=v[0]?F("true"):F("false");
  body+=F(",\"finished\":");body+=v[1]?F("true"):F("false");
  body+=F(",\"good_pairs\":");body+=v[2];body+=F(",\"target_pairs\":");body+=v[3];
  body+=F(",\"quiet_p99_derivative\":");body+=v[4];body+=F(",\"suggested_threshold\":");body+=v[5];body+='}';
  sendJson(200,body);
}

void handleClapCalibrationStart() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  long pairs=0;
  if(jsonLong(server.arg(F("plain")),"pairs",3,12,pairs)!=JSON_OK){
    sendError(400,F("BAD_PAIRS"));return;
  }
  const NanoResult r=nanoData(40,makeCommand(40,pairs),Cfg::NANO_LONG_TIMEOUT_MS);
  if(!r.ok){sendNanoFailure(r);return;}
  uint32_t v[5];uint8_t n=0;
  if(!parseDataLine(r.response,40,-1,v,5,n)||n<3){
    sendError(502,F("BAD_CLAPCAL_START"));return;
  }
  String body=F("{\"ok\":true,\"quiet_p99_derivative\":");body+=v[1];
  body+=F(",\"target_pairs\":");body+=v[2];body+='}';
  sendJson(200,body);
}

void handleClapCalibrationSample() {
  const NanoResult r=nanoData(41,F("41"),Cfg::NANO_LONG_TIMEOUT_MS);
  if(!r.ok){sendNanoFailure(r);return;}
  uint32_t v[6];uint8_t n=0;
  if(!parseDataLine(r.response,41,-1,v,6,n)||n<2){
    sendError(502,F("BAD_CLAPCAL_SAMPLE"));return;
  }
  String body=F("{\"ok\":true,\"accepted\":");body+=v[0]?F("true"):F("false");
  if(v[0]&&n>=5){
    body+=F(",\"good_pairs\":");body+=v[1];body+=F(",\"target_pairs\":");body+=v[2];
    body+=F(",\"weak_strength\":");body+=v[3];body+=F(",\"strong_strength\":");body+=v[4];
  }else{
    body+=F(",\"detected_claps\":");body+=v[1];
  }
  body+='}';
  sendJson(200,body);
}

void handleClapCalibrationFinish() {
  const NanoResult r=nanoData(42,F("42"),Cfg::NANO_LONG_TIMEOUT_MS);
  if(!r.ok){sendNanoFailure(r);return;}
  uint32_t v[6];uint8_t n=0;
  if(!parseDataLine(r.response,42,-1,v,6,n)||n<4){
    sendError(502,F("BAD_CLAPCAL_RESULT"));return;
  }
  String body=F("{\"ok\":true,\"quiet_p99_derivative\":");body+=v[1];
  body+=F(",\"clap_p20\":");body+=v[2];body+=F(",\"suggested_threshold\":");body+=v[3];body+='}';
  sendJson(200,body);
}

void handleClapCalibrationSave() {
  if(!runAck(43,F("43")))return;
  sendJson(200,F("{\"ok\":true,\"saved\":true}"));
}

void handleClapCalibrationCancel() {
  if(!runAck(44,F("44")))return;
  sendJson(200,F("{\"ok\":true,\"cancelled\":true}"));
}

// ---------------------------------------------------------------------------
// Music
// ---------------------------------------------------------------------------

int musicCode(const String& id) {
  if(id=="M01")return 0;if(id=="M02")return 1;if(id=="M03")return 2;
  if(id=="M04")return 3;if(id=="M05")return 4;if(id=="M08")return 5;
  if(id=="M09")return 6;return -1;
}

void handleMusicMode() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  String id;
  if(jsonString(server.arg(F("plain")),"id",id)!=JSON_OK){
    sendError(400,F("BAD_MUSIC_MODE"));return;
  }
  const int code=musicCode(id);
  if(code<0){sendError(400,F("BAD_MUSIC_MODE"));return;}
  if(!runAck(80,makeCommand(80,code)))return;
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handleMusicSettings() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  const String body=server.arg(F("plain"));
  bool any=false;
  long v=0;
  JsonRead jr;

  jr=jsonLong(body,"active_brightness",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_BRIGHTNESS"));return;}
  if(jr==JSON_OK){if(!runAck(81,makeCommand(81,v)))return;any=true;}

  jr=jsonLong(body,"background_brightness",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_BACKGROUND"));return;}
  if(jr==JSON_OK){if(!runAck(82,makeCommand(82,v)))return;any=true;}

  jr=jsonLong(body,"smoothing",5,100,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_SMOOTHING"));return;}
  if(jr==JSON_OK){if(!runAck(83,makeCommand(83,v)))return;any=true;}

  jr=jsonLong(body,"sensitivity",50,200,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_SENSITIVITY"));return;}
  if(jr==JSON_OK){if(!runAck(84,makeCommand(84,v)))return;any=true;}

  String sub;
  jr=jsonString(body,"submode",sub);
  if(jr==JSON_BAD){sendError(400,F("BAD_SUBMODE"));return;}
  if(jr==JSON_OK){
    int sm=-1;
    if(sub=="three")sm=0;else if(sub=="low")sm=1;else if(sub=="mid")sm=2;else if(sub=="high")sm=3;
    if(sm<0){sendError(400,F("BAD_SUBMODE"));return;}
    if(!runAck(85,makeCommand(85,sm)))return;any=true;
  }

  jr=jsonLong(body,"speed",1,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_SPEED"));return;}
  if(jr==JSON_OK){if(!runAck(86,makeCommand(86,v)))return;any=true;}

  jr=jsonLong(body,"rainbow_step10",5,200,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_RAINBOW_STEP"));return;}
  if(jr==JSON_OK){if(!runAck(87,makeCommand(87,v)))return;any=true;}

  jr=jsonLong(body,"hue_step",1,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_HUE_STEP"));return;}
  if(jr==JSON_OK){if(!runAck(87,makeCommand(87,v)))return;any=true;}

  jr=jsonLong(body,"hue_start",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_HUE_START"));return;}
  if(jr==JSON_OK){if(!runAck(88,makeCommand(88,v)))return;any=true;}

  bool persist=true;
  jr=jsonBool(body,"persist",persist);
  if(jr==JSON_BAD){sendError(400,F("BAD_PERSIST"));return;}
  if(persist && any){if(!runAck(89,F("89")))return;}

  if(!any){sendError(400,F("NO_SUPPORTED_FIELDS"));return;}
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handleMusicCalibrate() {
  const NanoResult r=nanoData(90,F("90"),Cfg::NANO_LONG_TIMEOUT_MS);
  if(!r.ok){sendNanoFailure(r);return;}
  uint32_t v[6];uint8_t n=0;
  if(!parseDataLine(r.response,90,0,v,6,n)||n<3){
    sendError(502,F("BAD_AUDIO_CALIBRATION"));return;
  }
  String body=F("{\"ok\":true,\"mic_dc\":");body+=v[0];
  body+=F(",\"vu_low_pass\":");body+=v[1];body+=F(",\"spectrum_low_pass\":");body+=v[2];body+='}';
  sendJson(200,body);
}

// ---------------------------------------------------------------------------
// Ambient
// ---------------------------------------------------------------------------

int ambientCode(const String& id) {
  if(id=="F01")return 0;if(id=="F02")return 1;if(id=="F03")return 2;return -1;
}

void handleAmbientEffect() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  String id;
  if(jsonString(server.arg(F("plain")),"id",id)!=JSON_OK){
    sendError(400,F("BAD_EFFECT"));return;
  }
  const int code=ambientCode(id);
  if(code<0){sendError(400,F("BAD_EFFECT"));return;}
  if(!runAck(100,makeCommand(100,code)))return;
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handleAmbientSettings() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  const String body=server.arg(F("plain"));
  bool any=false;
  long v=0;JsonRead jr;

  bool flag=false;
  jr=jsonBool(body,"auto_cycle",flag);
  if(jr==JSON_BAD){sendError(400,F("BAD_AUTO_CYCLE"));return;}
  if(jr==JSON_OK){if(!runAck(103,makeCommand(103,flag?1:0)))return;any=true;}

  jr=jsonLong(body,"auto_period_s",1,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_PERIOD"));return;}
  if(jr==JSON_OK){if(!runAck(104,makeCommand(104,v)))return;any=true;}

  jr=jsonLong(body,"hue",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_HUE"));return;}
  if(jr==JSON_OK){if(!runAck(105,makeCommand(105,v)))return;any=true;}

  jr=jsonLong(body,"saturation",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_SATURATION"));return;}
  if(jr==JSON_OK){if(!runAck(106,makeCommand(106,v)))return;any=true;}

  jr=jsonLong(body,"brightness",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_BRIGHTNESS"));return;}
  if(jr==JSON_OK){if(!runAck(107,makeCommand(107,v)))return;any=true;}

  jr=jsonLong(body,"speed",1,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_SPEED"));return;}
  if(jr==JSON_OK){if(!runAck(108,makeCommand(108,v)))return;any=true;}

  double step=0.0;
  jr=jsonDouble(body,"rainbow_step",0.5,10.0,step);
  if(jr==JSON_BAD){sendError(400,F("BAD_RAINBOW_STEP"));return;}
  if(jr==JSON_OK){
    const long step10=static_cast<long>(step*10.0+0.5);
    if(!runAck(109,makeCommand(109,step10)))return;any=true;
  }

  bool persist=true;
  jr=jsonBool(body,"persist",persist);
  if(jr==JSON_BAD){sendError(400,F("BAD_PERSIST"));return;}
  if(persist && any){if(!runAck(110,F("110")))return;}

  if(!any){sendError(400,F("NO_SUPPORTED_FIELDS"));return;}
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

// ---------------------------------------------------------------------------
// Night
// ---------------------------------------------------------------------------

void handleNightSettings() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  const String body=server.arg(F("plain"));
  bool any=false;
  bool previewChanged=false;

  bool enabled=false;
  JsonRead jr=jsonBool(body,"enabled",enabled);
  if(jr==JSON_BAD){sendError(400,F("BAD_ENABLED"));return;}

  long v=0;
  JsonRead jh=jsonLong(body,"hue",0,255,v);
  if(jh==JSON_BAD){sendError(400,F("BAD_HUE"));return;}
  if(jh==JSON_OK){if(!runAck(51,makeCommand(51,v)))return;any=true;previewChanged=true;}

  JsonRead js=jsonLong(body,"saturation",0,255,v);
  if(js==JSON_BAD){sendError(400,F("BAD_SATURATION"));return;}
  if(js==JSON_OK){if(!runAck(52,makeCommand(52,v)))return;any=true;previewChanged=true;}

  JsonRead jb=jsonLong(body,"brightness",0,255,v);
  if(jb==JSON_BAD){sendError(400,F("BAD_BRIGHTNESS"));return;}
  if(jb==JSON_OK){if(!runAck(53,makeCommand(53,v)))return;any=true;previewChanged=true;}

  String onText,offText;
  JsonRead jon=jsonString(body,"schedule_on",onText);
  JsonRead joff=jsonString(body,"schedule_off",offText);
  if(jon==JSON_BAD||joff==JSON_BAD){sendError(400,F("BAD_SCHEDULE_TIME"));return;}
  if((jon==JSON_OK)!=(joff==JSON_OK)){sendError(400,F("BOTH_SCHEDULE_TIMES_REQUIRED"));return;}
  if(jon==JSON_OK){
    uint8_t oh,om,fh,fm;
    if(!parseClock(onText,oh,om)||!parseClock(offText,fh,fm)||(oh==fh&&om==fm)){
      sendError(400,F("BAD_SCHEDULE_TIME"));return;
    }
    if(!runAck(55,makeCommand(55,oh,om,fh,fm)))return;
    any=true;
  }

  bool schedule=false;
  JsonRead jse=jsonBool(body,"schedule_enabled",schedule);
  if(jse==JSON_BAD){sendError(400,F("BAD_SCHEDULE_ENABLED"));return;}
  if(jse==JSON_OK){if(!runAck(54,makeCommand(54,schedule?1:0)))return;any=true;}

  // Manual ON/OFF is applied after preview values so an explicit enabled change
  // persists the final current Night values by design.
  if(jr==JSON_OK){if(!runAck(50,makeCommand(50,enabled?1:0)))return;any=true;previewChanged=false;}

  bool persist=true;
  JsonRead jp=jsonBool(body,"persist",persist);
  if(jp==JSON_BAD){sendError(400,F("BAD_PERSIST"));return;}
  if(previewChanged && persist){if(!runAck(56,F("56")))return;}

  if(!any){sendError(400,F("NO_SUPPORTED_FIELDS"));return;}
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

// ---------------------------------------------------------------------------
// Alarm / Dawn
// ---------------------------------------------------------------------------

void handleAlarmSettings() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  const String body=server.arg(F("plain"));
  bool any=false;
  bool dawnChanged=false;

  bool enabled=false;
  JsonRead je=jsonBool(body,"enabled",enabled);
  if(je==JSON_BAD){sendError(400,F("BAD_ENABLED"));return;}
  if(je==JSON_OK){if(!runAck(60,makeCommand(60,enabled?1:0)))return;any=true;}

  long hour=0,minute=0;
  JsonRead jh=jsonLong(body,"hour",0,23,hour);
  JsonRead jm=jsonLong(body,"minute",0,59,minute);
  if(jh==JSON_BAD||jm==JSON_BAD){sendError(400,F("BAD_ALARM_TIME"));return;}
  if((jh==JSON_OK)!=(jm==JSON_OK)){sendError(400,F("HOUR_AND_MINUTE_REQUIRED"));return;}
  if(jh==JSON_OK){if(!runAck(61,makeCommand(61,hour,minute)))return;any=true;}

  long v=0;
  JsonRead jr=jsonLong(body,"fade_minutes",1,120,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_FADE"));return;}
  if(jr==JSON_OK){if(!runAck(73,makeCommand(73,v)))return;any=true;dawnChanged=true;}

  jr=jsonLong(body,"max_brightness",1,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_MAX_BRIGHTNESS"));return;}
  if(jr==JSON_OK){if(!runAck(74,makeCommand(74,v)))return;any=true;dawnChanged=true;}

  jr=jsonLong(body,"start_hue",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_START_HUE"));return;}
  if(jr==JSON_OK){if(!runAck(75,makeCommand(75,v)))return;any=true;dawnChanged=true;}

  jr=jsonLong(body,"end_hue",0,255,v);
  if(jr==JSON_BAD){sendError(400,F("BAD_END_HUE"));return;}
  if(jr==JSON_OK){if(!runAck(76,makeCommand(76,v)))return;any=true;dawnChanged=true;}

  bool persist=true;
  JsonRead jp=jsonBool(body,"persist",persist);
  if(jp==JSON_BAD){sendError(400,F("BAD_PERSIST"));return;}
  if(dawnChanged&&persist){if(!runAck(77,F("77")))return;}

  if(!any){sendError(400,F("NO_SUPPORTED_FIELDS"));return;}
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

void handleStopDawn() {
  if(!runAck(71,F("71")))return;
  sendJson(200,F("{\"ok\":true,\"stopped\":true}"));
}

// ---------------------------------------------------------------------------
// System service settings
// ---------------------------------------------------------------------------

void handleCurrentLimitGet() {
  const NanoResult sr=nanoSettings();
  if(!sr.ok){sendNanoFailure(sr);return;}
  uint32_t v[8];uint8_t n=0;
  if(!getSettingLine(sr,20,v,8,n)||n<1){
    sendError(502,F("BAD_SYSTEM_SETTINGS"));return;
  }
  String body=F("{\"ok\":true,\"milliamps\":");body+=v[0];body+=F(",\"hard_max_ma\":4500}");
  sendJson(200,body);
}

void handleCurrentLimitSet() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  long ma=0;
  if(jsonLong(server.arg(F("plain")),"milliamps",500,4500,ma)!=JSON_OK){
    sendError(400,F("BAD_CURRENT_LIMIT"));return;
  }
  if(!runAck(120,makeCommand(120,ma)))return;
  sendJson(200,F("{\"ok\":true,\"applied\":true}"));
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

const char* eventName(uint32_t code) {
  switch(code){
    case 1:return "night_schedule";
    case 2:return "dawn_start";
    case 3:return "dawn_stop";
    case 4:return "dawn_complete";
    case 5:return "dawn_recover";
    case 6:return "alarm_trigger";
    case 7:return "clap_toggle";
    case 8:return "cold_power_on";
    case 9:return "warm_reset";
    default:return "unknown";
  }
}

void appendEventJson(String& body,const String& raw) {
  if(raw.startsWith(F("ARDU1 "))){
    body+=F("{\"type\":\"nano_startup\",\"raw\":\"");
    body+=jsonEscape(raw);body+=F("\"}");
    return;
  }

  uint32_t vals[4]={0};uint8_t count=0;
  int pos=2;uint32_t v=0;
  while(count<4&&nextU32Token(raw,pos,v))vals[count++]=v;

  body+=F("{\"type\":\"");
  body+=count?eventName(vals[0]):"unknown";
  body+=F("\",\"code\":");body+=count?vals[0]:0;
  body+=F(",\"args\":[");
  for(uint8_t i=1;i<count;++i){if(i>1)body+=',';body+=vals[i];}
  body+=F("],\"raw\":\"");body+=jsonEscape(raw);body+=F("\"}");
}

void handleEvents() {
  drainNanoAsync();

  String body=F("{\"ok\":true,\"events\":[");
  const uint8_t count=eventStored;
  const uint8_t first=static_cast<uint8_t>((eventWrite+Cfg::EVENT_COUNT-count)%Cfg::EVENT_COUNT);

  for(uint8_t i=0;i<count;++i){
    if(i)body+=',';
    const uint8_t idx=static_cast<uint8_t>((first+i)%Cfg::EVENT_COUNT);
    appendEventJson(body,String(eventLines[idx]));
  }
  body+=F("]}");
  sendJson(200,body);
}

// ---------------------------------------------------------------------------
// Developer raw numeric UART
// ---------------------------------------------------------------------------

void handleDevNano() {
  if(!server.hasArg(F("plain"))){sendError(400,F("BODY_REQUIRED"));return;}
  String command=server.arg(F("plain"));
  command.trim();
  if(!safeRawCommand(command)){sendError(400,F("BAD_COMMAND"));return;}

  uint32_t op32=0;int next=0;
  if(!parseLeadingU32(command,op32,next)||op32>65535){
    sendError(400,F("NUMERIC_UART_REQUIRED"));return;
  }

  const uint16_t op=static_cast<uint16_t>(op32);
  const bool dataOnly=(op==2||op==4||op==40||op==41||op==42||op==45||op==90);
  const bool settings=(op==3);

  NanoResult r;
  if(settings)r=transactNano(command,op,true,Cfg::NANO_DEV_TIMEOUT_MS);
  else r=transactNano(command,op,!dataOnly,Cfg::NANO_DEV_TIMEOUT_MS);

  String body=F("{\"ok\":");
  body+=r.ok?F("true"):F("false");
  body+=F(",\"command\":\"");body+=jsonEscape(command);
  body+=F("\",\"nano\":\"");body+=jsonEscape(r.response);
  body+=F("\",\"timed_out\":");body+=r.timedOut?F("true"):F("false");
  body+=F(",\"nano_error_code\":");body+=r.errorCode;
  body+='}';
  sendJson(r.ok?200:nanoHttpStatus(r),body);
}

// ---------------------------------------------------------------------------
// HTTP routing
// ---------------------------------------------------------------------------

void handleNotFound(){sendError(404,F("NOT_FOUND"));}

void setupHttp() {
  server.on("/api/ping",HTTP_GET,handlePing);
  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/settings",HTTP_GET,handleSettings);
  server.on("/api/time",HTTP_GET,handleTime);
  server.on("/api/time/sync",HTTP_POST,handleTimeSync);
  server.on("/api/power",HTTP_POST,handlePower);
  server.on("/api/mode",HTTP_POST,handleMode);

  server.on("/api/light/status",HTTP_GET,handleLightStatus);
  server.on("/api/light/settings",HTTP_POST,handleLightSettings);

  server.on("/api/light/clap/calibration",HTTP_GET,handleClapCalibrationStatus);
  server.on("/api/light/clap/calibration/start",HTTP_POST,handleClapCalibrationStart);
  server.on("/api/light/clap/calibration/sample",HTTP_POST,handleClapCalibrationSample);
  server.on("/api/light/clap/calibration/finish",HTTP_POST,handleClapCalibrationFinish);
  server.on("/api/light/clap/calibration/save",HTTP_POST,handleClapCalibrationSave);
  server.on("/api/light/clap/calibration/cancel",HTTP_POST,handleClapCalibrationCancel);

  server.on("/api/music/mode",HTTP_POST,handleMusicMode);
  server.on("/api/music/settings",HTTP_POST,handleMusicSettings);
  server.on("/api/music/calibrate",HTTP_POST,handleMusicCalibrate);

  server.on("/api/ambient/effect",HTTP_POST,handleAmbientEffect);
  server.on("/api/ambient/settings",HTTP_POST,handleAmbientSettings);

  server.on("/api/night/settings",HTTP_POST,handleNightSettings);

  server.on("/api/alarm/settings",HTTP_POST,handleAlarmSettings);
  server.on("/api/alarm/stop-dawn",HTTP_POST,handleStopDawn);

  server.on("/api/system/current-limit",HTTP_GET,handleCurrentLimitGet);
  server.on("/api/system/current-limit",HTTP_POST,handleCurrentLimitSet);

  server.on("/api/events",HTTP_GET,handleEvents);
  server.on("/api/dev/nano",HTTP_POST,handleDevNano);

  const char* optionsRoutes[]={
    "/api/ping","/api/status","/api/settings","/api/time","/api/time/sync",
    "/api/power","/api/mode","/api/light/status","/api/light/settings",
    "/api/light/clap/calibration","/api/light/clap/calibration/start",
    "/api/light/clap/calibration/sample","/api/light/clap/calibration/finish",
    "/api/light/clap/calibration/save","/api/light/clap/calibration/cancel",
    "/api/music/mode","/api/music/settings","/api/music/calibrate",
    "/api/ambient/effect","/api/ambient/settings","/api/night/settings",
    "/api/alarm/settings","/api/alarm/stop-dawn",
    "/api/system/current-limit","/api/events","/api/dev/nano"
  };
  for(const char* route:optionsRoutes)server.on(route,HTTP_OPTIONS,handleOptions);

  server.onNotFound(handleNotFound);
  server.begin();
}

// ---------------------------------------------------------------------------
// Wi-Fi / OTA
// ---------------------------------------------------------------------------

bool credentialsLookConfigured() {
  return strcmp(WIFI_SSID,"PUT_YOUR_WIFI_SSID_HERE")!=0 &&
         strcmp(WIFI_PASSWORD,"PUT_YOUR_WIFI_PASSWORD_HERE")!=0 &&
         strcmp(OTA_PASSWORD,"PUT_A_STRONG_OTA_PASSWORD_HERE")!=0 &&
         strlen(WIFI_SSID)>0 &&
         strlen(WIFI_PASSWORD)>0 &&
         strlen(OTA_PASSWORD)>=8;
}

bool connectWifiBlocking() {
  if(!credentialsLookConfigured())return false;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(Cfg::HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);

  const unsigned long start=millis();
  while(WiFi.status()!=WL_CONNECTED &&
        millis()-start<Cfg::WIFI_CONNECT_TIMEOUT_MS){
    delay(100);yield();
  }
  return WiFi.status()==WL_CONNECTED;
}

void startNetworkServicesIfPossible() {
  if(networkServicesStarted||WiFi.status()!=WL_CONNECTED)return;

  ArduinoOTA.setPort(Cfg::OTA_PORT);
  ArduinoOTA.setHostname(Cfg::HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([](){otaInProgress=true;});
  ArduinoOTA.onEnd([](){otaInProgress=false;});
  ArduinoOTA.onError([](ota_error_t){otaInProgress=false;});
  ArduinoOTA.begin();

  MDNS.addService("http","tcp",Cfg::HTTP_PORT);
  networkServicesStarted=true;
}

void stopNetworkServices() {
  if(!networkServicesStarted)return;
  ArduinoOTA.end();
  networkServicesStarted=false;
  otaInProgress=false;
}

void maintainWifi() {
  if(!credentialsLookConfigured()){stopNetworkServices();return;}
  if(WiFi.status()==WL_CONNECTED){startNetworkServicesIfPossible();return;}

  stopNetworkServices();
  const unsigned long now=millis();
  if(now-lastWifiRetryMs<Cfg::WIFI_RETRY_MS)return;
  lastWifiRetryMs=now;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
}

void resyncNanoAfterEspBoot() {
  Serial.print('\n');
  Serial.flush();
  delay(50);
  while(Serial.available()>0)(void)Serial.read();
}

void setup() {
  Serial.begin(Cfg::NANO_BAUD);
  Serial.setTimeout(50);
  delay(250);

  (void)connectWifiBlocking();
  resyncNanoAfterEspBoot();

  startNetworkServicesIfPossible();
  setupHttp();
}

void loop() {
  maintainWifi();

  if(networkServicesStarted)ArduinoOTA.handle();

  if(otaInProgress){
    yield();
    return;
  }

  server.handleClient();
  drainNanoAsync();
  yield();
}
