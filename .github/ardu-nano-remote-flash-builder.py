from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    assert count == 1, f"{label}: expected exactly one match, got {count}"
    return text.replace(old, new, 1)


root = Path('.')
v3_path = root / '05_WiFi_и_приложение/esp8266_warm_dawn_v3/esp8266_warm_dawn_v3.ino'
v4_dir = root / '05_WiFi_и_приложение/esp8266_nano_remote_v4'
v4_dir.mkdir(parents=True, exist_ok=True)
v4_path = v4_dir / 'esp8266_nano_remote_v4.ino'
s = v3_path.read_text(encoding='utf-8')

s = replace_once(
    s,
    '/*\n  ARDU ESP8266 v1 release candidate.\n',
    '/*\n  ARDU ESP8266 V4: Warm Dawn V3 + authenticated remote Nano programming bridge.\n',
    'header')
s = replace_once(
    s,
    'constexpr char FW_NAME[] = "ARDU_ESP_V3_WARM_DAWN";',
    'constexpr char FW_NAME[] = "ARDU_ESP_V4_NANO_REMOTE";',
    'firmware name')
s = replace_once(
    s,
    'constexpr unsigned long NANO_DEV_TIMEOUT_MS = 8000UL;\n',
    '''constexpr unsigned long NANO_DEV_TIMEOUT_MS = 8000UL;

// Remote Nano programming uses the factory Arduino Nano ATmega328P Old
// Bootloader. Arduino AVR boards.txt specifies protocol=arduino and 57600 baud.
// ESP UART0 is temporarily switched from the normal 115200 command transport
// to a transparent TCP<->UART bridge while avrdude owns the Nano bootloader.
constexpr uint16_t NANO_PROGRAM_PORT = 3280;
constexpr unsigned long NANO_PROGRAM_BAUD = 57600UL;
constexpr uint8_t NANO_RESET_GPIO = 4; // GPIO4 -> resistor -> NPN base; collector -> Nano RESET
constexpr unsigned long NANO_PROGRAM_ARM_MS = 30000UL;
constexpr unsigned long NANO_PROGRAM_IDLE_MS = 15000UL;
''',
    'remote programmer constants')
s = replace_once(
    s,
    'ESP8266WebServer server(Cfg::HTTP_PORT);\n',
    '''ESP8266WebServer server(Cfg::HTTP_PORT);
WiFiServer nanoProgramServer(Cfg::NANO_PROGRAM_PORT);
WiFiClient nanoProgramClient;
''',
    'program server globals')
s = replace_once(
    s,
    'bool otaInProgress = false;\n',
    '''bool otaInProgress = false;

bool nanoProgramArmed = false;
bool nanoProgramActive = false;
IPAddress nanoProgramAuthorizedIp;
unsigned long nanoProgramArmDeadlineMs = 0;
unsigned long nanoProgramLastTrafficMs = 0;
String nanoProgramLastResult = "never";
''',
    'program state globals')

