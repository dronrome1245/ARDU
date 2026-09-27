# ARDU — M06 R1 strobe hardware test

Дата: 2026-09-27

## Scope

M06 R1 переносит механику стробоскопа ColorMusic v2.10, но с отдельными firmware-ограничениями ARDU:

- startup = OFF;
- никакого автоматического старта;
- частота `0.5..3.0 Hz` (`FREQ10 5..30`);
- default = `1.0 Hz`;
- фиксированный duty = 20%;
- яркость R1 ограничена `1..128`;
- одна ручная сессия автоматически останавливается через 15 s;
- `OFF` останавливает немедленно;
- FastLED/UART baseline из M05 R4: interrupts ON + 5 ms RX guard.

Частотный предел не зависит от приложения/ESP: Nano отвергает значение выше 3.0 Hz.

## 1. Verify + Upload

Открыть:

`04_Прошивка/nano_m06_r1/nano_m06_r1.ino`

Arduino IDE:

- Board: Arduino Nano;
- Processor: ATmega328P (Old Bootloader);
- Serial Monitor: 115200 baud, Newline.

После startup ожидается:

```
ARDU NANO M06 R1 READY
STROBE DEFAULT OFF; MANUAL ON ONLY; AUTO-STOP 15S
STATUS FW=M06 REV=1 MODE=M06 POWER=OFF ...
```

Кольцо после загрузки не должно мигать.

## 2. Проверка firmware limits без запуска

Отправить:

```
FREQ10 31
BRIGHT 129
FREQ10 30
BRIGHT 64
SMOOTH 200
STATUS
```

Ожидается:

- `FREQ10 31` → `ERR BAD_FREQ10 RANGE=5..30`;
- `BRIGHT 129` → `ERR BAD_BRIGHTNESS RANGE=1..128`;
- валидные команды → `OK`;
- STATUS: `FREQ10=30 PERIOD_MS=334`, `MAX_FREQ10=30`, `MAX_BRIGHT=128`.

Вернуть перед визуальным тестом:

```
FREQ10 10
BRIGHT 64
SMOOTH 200
```

## 3. Ручной старт / default 1 Hz

Отправить:

```
ON
```

Ожидается `OK STROBE=ON`.

Визуально:

- белые вспышки примерно 1 раз/с;
- режим не реагирует на микрофон/музыку;
- случайных цветов нет.

Через несколько секунд:

```
STATUS
```

`POWER=ON`, а `RUN_REMAIN_MS` уменьшается.

## 4. Частота

Каждый тест запускается командой ON и длится только несколько секунд.

Медленно:

```
OFF
FREQ10 5
ON
```

Ожидается примерно 0.5 вспышки/с.

Затем:

```
OFF
FREQ10 20
ON
```

Ожидается примерно 2 вспышки/с.

Максимум R1:

```
OFF
FREQ10 30
ON
```

Ожидается не более примерно 3 вспышек/с.

После проверки:

```
OFF
FREQ10 10
```

## 5. Плавность

С default frequency:

```
SMOOTH 20
ON
```

Вспышка должна иметь заметно более плавное нарастание/угасание.

Затем:

```
OFF
SMOOTH 255
ON
```

Переход должен быть значительно резче.

Вернуть:

```
OFF
SMOOTH 200
```

## 6. Яркость

Коротко сравнить:

```
BRIGHT 20
ON
```

и:

```
OFF
BRIGHT 96
ON
```

Должна меняться общая яркость вспышки.

Вернуть:

```
OFF
BRIGHT 64
```

## 7. Auto-stop 15 s

Установить defaults:

```
FREQ10 10
SMOOTH 200
BRIGHT 64
ON
```

Ничего больше не отправлять.

Примерно через 15 s ожидается:

```
EVENT STROBE_AUTO_STOP
```

Кольцо должно погаснуть самостоятельно.

После:

```
STATUS
```

Ожидается `POWER=OFF RUN_REMAIN_MS=0`.

## 8. OFF немедленно

```
ON
```

Через 1–2 вспышки:

```
OFF
```

Кольцо должно погаснуть сразу, без ожидания конца 15-секундной сессии.

## 9. UART stress

Запустить:

```
FREQ10 20
ON
```

Пока M06 активен, быстро отправить:

```
STATUS
STATUS
STATUS
PING
STATUS
PING
STATUS
```

Критерий:

- нет повреждённых команд;
- нет `ERR UNKNOWN_COMMAND`;
- ответы читаемые;
- `LEDS=43` не теряет символы;
- `UPTIME_MS` растёт;
- визуальный вывод не зависает.

Закончить:

```
OFF
FREQ10 10
BRIGHT 64
SMOOTH 200
STATUS
```

## PASS M06 R1

M06 R1 считается пройденным если:

1. Compile + Upload прошли;
2. startup всегда OFF;
3. значения >3.0 Hz отвергаются Nano;
4. BRIGHT >128 отвергается в R1;
5. 0.5/2.0/3.0 Hz визуально различаются;
6. SMOOTH влияет на фронт вспышки;
7. BRIGHT влияет на яркость;
8. auto-stop срабатывает примерно через 15 s;
9. OFF останавливает немедленно;
10. UART stress проходит без повреждения строк.

После PASS M06 следующий отдельный слой: L01 ordinary light + clap diagnostic.
