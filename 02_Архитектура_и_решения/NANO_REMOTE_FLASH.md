# Удалённая прошивка Arduino Nano через ESP8266 — R1

Дата: 2026-10-17  
Статус: SOURCE/CI PASS; физический монтаж reset-цепи и первый Wi-Fi upload владельцем pending.

## 1. Что реализовано

ESP `ARDU_ESP_V4_NANO_REMOTE` остаётся обычным HTTP/UART шлюзом и получает сервисный режим удалённой прошивки Nano. После авторизации ESP на 30 секунд разрешает TCP-соединение только с того IP, который выполнил arm-запрос. При подключении `avrdude` ESP переключает общий UART с 115200 на 57600, аппаратно сбрасывает Nano в её штатный Old Bootloader и прозрачно передаёт байты `avrdude ↔ Nano`. После завершения TCP-сессии ESP возвращает UART 115200.

Формат HEX, запись Flash и verify выполняет стандартный AVRDUDE, а не самодельный flash-parser ESP. Для фактической Nano проект уже подтверждал `ATmega328P (Old Bootloader)`; официальный Arduino AVR `boards.txt` задаёт для него `protocol=arduino`, `upload.speed=57600`, `maximum_size=30720`.

## 2. Дополнительная reset-цепь

**Тип соединения:** ESP управляет RESET Nano **через NPN-транзистор как открытый коллектор**. Nano RESET напрямую к GPIO ESP не подключать: RESET Nano подтянут в 5-вольтовом домене, а ESP8266 не является 5-V tolerant.

```text
ESP GPIO4 ── 1.5 kΩ ──┬──► B  BC548
                       │
                     4.7 kΩ
                       │
GND ESP/Nano ──────────┴──► E  BC548

Nano RESET ─────────────────► C  BC548
```

- `GPIO4 LOW` → транзистор закрыт → Nano работает от штатной RESET pull-up;
- `GPIO4 HIGH` → транзистор открыт → RESET притянут к GND;
- 4.7 kΩ B→GND удерживает транзистор выключенным во время старта ESP;
- земли ESP и Nano уже должны быть общими по рабочему UART;
- точный порядок физических ножек `C/B/E` конкретного BC548 перед пайкой сверить по маркировке/даташиту экземпляра, не угадывать по форме корпуса.

GPIO4 выбран потому, что текущая ESP V3 его не использует, а у ESP-12 он не является boot strap pin. GPIO0/GPIO2/GPIO15 для этой функции не используются.

## 3. Сервисный API

`GET /api/ping` дополнительно:

```json
{"nano_remote_flash":true,"nano_program_port":3280,"nano_program_baud":57600}
```

`GET /api/system/nano-program/status` возвращает `supported/active/armed/port/baud/reset_gpio/last_result`.

`POST /api/system/nano-program/arm`:

```json
{"password":"CURRENT_ESP_OTA_PASSWORD"}
```

Ограничения: только домашний Station; пароль совпадает с локальным `ARDU_OTA_PASSWORD`; ARDU-DIRECT не разрешает программирование Nano; arm действует 30 s и привязан к IP компьютера.

## 4. Прошивка с Windows

В репозитории: `tools/flash_nano_over_wifi.ps1`. Скрипт сам находит AVRDUDE из Arduino IDE, спрашивает текущий OTA-пароль без вывода на экран, arm-ит ESP и запускает стандартную запись + verify через `-P net:<IP>:3280`.

Пример:

```powershell
cd C:\ARDU
.\tools\flash_nano_over_wifi.ps1 `
  -HexPath .\path\to\nano_warm_dawn_v6.ino.hex `
  -DeviceIp 192.168.0.4
```

Во время удалённой прошивки Nano USB-кабель не подключать. Питание ARDU не выключать и Wi-Fi не переключать. Если TCP/upload оборвался, повторно выполнить arm и запуск скрипта: аппаратный GPIO4→RESET позволяет снова войти в bootloader даже при повреждённом приложении Nano.

## 5. Первый физический gate

1. При снятом питании собрать транзисторную RESET-цепь.
2. Проверить мультиметром отсутствие прямого соединения Nano RESET↔ESP GPIO4 без резистора/транзистора.
3. Включить ARDU, проверить обычный `/api/status`.
4. OTA установить ESP V4 и убедиться, что `/api/ping` содержит `nano_remote_flash:true`.
5. Первый раз удалённо записать Nano R6 HEX скриптом.
6. После verify проверить `/api/status`, `/api/settings`, `/api/time`, L01 и Night.
7. Не считать механизм hardware-PASS до первого реального write+verify+post-status.
