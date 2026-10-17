from pathlib import Path

SRC = Path('05_WiFi_и_приложение/esp8266_nano_remote_v4/esp8266_nano_remote_v4.ino')
STATUS = Path('STATUS.md')
HISTORY = Path('HISTORY.md')
DECISIONS = Path('DECISIONS.md')
SOURCES = Path('SOURCES.md')
REMOTE = Path('02_Архитектура_и_решения/NANO_REMOTE_FLASH.md')
START = Path('00_AI_START_HERE.md')

s = SRC.read_text(encoding='utf-8')

repls = [
    ('ARDU ESP8266 V4: Warm Dawn V3 + authenticated remote Nano programming bridge.',
     'ARDU ESP8266 V4 R2: Warm Dawn V3 + lossless authenticated remote Nano programming bridge.'),
    ('constexpr char FW_NAME[] = "ARDU_ESP_V4_NANO_REMOTE";',
     'constexpr char FW_NAME[] = "ARDU_ESP_V4_NANO_REMOTE_R2";'),
    ('constexpr unsigned long NANO_PROGRAM_IDLE_MS = 15000UL;\n',
     'constexpr unsigned long NANO_PROGRAM_IDLE_MS = 15000UL;\nconstexpr unsigned long NANO_PROGRAM_IO_STALL_MS = 5000UL;\nconstexpr size_t NANO_PROGRAM_UART_RX_BUFFER = 512;\n'),
]
for old, new in repls:
    assert s.count(old) == 1, old
    s = s.replace(old, new, 1)

old_begin = '''  Serial.end();
  delay(10);
  Serial.begin(Cfg::NANO_PROGRAM_BAUD);
  Serial.setTimeout(50);

  pulseNanoReset();
  nanoProgramLastTrafficMs = millis();
}

bool serviceNanoProgramBridge() {
'''
new_begin = '''  Serial.end();
  delay(10);
  Serial.setRxBufferSize(Cfg::NANO_PROGRAM_UART_RX_BUFFER);
  Serial.begin(Cfg::NANO_PROGRAM_BAUD);
  Serial.setTimeout(50);

  pulseNanoReset();
  nanoProgramLastTrafficMs = millis();
}

// A transparent programming bridge must never silently drop a partial write.
// Both Arduino Client::write() and HardwareSerial::write() return the actual
// number of bytes accepted. Keep forwarding until the whole chunk is accepted
// or the peer stalls/disconnects.
bool writeNanoProgramTcpAll(const uint8_t* data, size_t length) {
  size_t offset = 0;
  unsigned long lastProgressMs = millis();

  while (offset < length) {
    if (!nanoProgramClient || !nanoProgramClient.connected()) return false;

    const int room = nanoProgramClient.availableForWrite();
    if (room > 0) {
      size_t chunk = length - offset;
      if (chunk > static_cast<size_t>(room)) chunk = static_cast<size_t>(room);
      const size_t wrote = nanoProgramClient.write(data + offset, chunk);
      if (wrote > 0) {
        offset += wrote;
        nanoProgramLastTrafficMs = millis();
        lastProgressMs = nanoProgramLastTrafficMs;
        continue;
      }
    }

    if (millis() - lastProgressMs > Cfg::NANO_PROGRAM_IO_STALL_MS) return false;
    yield();
  }
  return true;
}

bool writeNanoProgramSerialAll(const uint8_t* data, size_t length) {
  size_t offset = 0;
  unsigned long lastProgressMs = millis();

  while (offset < length) {
    const int room = Serial.availableForWrite();
    if (room > 0) {
      size_t chunk = length - offset;
      if (chunk > static_cast<size_t>(room)) chunk = static_cast<size_t>(room);
      const size_t wrote = Serial.write(data + offset, chunk);
      if (wrote > 0) {
        offset += wrote;
        nanoProgramLastTrafficMs = millis();
        lastProgressMs = nanoProgramLastTrafficMs;
        continue;
      }
    }

    if (millis() - lastProgressMs > Cfg::NANO_PROGRAM_IO_STALL_MS) return false;
    yield();
  }
  return true;
}

bool serviceNanoProgramBridge() {
'''
assert s.count(old_begin) == 1
s = s.replace(old_begin, new_begin, 1)