remote_block = r'''
// ---------------------------------------------------------------------------
// Authenticated remote Nano programming bridge
// ---------------------------------------------------------------------------
//
// Security / recovery model:
// - Station Wi-Fi only; ARDU-DIRECT cannot arm the programmer.
// - POST /api/system/nano-program/arm must contain the same local OTA password
//   used by ArduinoOTA. The password is never returned by the API.
// - The arm request authorizes only the requesting IPv4 address for 30 s.
// - GPIO4 drives Nano RESET through an NPN open-collector stage; never connect
//   Nano RESET (5 V pull-up domain) directly to an ESP8266 GPIO.
// - On TCP connect, ESP switches UART0 to 57600 and pulses Nano RESET. avrdude
//   then speaks the standard Arduino/STK500 bootloader protocol transparently.
// - On disconnect/timeout, UART0 returns to the normal 115200 ARDU protocol.
//
// The bridge intentionally does not parse Intel HEX and does not implement its
// own flash writer. avrdude remains responsible for HEX validation, programming
// and verify, reducing the amount of safety-critical code on ESP8266.

bool nanoProgramArmStillValid() {
  return nanoProgramArmed &&
         static_cast<long>(nanoProgramArmDeadlineMs - millis()) > 0;
}

void clearNanoProgramArm(const char* result = nullptr) {
  nanoProgramArmed = false;
  nanoProgramArmDeadlineMs = 0;
  if (result) nanoProgramLastResult = result;
}

void pulseNanoReset() {
  // Transistor stage is active HIGH: GPIO4 HIGH -> NPN ON -> RESET pulled LOW.
  digitalWrite(Cfg::NANO_RESET_GPIO, HIGH);
  delay(60);
  digitalWrite(Cfg::NANO_RESET_GPIO, LOW);
  delay(40);
}

void restoreNormalNanoUart() {
  Serial.flush();
  Serial.end();
  delay(10);
  Serial.begin(Cfg::NANO_BAUD);
  Serial.setTimeout(50);
  asyncLength = 0;
  while (Serial.available() > 0) (void)Serial.read();
}

void finishNanoProgramSession(const char* result) {
  if (nanoProgramClient) nanoProgramClient.stop();
  restoreNormalNanoUart();
  nanoProgramActive = false;
  clearNanoProgramArm(result);
  nanoProgramLastTrafficMs = millis();
}

void startNanoProgramSession(WiFiClient client) {
  nanoProgramClient = client;
  nanoProgramClient.setNoDelay(true);
  nanoProgramActive = true;
  nanoProgramArmed = false;
  nanoProgramLastResult = "connected";

  // Stop interpreting the shared UART as ARDU commands before changing baud.
  Serial.flush();
  while (Serial.available() > 0) (void)Serial.read();
  Serial.end();
  delay(10);
  Serial.begin(Cfg::NANO_PROGRAM_BAUD);
  Serial.setTimeout(50);

  pulseNanoReset();
  nanoProgramLastTrafficMs = millis();
}

bool serviceNanoProgramBridge() {
  if (nanoProgramActive) {
    if (!nanoProgramClient || !nanoProgramClient.connected()) {
      finishNanoProgramSession("disconnected");
      return false;
    }

    bool traffic = false;
    uint8_t buf[128];

    int available = nanoProgramClient.available();
    if (available > 0) {
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);
      const int got = nanoProgramClient.read(buf, available);
      if (got > 0) {
        Serial.write(buf, static_cast<size_t>(got));
        traffic = true;
      }
    }

    available = Serial.available();
    if (available > 0) {
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);
      const int got = Serial.readBytes(buf, static_cast<size_t>(available));
      if (got > 0) {
        nanoProgramClient.write(buf, static_cast<size_t>(got));
        traffic = true;
      }
    }

    if (traffic) nanoProgramLastTrafficMs = millis();
    if (millis() - nanoProgramLastTrafficMs > Cfg::NANO_PROGRAM_IDLE_MS) {
      finishNanoProgramSession("idle_timeout");
      return false;
    }

    yield();
    return true;
  }

  if (nanoProgramArmed && !nanoProgramArmStillValid()) {
    clearNanoProgramArm("arm_expired");
  }
  if (nanoProgramArmed && WiFi.status() != WL_CONNECTED) {
    clearNanoProgramArm("station_lost");
  }

  WiFiClient candidate = nanoProgramServer.available();
  if (!candidate) return false;

  const IPAddress remote = candidate.remoteIP();
  if (!nanoProgramArmStillValid() ||
      WiFi.status() != WL_CONNECTED ||
      !(remote == nanoProgramAuthorizedIp)) {
    candidate.stop();
    nanoProgramLastResult = "rejected_client";
    return false;
  }

  startNanoProgramSession(candidate);
  return true;
}

void handleNanoProgramStatus() {
  if (nanoProgramArmed && !nanoProgramArmStillValid()) {
    clearNanoProgramArm("arm_expired");
  }
  String body = F("{\"ok\":true,\"supported\":true,\"station_required\":true");
  body += F(",\"active\":"); body += nanoProgramActive ? F("true") : F("false");
  body += F(",\"armed\":"); body += nanoProgramArmStillValid() ? F("true") : F("false");
  body += F(",\"port\":"); body += Cfg::NANO_PROGRAM_PORT;
  body += F(",\"baud\":"); body += Cfg::NANO_PROGRAM_BAUD;
  body += F(",\"reset_gpio\":"); body += Cfg::NANO_RESET_GPIO;
  body += F(",\"last_result\":\""); body += jsonEscape(nanoProgramLastResult); body += '"';
  if (nanoProgramArmStillValid()) {
    body += F(",\"arm_remaining_ms\":");
    body += static_cast<unsigned long>(nanoProgramArmDeadlineMs - millis());
  }
  body += '}';
  sendJson(200, body);
}

void handleNanoProgramArm() {
  if (nanoProgramActive) { sendError(409, F("NANO_PROGRAM_ACTIVE")); return; }
  if (WiFi.status() != WL_CONNECTED) {
    sendError(409, F("NANO_PROGRAM_REQUIRES_STATION"));
    return;
  }
  if (!otaPasswordLooksConfigured()) {
    sendError(503, F("OTA_PASSWORD_NOT_CONFIGURED"));
    return;
  }
  if (!server.hasArg(F("plain"))) { sendError(400, F("BODY_REQUIRED")); return; }

  String supplied;
  if (jsonString(server.arg(F("plain")), "password", supplied) != JSON_OK) {
    sendError(400, F("PASSWORD_REQUIRED"));
    return;
  }
  const bool authorized = supplied == OTA_PASSWORD;
  supplied = "";
  if (!authorized) { sendError(403, F("AUTH_FAILED")); return; }

  nanoProgramAuthorizedIp = server.client().remoteIP();
  nanoProgramArmed = true;
  nanoProgramArmDeadlineMs = millis() + Cfg::NANO_PROGRAM_ARM_MS;
  nanoProgramLastResult = "armed";

  String body = F("{\"ok\":true,\"armed\":true,\"port\":");
  body += Cfg::NANO_PROGRAM_PORT;
  body += F(",\"baud\":"); body += Cfg::NANO_PROGRAM_BAUD;
  body += F(",\"window_ms\":"); body += Cfg::NANO_PROGRAM_ARM_MS;
  body += F(",\"reset_gpio\":"); body += Cfg::NANO_RESET_GPIO;
  body += '}';
  sendJson(200, body);
}

'''
s = replace_once(
    s,
    '// ---------------------------------------------------------------------------\n// System service settings\n// ---------------------------------------------------------------------------\n',
    remote_block + '// ---------------------------------------------------------------------------\n// System service settings\n// ---------------------------------------------------------------------------\n',
    'remote programmer block')

