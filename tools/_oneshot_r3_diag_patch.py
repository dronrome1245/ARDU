from pathlib import Path
import re

SRC = Path("05_WiFi_и_приложение/esp8266_nano_remote_v4/esp8266_nano_remote_v4.ino")
s = SRC.read_text()


def once(old: str, new: str, label: str) -> None:
    global s
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, got {count}")
    s = s.replace(old, new, 1)


once(
    "ARDU ESP8266 V4 R2: Warm Dawn V3 + lossless authenticated remote Nano programming bridge.",
    "ARDU ESP8266 V4 R3 DIAG: Warm Dawn V3 + instrumented authenticated remote Nano programming bridge.",
    "banner",
)

once(
    'constexpr char FW_NAME[] = "ARDU_ESP_V4_NANO_REMOTE_R2";',
    'constexpr char FW_NAME[] = "ARDU_ESP_V4_NANO_REMOTE_R3_DIAG";',
    "fw name",
)

once(
    'String nanoProgramLastResult = "never";\n',
    '''String nanoProgramLastResult = "never";

// Latched diagnostics for the most recently started Nano programming session.
// ESP8266 HardwareSerial::hasOverrun()/hasRxError() return-and-clear their
// core flags, so preserve evidence until the next session starts.
bool nanoProgramUartOverrun = false;
bool nanoProgramUartRxError = false;
uint32_t nanoProgramTcpToUartBytes = 0;
uint32_t nanoProgramUartToTcpBytes = 0;
uint32_t nanoProgramMaxSerialAvailable = 0;
uint32_t nanoProgramMaxTcpAvailable = 0;
''',
    "diag globals",
)

once(
    '''void finishNanoProgramSession(const char* result) {
  if (nanoProgramClient) nanoProgramClient.stop();
  restoreNormalNanoUart();
  nanoProgramActive = false;
  clearNanoProgramArm(result);
  nanoProgramLastTrafficMs = millis();
}

void startNanoProgramSession(WiFiClient client) {''',
    '''void latchNanoProgramUartErrors() {
  if (Serial.hasOverrun()) nanoProgramUartOverrun = true;
  if (Serial.hasRxError()) nanoProgramUartRxError = true;
}

void finishNanoProgramSession(const char* result) {
  // Capture UART RX faults before Serial.end() destroys the programming UART.
  latchNanoProgramUartErrors();
  if (nanoProgramClient) nanoProgramClient.stop();
  restoreNormalNanoUart();
  nanoProgramActive = false;
  clearNanoProgramArm(result);
  nanoProgramLastTrafficMs = millis();
}

void startNanoProgramSession(WiFiClient client) {''',
    "finish helper",
)

once(
    '''  Serial.setRxBufferSize(Cfg::NANO_PROGRAM_UART_RX_BUFFER);
  Serial.begin(Cfg::NANO_PROGRAM_BAUD);
  Serial.setTimeout(50);

  pulseNanoReset();''',
    '''  Serial.setRxBufferSize(Cfg::NANO_PROGRAM_UART_RX_BUFFER);
  Serial.begin(Cfg::NANO_PROGRAM_BAUD);
  Serial.setTimeout(50);

  nanoProgramUartOverrun = false;
  nanoProgramUartRxError = false;
  nanoProgramTcpToUartBytes = 0;
  nanoProgramUartToTcpBytes = 0;
  nanoProgramMaxSerialAvailable = 0;
  nanoProgramMaxTcpAvailable = 0;
  // Clear stale core flags before this programming session begins.
  (void)Serial.hasOverrun();
  (void)Serial.hasRxError();

  pulseNanoReset();''',
    "start diagnostics",
)

once(
    '''    bool traffic = false;
    uint8_t buf[128];

    int available = nanoProgramClient.available();
    if (available > 0) {''',
    '''    bool traffic = false;
    uint8_t buf[128];

    latchNanoProgramUartErrors();

    int available = nanoProgramClient.available();
    if (available > 0) {
      if (static_cast<uint32_t>(available) > nanoProgramMaxTcpAvailable) {
        nanoProgramMaxTcpAvailable = static_cast<uint32_t>(available);
      }''',
    "tcp max",
)

once(
    '''        if (!writeNanoProgramSerialAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("uart_write_stall");
          return false;
        }
        traffic = true;''',
    '''        if (!writeNanoProgramSerialAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("uart_write_stall");
          return false;
        }
        nanoProgramTcpToUartBytes += static_cast<uint32_t>(got);
        traffic = true;''',
    "tcp to uart count",
)