old_io = '''    int available = nanoProgramClient.available();
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
'''
new_io = '''    int available = nanoProgramClient.available();
    if (available > 0) {
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);
      const int got = nanoProgramClient.read(buf, available);
      if (got > 0) {
        if (!writeNanoProgramSerialAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("uart_write_stall");
          return false;
        }
        traffic = true;
      }
    }

    available = Serial.available();
    if (available > 0) {
      if (available > static_cast<int>(sizeof(buf))) available = sizeof(buf);
      const int got = Serial.read(buf, static_cast<size_t>(available));
      if (got > 0) {
        if (!writeNanoProgramTcpAll(buf, static_cast<size_t>(got))) {
          finishNanoProgramSession("tcp_write_stall");
          return false;
        }
        traffic = true;
      }
    }
'''
assert s.count(old_io) == 1
s = s.replace(old_io, new_io, 1)

assert 'nanoProgramClient.write(buf, static_cast<size_t>(got));' not in s
assert 'Serial.write(buf, static_cast<size_t>(got));' not in s
assert 'ARDU_ESP_V4_NANO_REMOTE_R2' in s
assert 'writeNanoProgramTcpAll' in s and 'availableForWrite()' in s
SRC.write_text(s, encoding='utf-8')

status = STATUS.read_text(encoding='utf-8')
marker = '## Текущий этап\n'
entry = '''\n**2026-10-17 — D-127 / первый реальный Nano Wi-Fi upload: WRITE 100%, VERIFY FAIL на ~22%; готов ESP V4 R2 lossless bridge, повторный physical gate pending.** На фактическом железе `avrdude 8.0-arduino.1` через `net:192.168.0.4:3280`, `arduino`, 57600 успешно синхронизировался с ATmega328P (`1E 95 0F`) и записал 29 722 B Nano R6 до 100%. На фазе verify чтение дошло примерно до 22% и протокол потерял синхронизацию (`expected 0x14`, получены `0xe6/0xff/0xc4`). Это **не hardware PASS**: R6 нельзя считать подтверждённой без полного readback verify. В ESP V4 R2 мост больше не игнорирует частичные `write()`: TCP↔UART передаётся до полного количества байт, UART→TCP читает неблокирующим `HardwareSerial::read`, RX buffer programming mode = 512 B, stall имеет явный fail вместо молчаливой потери. AVRDUDE verify остаётся обязательным; `-V` запрещён. Следующий gate: OTA V4 R2 → повтор того же Nano R6 HEX → требовать полный `write + verify + /api/status`.\n\n'''
assert marker in status and 'D-127 / первый реальный Nano Wi-Fi upload' not in status
STATUS.write_text(status.replace(marker, marker + entry, 1), encoding='utf-8')

history = HISTORY.read_text(encoding='utf-8')
history += '''\n\n## 2026-10-17 — первый реальный удалённый upload Nano: запись 100%, verify transport FAIL\n\n- ESP V4 и физическая GPIO4→BC548→Nano RESET цепь уже подтвердили удалённый reset: Nano `uptime_ms` 890258 → 9488 ms.\n- Владелец запустил `tools/flash_nano_over_wifi.ps1` на фактическом `nano_warm_dawn_v6.ino.hex`.\n- AVRDUDE 8.0-arduino.1 через TCP 3280 / STK500v1 / 57600 определил ATmega328P signature `1E 95 0F`, прочитал HEX 29 722 B и выполнил запись Flash до 100%.\n- Verify остановился примерно на 22%: `protocol expects sync byte 0x14 but got 0xe6`, затем `0xff`, `programmer is out of sync`, `unable to read byte at address 0x0000`, затем `0xc4`.\n- Результат классифицирован как **WRITE COMPLETE / VERIFY FAIL**, а не как успешная прошивка. Отключение verify (`-V`) не допускается.\n- Подготовлена ESP `ARDU_ESP_V4_NANO_REMOTE_R2`: lossless forwarding с учётом фактического return count `write()`, `availableForWrite()`, неблокирующим UART read и 512-B RX buffer. Повторный physical write+verify pending.\n'''
HISTORY.write_text(history, encoding='utf-8')