s = replace_once(
    s,
    '  body += F(",\\\"ota_port\\\":");\n  body += Cfg::OTA_PORT;\n  body += \'}\';\n',
    '''  body += F(",\\\"ota_port\\\":");
  body += Cfg::OTA_PORT;
  body += F(",\\\"nano_remote_flash\\\":true,\\\"nano_program_port\\\":");
  body += Cfg::NANO_PROGRAM_PORT;
  body += F(",\\\"nano_program_baud\\\":");
  body += Cfg::NANO_PROGRAM_BAUD;
  body += '}';
''',
    'ping capability')

s = replace_once(
    s,
    '  server.on("/api/system/reset-defaults",HTTP_POST,handleResetDefaults);\n',
    '''  server.on("/api/system/reset-defaults",HTTP_POST,handleResetDefaults);
  server.on("/api/system/nano-program/status",HTTP_GET,handleNanoProgramStatus);
  server.on("/api/system/nano-program/arm",HTTP_POST,handleNanoProgramArm);
''',
    'program HTTP routes')
s = replace_once(
    s,
    '    "/api/system/current-limit","/api/system/reset-defaults",\n',
    '    "/api/system/current-limit","/api/system/reset-defaults",\n    "/api/system/nano-program/status","/api/system/nano-program/arm",\n',
    'program OPTIONS routes')

s = replace_once(
    s,
    '''void setup() {
  Serial.begin(Cfg::NANO_BAUD);
''',
    '''void setup() {
  // GPIO4 drives only the base resistor of the NPN reset stage. LOW = transistor
  // off, so the Nano's own RESET pull-up keeps it running normally.
  pinMode(Cfg::NANO_RESET_GPIO, OUTPUT);
  digitalWrite(Cfg::NANO_RESET_GPIO, LOW);

  Serial.begin(Cfg::NANO_BAUD);
''',
    'setup reset pin')
s = replace_once(
    s,
    '''  startNetworkServicesIfPossible();
  setupHttp();
}
''',
    '''  startNetworkServicesIfPossible();
  nanoProgramServer.begin();
  setupHttp();
}
''',
    'setup program server')
