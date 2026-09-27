# ARDU — L01 R1 ordinary light hardware test

Дата: 2026-09-27

## Scope

L01 R1 — первый отдельный аппаратный слой обычного освещения.

Проверяется только одно уже подтверждённое кольцо:

- D6;
- 43 WS2812B;
- D7 удерживается LOW;
- постоянный RGB-white 255,255,255;
- ON/OFF;
- BRIGHT 0..255;
- FastLED current limit = 500 mA для текущего однокольцевого стенда;
- UART/FastLED baseline.

В R1 нет clap detector и нет общей EEPROM persistence.

Startup R1 = ON, чтобы отдельно подтвердить требование «после появления питания обычный свет включается». Отличение сервисного Reset от реального cold power-on будет тестироваться в общей state machine, не в этом изолированном скетче.

## 1. Verify + Upload

Открыть:

04_Прошивка/nano_l01_r1/nano_l01_r1.ino

Arduino IDE:

- Board: Arduino Nano;
- Processor: ATmega328P (Old Bootloader);
- Serial Monitor: 115200 baud;
- line ending: Newline.

После загрузки ожидается:

ARDU NANO L01 R1 READY
ORDINARY WHITE LIGHT; STARTUP ON; SINGLE-RING LIMIT 500MA
STATUS FW=L01 REV=1 MODE=L01 POWER=ON ...

Кольцо должно автоматически загореться белым.

## 2. Startup/status

Отправить STATUS.

Проверить:

- FW=L01 REV=1;
- MODE=L01;
- POWER=ON;
- BRIGHTNESS=64;
- WHITE=RGB255,255,255;
- LIMIT_MA=500;
- LEDS=43;
- CLAP=NOT_IN_R1;
- PERSISTENCE=NO_R1.

Визуально все 43 LED должны светить одинаковым белым без цветных точек и мерцания.

## 3. Яркость

Отправить последовательно, задерживаясь на каждом значении несколько секунд:

BRIGHT 8
BRIGHT 32
BRIGHT 64
BRIGHT 128
BRIGHT 255

Критерии:

- яркость растёт в нижней/средней части диапазона;
- при больших значениях рост может перестать быть пропорциональным — это ожидаемо из-за текущего лимита 500 mA;
- Nano не перезагружается;
- лента не мигает и не меняет оттенок случайно.

На BRIGHT 255 оставить свет включённым около 10 s и выполнить STATUS. UPTIME_MS должен продолжать расти.

Затем вернуть:

BRIGHT 64

## 4. ON/OFF

OFF
STATUS

Норма:

- кольцо полностью погашено;
- POWER=OFF.

Затем:

ON
STATUS

Норма:

- белый свет возвращается;
- POWER=ON;
- установленная BRIGHTNESS=64 сохраняется в RAM без reset.

Повторить ON/OFF 3–5 раз. Самопроизвольных вспышек быть не должно.

## 5. BRIGHT 0 semantics

BRIGHT 0
STATUS

При POWER=ON кольцо визуально погашено, но STATUS остаётся POWER=ON BRIGHTNESS=0.

Затем:

BRIGHT 64

Свет возвращается без OFF/ON.

## 6. UART stress

Оставить:

ON
BRIGHT 64

Быстро отправить:

STATUS
PING
STATUS
BRIGHT 32
STATUS
BRIGHT 64
PING
STATUS

Критерии:

- каждый PING → PONG;
- нет ERR UNKNOWN_COMMAND;
- строки не обрезаются;
- стабильно LEDS=43;
- UPTIME_MS растёт;
- изменение brightness применяется;
- белый свет не получает случайных цветных/яркостных артефактов.

## 7. Standalone restart behavior

Оставить:

OFF
BRIGHT 32

Нажать Reset Nano.

Для этого отдельного R1 ожидается:

- startup снова POWER=ON BRIGHTNESS=64;
- кольцо включается белым.

Это не тест финальной persistence/state-machine. R1 намеренно использует defaults после reset.

## PASS L01 R1

L01 R1 считается пройденным, если:

1. compile + upload прошли;
2. после startup кольцо автоматически включается белым;
3. все LED одинакового белого цвета;
4. BRIGHT регулирует яркость;
5. current-limit режим не вызывает reset/мерцание на BRIGHT 255;
6. ON/OFF работает стабильно;
7. BRIGHT 0 отличается от POWER=OFF только состоянием логики;
8. UART stress проходит без повреждения строк.

После PASS L01 R1 следующий слой — отдельный MAX9814 clap diagnostic. В нём сначала подбирается детектор и проверяются ложные срабатывания, и только затем clap control объединяется с L01.