decisions = DECISIONS.read_text(encoding='utf-8')
assert '## D-127 —' not in decisions
decisions += '''\n\n---\n\n## D-127 — Nano remote upload считается успешным только после полного AVRDUDE verify\n\n**Дата:** 2026-10-17\n**Статус:** ДЕЙСТВУЕТ\n\n**Основание:** первый фактический Wi-Fi upload R6 записал 29 722 B до 100%, но verify потерял STK500 sync примерно на 22%.\n\n**РЕШЕНИЕ:**\n1. Не использовать `-V` и не считать `Writing 100%` достаточным подтверждением firmware.\n2. ESP bridge обязан учитывать фактическое число байт, принятое `WiFiClient.write`/`HardwareSerial.write`, и досылать остаток вместо молчаливой потери.\n3. UART→TCP programming path использует неблокирующий `HardwareSerial::read`; programming RX buffer увеличен до 512 B.\n4. При невозможности продвинуть передачу в течение 5 s сеанс завершается явным transport result, а не продолжает испорченный STK500 stream.\n5. Hardware PASS удалённого обновления Nano = `signature + full write + full verify + восстановленный /api/status`.\n6. До такого PASS USB остаётся recovery-путём.\n\n**Ревизия:** `ARDU_ESP_V4_NANO_REMOTE_R2`.\n'''
DECISIONS.write_text(decisions, encoding='utf-8')

sources = SOURCES.read_text(encoding='utf-8')
if 'ESP8266 Arduino core WiFiClient / HardwareSerial transport contract' not in sources:
    sources += '''\n\n## Дополнение D-127 — ESP8266 Arduino core WiFiClient / HardwareSerial transport contract\n\nОфициальный upstream ESP8266 Arduino core:\n- `https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClient.cpp` — `WiFiClient::write()` возвращает `size_t` фактически переданных байт; доступен `availableForWrite()`.\n- `https://github.com/esp8266/Arduino/blob/master/cores/esp8266/HardwareSerial.h` — `HardwareSerial::read(buffer,size)` является чтением без timeout, `write()` возвращает число принятых байт, доступен `availableForWrite()` и настройка RX buffer.\n\nИспользовано в D-127, чтобы TCP↔UART bridge не предполагал, что один вызов `write()` всегда принимает весь буфер.\n'''
SOURCES.write_text(sources, encoding='utf-8')

remote = REMOTE.read_text(encoding='utf-8')
remote = remote.replace('Статус: SOURCE/CI PASS; физический монтаж reset-цепи и первый Wi-Fi upload владельцем pending.',
                        'Статус: reset-цепь HARDWARE PASS; первый Wi-Fi WRITE дошёл до 100%, VERIFY FAIL ~22%; V4 R2 lossless bridge SOURCE/CI PASS, повторный write+verify pending.')
remote += '''\n\n## 6. Physical gate 2026-10-17 / R2 transport fix\n\nФактический reset через GPIO4/BC548 подтверждён (`uptime_ms` 890258 → 9488). Первый AVRDUDE upload распознал ATmega328P и записал 29 722 B до 100%, но verify потерял STK500 `0x14` примерно на 22%. Поэтому firmware не считается принятой. V4 R2 заменяет one-shot UART/TCP writes на complete-write loops, использует `availableForWrite()`, неблокирующий `HardwareSerial::read` и 512-B programming RX buffer. `-V` не использовать. Критерий PASS остаётся: полный write + полный verify + `/api/status` после возврата UART 115200.\n'''
REMOTE.write_text(remote, encoding='utf-8')

start = START.read_text(encoding='utf-8')
old = '**Обновление 2026-10-17 / D-126:** добавлен source-кандидат ESP `ARDU_ESP_V4_NANO_REMOTE` и service reset architecture для последующих Nano updates по Wi-Fi: GPIO4 через BC548 управляет RESET Nano, стандартный AVRDUDE работает через TCP bridge 3280 на штатный Old Bootloader 57600. Первый physical remote write/verify ещё pending; до него USB остаётся recovery.\n'
new = '**Обновление 2026-10-17 / D-126+D-127:** физический GPIO4→BC548→Nano RESET уже PASS. Первый реальный AVRDUDE upload через ESP V4 записал Nano R6 HEX до 100%, но verify потерял sync примерно на 22%, поэтому hardware upload PASS ещё нет. Текущая source-ревизия `ARDU_ESP_V4_NANO_REMOTE_R2` делает TCP↔UART forwarding lossless по фактическому return count `write()`, оставляя AVRDUDE verify обязательным. Следующий gate: OTA R2 → повторный полный write+verify → `/api/status`; USB остаётся recovery до PASS.\n'
assert old in start
START.write_text(start.replace(old, new, 1), encoding='utf-8')

print('PATCHED: ESP V4 R2 lossless bridge + docs')