s = replace_once(
    s,
    '''void loop() {
  maintainWifi();

  if(networkServicesStarted)ArduinoOTA.handle();
''',
    '''void loop() {
  // During a Nano programming session, the UART byte stream belongs entirely
  // to avrdude. Do not let HTTP handlers, async parsing or Wi-Fi reconnect logic
  // inject/read UART bytes until the TCP session ends.
  if(serviceNanoProgramBridge()) return;

  maintainWifi();

  if(networkServicesStarted)ArduinoOTA.handle();
''',
    'loop bridge priority')

# Static source assertions before writing.
for required in [
    'ARDU_ESP_V4_NANO_REMOTE',
    'NANO_PROGRAM_BAUD = 57600UL',
    'NANO_RESET_GPIO = 4',
    'WiFiServer nanoProgramServer',
    'server.client().remoteIP()',
    '/api/system/nano-program/arm',
    'serviceNanoProgramBridge()',
    'pulseNanoReset()',
    'nano_remote_flash',
]:
    assert required in s, required
v4_path.write_text(s, encoding='utf-8')

# Windows helper: arm ESP with the local OTA password, then use the Arduino IDE
# AVRDUDE 8.x installation over its supported net:host:port transport.
tool_dir = root / 'tools'
tool_dir.mkdir(exist_ok=True)
ps = r'''param(
    [Parameter(Mandatory=$true)]
    [string]$HexPath,

    [string]$DeviceIp = "192.168.0.4",
    [string]$OtaPassword,
    [string]$AvrdudePath,
    [string]$AvrdudeConfPath
)

$ErrorActionPreference = "Stop"

function Find-Avrdude {
    $root = Join-Path $env:LOCALAPPDATA "Arduino15\packages\arduino\tools\avrdude"
    if (-not (Test-Path $root)) {
        throw "Arduino AVRDUDE folder not found: $root"
    }
    $exe = Get-ChildItem -Path $root -Recurse -Filter avrdude.exe -File |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $exe) { throw "avrdude.exe not found under $root" }
    return $exe.FullName
}

$hex = (Resolve-Path $HexPath).Path
if (-not $hex.ToLowerInvariant().EndsWith(".hex")) {
    throw "Expected an Arduino Intel HEX file (*.hex): $hex"
}

if (-not $AvrdudePath) { $AvrdudePath = Find-Avrdude }
$AvrdudePath = (Resolve-Path $AvrdudePath).Path

if (-not $AvrdudeConfPath) {
    $binDir = Split-Path $AvrdudePath -Parent
    $toolRoot = Split-Path $binDir -Parent
    $AvrdudeConfPath = Join-Path $toolRoot "etc\avrdude.conf"
}
$AvrdudeConfPath = (Resolve-Path $AvrdudeConfPath).Path

if (-not $OtaPassword) {
    $secure = Read-Host "Current ARDU ESP OTA password" -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        $OtaPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    } finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    }
}

Write-Host "Checking ARDU ESP V4 remote programmer..."
$ping = Invoke-RestMethod "http://$DeviceIp/api/ping" -TimeoutSec 5
if (-not $ping.ok -or -not $ping.nano_remote_flash) {
    throw "ESP at $DeviceIp does not report nano_remote_flash=true"
}

$payload = @{ password = $OtaPassword } | ConvertTo-Json -Compress
$arm = Invoke-RestMethod \
    "http://$DeviceIp/api/system/nano-program/arm" \
    -Method Post \
    -ContentType "application/json" \
    -Body $payload \
    -TimeoutSec 5

# Do not keep the plaintext password around longer than needed.
$OtaPassword = $null
$payload = $null

if (-not $arm.ok -or -not $arm.armed) {
    throw "ESP did not arm Nano programming mode"
}

$netPort = "net:$DeviceIp`:$($arm.port)"
Write-Host "Programming Nano through $netPort at $($arm.baud) baud..."
Write-Host "HEX: $hex"

$args = @(
    "-C$AvrdudeConfPath",
    "-v",
    "-p", "atmega328p",
    "-c", "arduino",
    "-P", $netPort,
    "-b", "$($arm.baud)",
    "-D",
    "-U", "flash:w:$hex`:i"
)

& $AvrdudePath @args
$code = $LASTEXITCODE
if ($code -ne 0) {
    throw "AVRDUDE failed with exit code $code. The ESP reset bridge remains the recovery path; arm and retry."
}

Write-Host "AVRDUDE write + verify completed. Waiting for ARDU UART to return..."
$deadline = (Get-Date).AddSeconds(12)
$status = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 600
    try {
        $status = Invoke-RestMethod "http://$DeviceIp/api/status" -TimeoutSec 3
        if ($status.ok) { break }
    } catch {
        # Old Nano bootloader can take a few seconds to fall through after reset.
    }
}

if ($status -and $status.ok) {
    Write-Host "PASS: Nano is responding again. mode=$($status.mode), rtc_valid=$($status.rtc_valid)"
} else {
    Write-Warning "Programming verified, but /api/status did not return within 12 s. Check /api/system/nano-program/status and retry the Nano reset/program cycle if needed."
}
'''
(tool_dir / 'flash_nano_over_wifi.ps1').write_text(ps, encoding='utf-8')