once(
    '''    available = Serial.available();
    if (available > 0) {
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);''',
    '''    available = Serial.available();
    if (available > 0) {
      if (static_cast<uint32_t>(available) > nanoProgramMaxSerialAvailable) {
        nanoProgramMaxSerialAvailable = static_cast<uint32_t>(available);
      }
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);''',
    "uart max",
)

once(
    '''        if (!writeNanoProgramTcpAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("tcp_write_stall");
          return false;
        }
        traffic = true;''',
    '''        if (!writeNanoProgramTcpAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("tcp_write_stall");
          return false;
        }
        nanoProgramUartToTcpBytes += static_cast<uint32_t>(got);
        latchNanoProgramUartErrors();
        traffic = true;''',
    "uart to tcp count",
)

once(
    '''  body += F(",\\"reset_gpio\\":"); body += Cfg::NANO_RESET_GPIO;
  body += F(",\\"last_result\\":\\""); body += jsonEscape(nanoProgramLastResult); body += '"';''',
    '''  body += F(",\\"reset_gpio\\":"); body += Cfg::NANO_RESET_GPIO;
  body += F(",\\"last_result\\":\\""); body += jsonEscape(nanoProgramLastResult); body += '"';
  body += F(",\\"uart_rx_buffer_size\\":"); body += Cfg::NANO_PROGRAM_UART_RX_BUFFER;
  body += F(",\\"uart_overrun\\":"); body += nanoProgramUartOverrun ? F("true") : F("false");
  body += F(",\\"uart_rx_error\\":"); body += nanoProgramUartRxError ? F("true") : F("false");
  body += F(",\\"tcp_to_uart_bytes\\":"); body += nanoProgramTcpToUartBytes;
  body += F(",\\"uart_to_tcp_bytes\\":"); body += nanoProgramUartToTcpBytes;
  body += F(",\\"max_uart_available\\":"); body += nanoProgramMaxSerialAvailable;
  body += F(",\\"max_tcp_available\\":"); body += nanoProgramMaxTcpAvailable;''',
    "status diagnostics",
)

# Keep the event endpoint asynchronous-only. Also strip stale binary bytes that
# can precede ARDU1 when the UART switches from bootloader baud back to 115200.
once(
    '''      if (asyncLength) {
        asyncLine[asyncLength] = '\\0';
        pushEventLine(asyncLine);
        asyncLength = 0;
      }''',
    '''      if (asyncLength) {
        asyncLine[asyncLength] = '\\0';
        char* boot = strstr(asyncLine, "ARDU1 ");
        if (boot) pushEventLine(boot);
        else if (asyncLine[0] == 'V' && asyncLine[1] == ' ') pushEventLine(asyncLine);
        asyncLength = 0;
      }''',
    "async filter",
)

SRC.write_text(s)

status = Path("STATUS.md")
t = status.read_text()
marker = "## Текущий этап\n"
if marker not in t:
    raise RuntimeError("STATUS marker missing")
note = '''
**2026-10-17 — D-127 / R6 фактически запускается, но полный Wi-Fi verify всё ещё FAIL; подготовлен ESP V4 R3 DIAG.** На физическом ESP R2 повторный Nano upload снова дал `write 29 722 B = 100%`, а verify сорвался почти сразу. Отдельный `flash:v:` в новом bootloader-сеансе дошёл до 48% и потерял sync с тем же `expected 0x14`, затем эксперимент с 64-byte flash page дошёл только до 7%; значит переход write→verify и размер страницы не являются достаточным объяснением. После холодного старта Nano штатно отвечает `/api/status`, а аппаратный reset-only сеанс дал сырую startup-строку `�ARDU1 7` + `V 9 1`, что является физическим доказательством запуска Nano R6/FW_REV7, но **не заменяет обязательный byte-for-byte AVRDUDE verify**. R3 DIAG сохраняет R2 transport и добавляет latch `Serial.hasOverrun()/hasRxError()`, счётчики байтов и максимальную очередь UART/TCP в `/api/system/nano-program/status`; также фильтрует residual `D ...` из `/api/events` и очищает мусорный префикс перед `ARDU1`. Следующий gate: OTA R3 DIAG → только verify → сразу status diagnostics; Nano больше не записывать до этого результата.

'''
status.write_text(t.replace(marker, marker + note, 1))

