# ARDU — DAWN R2 reset-recovery test

Дата: 2026-09-26

## Что проверяет R2

DAWN R2 сохраняет только переходы состояния рассвета в EEPROM:

- START;
- COMPLETE/HOLD;
- STOP/IDLE.

Текущий прогресс не записывается периодически. После reset прогресс вычисляется заново из DS3231:

`elapsed = RTC_now - RTC_start`.

Это снижает число записей EEPROM.

## 1. Базовый recovery во время активного рассвета

После Verify/Upload:

```
STATUS
DAWN TEST 30
```

Подождать примерно 10 секунд и нажать Reset Nano.

После старта ожидается:

```
ARDU NANO DAWN R2 READY
EVENT DAWN_RECOVER PHASE=RUNNING SOURCE=TEST ELAPSED_S=...
```

В `STATUS`:

- `FW=DAWN REV=2`;
- `DAWN=RUNNING`;
- `SOURCE=TEST`;
- `RUNTIME_SAVED=YES`;
- `RECOVERED=YES`.

Визуально яркость после reset должна продолжиться примерно с той же точки, а не начаться с нуля.

Полный тест должен закончиться примерно через 30 секунд от первоначального `DAWN TEST 30`, а не через 30 секунд после reset.

## 2. HOLD recovery

После `EVENT DAWN_COMPLETE` нажать Reset.

Ожидается:

```
EVENT DAWN_RECOVER PHASE=HOLD SOURCE=TEST
```

Кольцо остаётся на конечной яркости/цвете.

Затем:

```
DAWN STOP
STATUS
```

Норма: `DAWN=IDLE`, кольцо выключено.

## 3. STOP persistence

После `DAWN STOP` нажать Reset ещё раз.

Норма:

- событие `DAWN_RECOVER` не появляется;
- `DAWN=IDLE`;
- кольцо остаётся выключенным.

## 4. Alarm recovery

Для ускоренной проверки:

```
FADEMIN 1
ALARM CLEARLAST
ALARMSET <следующая минута>
ALARM ON
```

После:

```
EVENT ALARM_TRIGGER ...
EVENT DAWN_START SOURCE=ALARM DURATION_S=60
```

подождать 15–25 секунд и нажать Reset.

Ожидается:

```
EVENT DAWN_RECOVER PHASE=RUNNING SOURCE=ALARM ...
```

Рассвет должен завершиться по исходному RTC-времени, без второго `ALARM_TRIGGER`.

После теста:

```
DAWN STOP
FADEMIN 30
```

## UART regression

Во время RUNNING после recovery несколько раз подряд выполнить `STATUS`.
`UNKNOWN_COMMAND` возвращаться не должен.
