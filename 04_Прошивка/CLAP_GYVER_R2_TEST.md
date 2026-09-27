# ARDU — CLAP GYVER R2 MAX9814 test

Дата: 2026-09-27

## Почему R2

GYVER R1 стартовал с параметром GyverLamp2 TRSH=250 и не распознавал хлопки на ARDU MAX9814.

Одновременно startup DC был ложным:
- startup DC=1019 / DC_OK=NO;
- позже ADC AVG=250 MIN=181 MAX=312 P2P=131.

R2 сохраняет Gyver derivative state machine, но адаптирует вход под наш MAX9814:

- derivative threshold default = 150 — default самой GyverLibs/Clap;
- sequence timeout = 500 ms — как GyverLamp2;
- startup ADC прогревается 1200 ms + discard reads;
- после CLEAR/TRSH/TIMEOUT действует 700 ms command-click guard;
- первый rawMax только инициализирует derivative baseline;
- target gesture остаётся double clap.

## 1. Verify + Upload

Файл:

`04_Прошивка/nano_clap_gyver_r2/nano_clap_gyver_r2.ino`

Arduino Nano / ATmega328P (Old Bootloader).

Serial Monitor: 115200 / Newline.

Ожидается:

```
ARDU NANO CLAP GYVER R2 READY
GYVER CLAP DEFAULT TRSH=150; DOUBLE CLAP; COMMAND CLICK GUARD
STATUS ... DC≈рабочий ... DC_OK=YES ... CLAP_TRSH=150 CLAP_TIMEOUT_MS=500
```

После startup дождаться:

```
EVENT CLAP_READY
```

## 2. ADC

```
ADC
STATUS
```

Норма:
- DC_OK=YES;
- DC близок к текущему AVG;
- saturation отсутствует.

## 3. Одиночные хлопки при TRSH=150

```
CLEAR
```

Дождаться `EVENT CLAP_READY`.

Сделать 5 одиночных хлопков с паузой >1 s.

Ожидается для каждого распознанного хлопка:

```
EVENT CLAP ...
EVENT CLAP_SEQUENCE COUNT=1 ...
```

После:

```
STATS
```

DOUBLE_EVENTS должен оставаться 0.

## 4. Если хлопки не ловятся при 150

Не менять timeout.

Включить:

```
TRACE ON
```

Сделать 3 отдельных хлопка, затем:

```
TRACE OFF
```

Прислать TRACE вокруг хлопков. Важен максимальный абсолютный DER.

Дополнительно можно проверить:

```
TRSH 100
```

Дождаться CLAP_READY и повторить 5 хлопков.

Не опускать threshold ниже 100 до анализа TRACE.

## 5. Double clap

Когда одиночный clap стабильно распознаётся:

```
CLEAR
```

Дождаться CLAP_READY.

Сделать 5 естественных пар хлоп-хлоп с интервалом примерно 150..450 ms.

Успешная пара:

```
EVENT CLAP ...
EVENT CLAP ...
EVENT CLAP_SEQUENCE COUNT=2 ...
EVENT DOUBLE_CLAP
```

После:

```
STATS
```

## 6. False positives

Отдельно:

- 10 mouse clicks → STATS;
- 30 s speech → STATS;
- 30 s music → STATS.

Главный критерий не SINGLE_EVENTS, а DOUBLE_EVENTS.

## PASS

R2 проходит если:

1. startup DC валиден;
2. найден минимальный рабочий derivative threshold;
3. single clap распознаётся стабильно;
4. double clap распознаётся стабильно;
5. одиночные mouse clicks не образуют DOUBLE_CLAP;
6. speech/music не создают стабильные DOUBLE_CLAP.

После PASS detector можно объединять с L01.