hist = Path("HISTORY.md")
h = hist.read_text()
h += '''
## 2026-10-17 — повторный Nano Wi-Fi verify и физическое подтверждение R6 startup

- На физическом ESP `ARDU_ESP_V4_NANO_REMOTE_R2` повторный upload Nano R6 через `net:192.168.0.4:3280`, 57600 снова записал 29 722 B до 100%, но verify потерял STK500 sync почти сразу.
- Отдельный verify-only в новом bootloader-сеансе дошёл до 48% и завершился теми же `expected 0x14`, `0xe6/0xff/0xc4`; вариант с 64-byte flash page дошёл до 7% и также FAIL.
- После полного power-cycle `/api/status` снова PASS: RTC valid, current limit 3000 mA, normal UART работает.
- Reset-only через ESP GPIO4 дал сырую startup-строку `�ARDU1 7` и событие `V 9 1`; следовательно, записанная Nano R6/FW_REV7 фактически стартует на железе. Это не отменяет требование полного AVRDUDE verify для remote-programming hardware PASS.
- Подготовлен ESP V4 R3 DIAG: latched UART overrun/RX-error, byte counters/max queue diagnostics и фильтрация residual UART строк в events.
'''
hist.write_text(h)

srcs = Path("SOURCES.md")
q = srcs.read_text()
nums = [int(x) for x in re.findall(r"^## S-(\d+)", q, re.M)]
nxt = max(nums) + 1 if nums else 1
q += f'''
## S-{nxt:03d} — Nano R6 remote-programming diagnostics 2026-10-17

Пользовательские аппаратные логи: R2 upload Nano R6 (`29 722 B`) дважды достигал write 100%, но полный verify терял STK500 sync; verify-only в отдельном сеансе достигал 48%, эксперимент с 64-byte page — 7%. После power-cycle нормальный API восстановился. Reset-only через GPIO4 дал `�ARDU1 7` и `V 9 1`, физически подтверждая запуск FW_REV7.

Для диагностики ESP8266 Arduino Core использованы официальные `HardwareSerial::hasOverrun()` / `hasRxError()`; нижний UART HAL документирует их как return-and-clear flags. Источники: `https://github.com/esp8266/Arduino/blob/master/cores/esp8266/HardwareSerial.h`, `https://github.com/esp8266/Arduino/blob/master/cores/esp8266/uart.h`.
'''
srcs.write_text(q)

remote = Path("02_Архитектура_и_решения/NANO_REMOTE_FLASH.md")
r = remote.read_text()
r += '''
## R3 DIAG — локализация нестабильного verify (2026-10-17)

Физические тесты R2 показали, что полный write устойчиво доходит до 100%, тогда как длинный Nano→ESP→TCP readback verify падает в непостоянной точке (около 1/7/22/48%). Отдельный verify-only исключил переход write→verify; 64-byte page test не устранил ошибку. R6 при этом физически запускается (`ARDU1 7`).

R3 DIAG не ослабляет критерий PASS и не отключает verify. В `/api/system/nano-program/status` после сеанса сохраняются: `uart_overrun`, `uart_rx_error`, `tcp_to_uart_bytes`, `uart_to_tcp_bytes`, `max_uart_available`, `max_tcp_available`, `uart_rx_buffer_size`. Следующий тест — verify-only и немедленное чтение этих полей.
'''
remote.write_text(r)

start = Path("00_AI_START_HERE.md")
a = start.read_text()
key = "## 11. Текущий следующий шаг\n"
if key not in a:
    raise RuntimeError("start marker missing")
latest = '''
**Обновление 2026-10-17 / D-127 R3 DIAG:** Nano R6 теперь физически подтверждена по startup `ARDU1 7` после ESP-driven reset, но remote-programming hardware PASS всё ещё закрыт, потому что полный AVRDUDE verify нестабилен. R2 write достигает 100%; verify-only падал в разных точках до 48%, 64-byte page не помог. Следующий gate: OTA `ARDU_ESP_V4_NANO_REMOTE_R3_DIAG` → verify-only → `/api/system/nano-program/status` с UART overrun/RX-error и byte/queue counters. Не переписывать Nano до этого диагностического результата.

'''
start.write_text(a.replace(key, key + latest, 1))