wiring = '''# Удалённая прошивка Arduino Nano через ESP8266 — R1\n\nДата: 2026-10-17  \nСтатус: SOURCE/CI PASS; физический монтаж reset-цепи и первый Wi-Fi upload владельцем pending.\n\n## 1. Что реализовано\n\nESP `ARDU_ESP_V4_NANO_REMOTE` остаётся обычным HTTP/UART шлюзом и получает сервисный режим удалённой прошивки Nano. После авторизации ESP на 30 секунд разрешает TCP-соединение только с того IP, который выполнил arm-запрос. При подключении `avrdude` ESP переключает общий UART с 115200 на 57600, аппаратно сбрасывает Nano в её штатный Old Bootloader и прозрачно передаёт байты `avrdude ↔ Nano`. После завершения TCP-сессии ESP возвращает UART 115200.\n\nФормат HEX, запись Flash и verify выполняет стандартный AVRDUDE, а не самодельный flash-parser ESP. Для фактической Nano проект уже подтверждал `ATmega328P (Old Bootloader)`; официальный Arduino AVR `boards.txt` задаёт для него `protocol=arduino`, `upload.speed=57600`, `maximum_size=30720`.\n\n## 2. Дополнительная reset-цепь\n\n**Тип соединения:** ESP управляет RESET Nano **через NPN-транзистор как открытый коллектор**. Nano RESET напрямую к GPIO ESP не подключать: RESET Nano подтянут в 5-вольтовом домене, а ESP8266 не является 5-V tolerant.\n\n```text\nESP GPIO4 ── 1.5 kΩ ──┬──► B  BC548\n                       │\n                     4.7 kΩ\n                       │\nGND ESP/Nano ──────────┴──► E  BC548\n\nNano RESET ─────────────────► C  BC548\n```\n\n- `GPIO4 LOW` → транзистор закрыт → Nano работает от штатной RESET pull-up;\n- `GPIO4 HIGH` → транзистор открыт → RESET притянут к GND;\n- 4.7 kΩ B→GND удерживает транзистор выключенным во время старта ESP;\n- земли ESP и Nano уже должны быть общими по рабочему UART;\n- точный порядок физических ножек `C/B/E` конкретного BC548 перед пайкой сверить по маркировке/даташиту экземпляра, не угадывать по форме корпуса.\n\nGPIO4 выбран потому, что текущая ESP V3 его не использует, а у ESP-12 он не является boot strap pin. GPIO0/GPIO2/GPIO15 для этой функции не используются.\n\n## 3. Сервисный API\n\n`GET /api/ping` дополнительно:\n\n```json\n{\"nano_remote_flash\":true,\"nano_program_port\":3280,\"nano_program_baud\":57600}\n```\n\n`GET /api/system/nano-program/status` возвращает `supported/active/armed/port/baud/reset_gpio/last_result`.\n\n`POST /api/system/nano-program/arm`:\n\n```json\n{\"password\":\"CURRENT_ESP_OTA_PASSWORD\"}\n```\n\nОграничения: только домашний Station; пароль совпадает с локальным `ARDU_OTA_PASSWORD`; ARDU-DIRECT не разрешает программирование Nano; arm действует 30 s и привязан к IP компьютера.\n\n## 4. Прошивка с Windows\n\nВ репозитории: `tools/flash_nano_over_wifi.ps1`. Скрипт сам находит AVRDUDE из Arduino IDE, спрашивает текущий OTA-пароль без вывода на экран, arm-ит ESP и запускает стандартную запись + verify через `-P net:<IP>:3280`.\n\nПример:\n\n```powershell\ncd C:\\ARDU\n.\\tools\\flash_nano_over_wifi.ps1 `\n  -HexPath .\\path\\to\\nano_warm_dawn_v6.ino.hex `\n  -DeviceIp 192.168.0.4\n```\n\nВо время удалённой прошивки Nano USB-кабель не подключать. Питание ARDU не выключать и Wi-Fi не переключать. Если TCP/upload оборвался, повторно выполнить arm и запуск скрипта: аппаратный GPIO4→RESET позволяет снова войти в bootloader даже при повреждённом приложении Nano.\n\n## 5. Первый физический gate\n\n1. При снятом питании собрать транзисторную RESET-цепь.\n2. Проверить мультиметром отсутствие прямого соединения Nano RESET↔ESP GPIO4 без резистора/транзистора.\n3. Включить ARDU, проверить обычный `/api/status`.\n4. OTA установить ESP V4 и убедиться, что `/api/ping` содержит `nano_remote_flash:true`.\n5. Первый раз удалённо записать Nano R6 HEX скриптом.\n6. После verify проверить `/api/status`, `/api/settings`, `/api/time`, L01 и Night.\n7. Не считать механизм hardware-PASS до первого реального write+verify+post-status.\n'''
(root / '02_Архитектура_и_решения/NANO_REMOTE_FLASH.md').write_text(wiring, encoding='utf-8')

# Canonical decision / history / source records. These files are committed only
# after the generated ESP V4 successfully compiles in the one-shot CI job.
dec_path = root / 'DECISIONS.md'
dec = dec_path.read_text(encoding='utf-8')
dec += '''\n\n---\n\n## D-126 — удалённая прошивка Nano через ESP8266 + аппаратный RESET\n\n**Дата:** 2026-10-17  \n**Статус:** ДЕЙСТВУЕТ; source/CI PASS, физический gate pending.\n\n**Решение владельца:** реализовать удалённое обновление Nano сразу, до следующей физической прошивки.\n\n**Архитектура:** ESP V4 предоставляет кратковременно авторизуемый TCP↔UART bridge для стандартного `avrdude`; программирование Nano выполняется её штатным `ATmega328P (Old Bootloader)` на 57600 baud. ESP не реализует собственный Intel HEX flash writer. Для надёжного recovery ESP GPIO4 управляет Nano RESET через имеющийся NPN BC548: GPIO4→1,5 kΩ→base, base→4,7 kΩ→GND, emitter→GND, collector→Nano RESET. Прямое RESET→ESP GPIO запрещено из-за 5-V pull-up домена Nano.\n\n**Безопасность:** arm только через домашний Station, с текущим ESP OTA password, на 30 секунд и только для IP инициатора; ARDU-DIRECT не разрешает Nano programming. Во время активной TCP-сессии общий UART полностью принадлежит avrdude; обычные Nano HTTP/UART операции приостанавливаются. ESP OTA сохраняется отдельно и продолжает обновлять сам ESP.\n\n**Почему не software-reset Nano:** если приложение Nano будет повреждено неудачной записью, оно уже не сможет принять команду входа в bootloader. Отдельный аппаратный RESET от ESP сохраняет удалённый recovery path.\n\nПодробно: `02_Архитектура_и_решения/NANO_REMOTE_FLASH.md`.\n'''
dec_path.write_text(dec, encoding='utf-8')

hist_path = root / 'HISTORY.md'
hist = hist_path.read_text(encoding='utf-8')
hist += '''\n\n## 2026-10-17 — Nano remote flashing R1 source/CI\n\n- По прямой команде владельца подготовлен ESP `ARDU_ESP_V4_NANO_REMOTE` поверх работающего Warm Dawn V3.\n- Реализован Station-only authenticated arm, IP-bound 30 s window, TCP port 3280, прозрачный `avrdude ↔ ESP UART0 ↔ Nano Old Bootloader` bridge на 57600 и GPIO4 reset control через NPN.\n- Добавлен Windows helper `tools/flash_nano_over_wifi.ps1`, использующий AVRDUDE из Arduino IDE и стандартный write+verify.\n- Добавлен монтажный/приёмочный документ `02_Архитектура_и_решения/NANO_REMOTE_FLASH.md`.\n- One-shot CI компилирует полный ESP V4 как Generic ESP8266 с четырьмя тестовыми credentials до публикации исходника. Физический reset-транзистор, реальный Wi-Fi Nano upload и recovery retry владельцем ещё не проверены.\n'''
hist_path.write_text(hist, encoding='utf-8')

src_path = root / 'SOURCES.md'
sources = src_path.read_text(encoding='utf-8')
sources += '''\n\n## S-NANO-OTA-001 — официальные основания remote Nano programming, 2026-10-17\n\n- Arduino `ArduinoCore-avr/boards.txt`: `nano.menu.cpu.atmega328old` использует `upload.protocol=arduino`, `upload.maximum_size=30720`, `upload.speed=57600`, bootloader `ATmegaBOOT_168_atmega328.hex`. Источник: `https://github.com/arduino/ArduinoCore-avr/blob/master/boards.txt`.\n- AVRDUDE 8.0 documentation: для serial-программаторов `-P net:host:port` создаёт TCP connection к terminal/console server, который прозрачно соединяет network stream с serial programmer. Источник: `https://avrdudes.github.io/avrdude/8.0/avrdude_4.html`.\n- White Adapter / ESP8266 pin references: GPIO4/GPIO5 являются обычными GPIO, в отличие от boot strap GPIO0/GPIO2/GPIO15; существующая white adapter routes ESP-12 signals to headers. Источники уже зафиксированы как S-010/S-011; дополнительная pin table: `https://homeding.github.io/boards/pins.htm`.\n\nЭти источники определяют software/protocol assumptions. Фактическая транзисторная RESET-цепь и первый remote flash Nano остаются hardware gate.\n'''
src_path.write_text(sources, encoding='utf-8')

api_path = root / '05_WiFi_и_приложение/API_V1.md'
api = api_path.read_text(encoding='utf-8')
api += '''\n\n## 11. Service: remote Nano programming — ESP V4 / D-126\n\nНе является пользовательским режимом лампы. Это сервисный maintenance path для владельца.\n\n- `GET /api/system/nano-program/status` — capability/state (`supported`, `active`, `armed`, `port=3280`, `baud=57600`, `reset_gpio=4`, `last_result`).\n- `POST /api/system/nano-program/arm` body `{\"password\":\"<current ESP OTA password>\"}` — открыть на 30 s TCP programming window для IP инициатора.\n- только Station; в ARDU-DIRECT arm возвращает conflict; пароль не возвращается API.\n- после TCP connect normal Nano UART API временно suspended; ESP переключает UART0 115200→57600 и аппаратно сбрасывает Nano.\n- TCP endpoint используется стандартным AVRDUDE `-P net:<ESP_IP>:3280 -c arduino -p atmega328p -b 57600`; после disconnect ESP возвращает normal UART 115200.\n\nPowerShell helper: `tools/flash_nano_over_wifi.ps1`. Подробная электрическая схема/приёмка: `02_Архитектура_и_решения/NANO_REMOTE_FLASH.md`.\n'''
api_path.write_text(api, encoding='utf-8')

arch_path = root / '02_Архитектура_и_решения/АРХИТЕКТУРА_СИСТЕМЫ.md'
arch = arch_path.read_text(encoding='utf-8')
arch += '''\n\n## 15. Сервисная удалённая прошивка Nano — D-126\n\nК рабочему UART D0/D1 добавляется одна отдельная service-линия: `ESP GPIO4 → NPN open collector → Nano RESET`. Она не участвует в режимах света. В обычной работе UART остаётся 115200 numeric v1; только после авторизованного maintenance arm ESP временно становится TCP↔UART bridge 57600 для штатного Nano Old Bootloader и возвращается в обычный режим после disconnect. Прямое соединение RESET Nano с 3,3-V GPIO ESP запрещено. Схема: `NANO_REMOTE_FLASH.md`.\n'''
arch_path.write_text(arch, encoding='utf-8')

bom_path = root / '02_КОМПОНЕНТЫ_BOM.md'
bom = bom_path.read_text(encoding='utf-8')
bom += '''\n\n## K. Nano remote flash service — D-126 (2026-10-17)\n\nДополнительная закупка не требуется: использовать 1× имеющийся `BC548`, 1× `1,5 кΩ`, 1× `4,7 кΩ` и короткий сигнальный провод. Назначение: изолированное управление Nano RESET от ESP GPIO4. Перед пайкой подтвердить C/B/E конкретного транзистора. Эти детали переводятся из резерва/общего запаса в service reset-цепь только после физической сборки; пока source/CI ready, hardware pending.\n'''
bom_path.write_text(bom, encoding='utf-8')

inv_path = root / '02_Наши_детали/ИНВЕНТАРЬ.md'
inv = inv_path.read_text(encoding='utf-8')
inv += '''\n\n### 2026-10-17 — назначение имеющихся деталей для Nano remote reset\n\nD-126 использует уже имеющиеся INV-060 BC548, INV-028 резистор 1,5 кΩ и INV-029 резистор 4,7 кΩ для service reset stage ESP GPIO4→Nano RESET. Новая покупка не требуется. Фактическое изъятие деталей из запаса/пайка ещё не подтверждены; до этого статус наличия не менять на «установлено».\n'''
inv_path.write_text(inv, encoding='utf-8')

status_path = root / 'STATUS.md'
status = status_path.read_text(encoding='utf-8')
status = replace_once(
    status,
    '## Текущий этап\n',
    '''## Текущий этап

**2026-10-17 — D-126 / удалённая прошивка Nano: ESP V4 SOURCE + compile gate PASS; hardware gate pending.** Подготовлен `esp8266_nano_remote_v4`: существующий ESP ArduinoOTA сохраняется, дополнительно Station-only service arm с текущим OTA password и IP binding открывает TCP 3280; при подключении стандартного AVRDUDE ESP переключает общий UART на Nano Old Bootloader 57600 и через GPIO4/NPN аппаратно сбрасывает Nano. Добавлен `tools/flash_nano_over_wifi.ps1`. Требуется один раз физически собрать reset stage из имеющихся BC548 + 1,5 kΩ + 4,7 kΩ; после первого реального write+verify будущие Nano updates можно выполнять по Wi-Fi без USB-доступа. Подробно: `02_Архитектура_и_решения/NANO_REMOTE_FLASH.md`.

''',
    'STATUS current stage')
status_path.write_text(status, encoding='utf-8')

start_path = root / '00_AI_START_HERE.md'
start = start_path.read_text(encoding='utf-8')
start = replace_once(
    start,
    '## 11. Текущий следующий шаг\n',
    '''## 11. Текущий следующий шаг

**Обновление 2026-10-17 / D-126:** добавлен source-кандидат ESP `ARDU_ESP_V4_NANO_REMOTE` и service reset architecture для последующих Nano updates по Wi-Fi: GPIO4 через BC548 управляет RESET Nano, стандартный AVRDUDE работает через TCP bridge 3280 на штатный Old Bootloader 57600. Первый physical remote write/verify ещё pending; до него USB остаётся recovery.

''',
    'AI start next step')
start_path.write_text(start, encoding='utf-8')

# Legacy handoff files: append only a compact current note if they exist.
for rel in ['01_СТАТУС_ПРОЕКТА.md', '02_СЛЕДУЮЩИЙ_ШАГ.md']:
    p = root / rel
    if p.exists():
        text = p.read_text(encoding='utf-8')
        text += '\n\n2026-10-17 D-126: ESP V4 Nano remote programming source/CI ready; add GPIO4→BC548→Nano RESET service line, then physical write+verify gate. See `02_Архитектура_и_решения/NANO_REMOTE_FLASH.md`.\n'
        p.write_text(text, encoding='utf-8')

# Minimal YAML handoff note without trying to reformat the historical schema.
yaml_path = root / '03_СОСТОЯНИЕ_ПРОЕКТА.yaml'
if yaml_path.exists():
    y = yaml_path.read_text(encoding='utf-8')
    y += '\n# 2026-10-17 D-126: ESP V4 remote Nano programming source/CI ready; physical reset-stage + first remote write/verify pending.\n'
    yaml_path.write_text(y, encoding='utf-8')

print('PASS: generated ESP V4 remote Nano programmer, helper and canonical docs')
