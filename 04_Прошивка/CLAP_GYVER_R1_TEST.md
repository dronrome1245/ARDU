# ARDU — CLAP GYVER R1 hardware test

Дата: 2026-09-27

## Основание

После неудачных P2P/DEV diagnostics проверены исходники AlexGyver:

- библиотека GyverLibs/Clap;
- совместимая старая VolAnalyzer logic;
- реальная интеграция GyverLamp2.

В GyverLamp2:

- Clap получает `vol.getRawMax()`;
- порог производной = 250;
- timeout последовательности = 500 ms;
- состояние лампы меняется только при `hasClaps(2)`.

ARDU test повторяет этот принцип без управления светом.

## 1. Verify + Upload

Файл:

`04_Прошивка/nano_clap_gyver_r1/nano_clap_gyver_r1.ino`

Arduino Nano / ATmega328P (Old Bootloader).

Serial Monitor: 115200 / Newline.

Startup:

```
ARDU NANO CLAP GYVER R1 READY
GYVERLAMP2 STYLE: VOL RAWMAX DERIVATIVE + DOUBLE CLAP
STATUS FW=CLAP_GYVER REV=1 ... CLAP_TRSH=250 CLAP_TIMEOUT_MS=500 TARGET=DOUBLE ...
```

## 2. ADC gate

```
ADC
STATUS
```

Норма:
- DC_OK=YES;
- ADC без saturation.

## 3. Одиночный хлопок

```
CLEAR
```

Не нажимать мышь рядом с микрофоном после этого; сделать 5 одиночных хлопков с паузой больше 1 s.

Для распознанного физического хлопка ожидается:

```
EVENT CLAP ...
```

После timeout:

```
EVENT CLAP_SEQUENCE COUNT=1 ...
```

Но `EVENT DOUBLE_CLAP` появляться не должен.

После:

```
STATS
```

## 4. Двойной хлопок

```
CLEAR
```

Сделать 5 естественных пар «хлоп-хлоп», интервал примерно 150..450 ms.

Для успешной пары ожидается:

```
EVENT CLAP ...
EVENT CLAP ...
EVENT CLAP_SEQUENCE COUNT=2 ...
EVENT DOUBLE_CLAP
```

После пяти пар:

```
STATS
```

Главный критерий: DOUBLE_EVENTS близко к 5.

## 5. Mouse click

```
CLEAR
```

Сделать 10 одиночных mouse clicks, не хлопать.

Щелчок может быть распознан как единичный CLAP — это допустимо.

Критический критерий:

- одиночный click не должен давать `DOUBLE_CLAP`;
- `DOUBLE_EVENTS=0`, если специально не делать два клика в нужном временном окне.

## 6. Речь

```
CLEAR
```

30 s обычной речи, затем:

```
STATS
```

Главное — отсутствие DOUBLE_CLAP. Одиночные false clap candidates допустимы только если не собираются в pair.

## 7. Музыка

```
CLEAR
```

30 s обычной музыки, затем:

```
STATS
```

Главное — отсутствие/минимум DOUBLE_CLAP.

Если музыка стабильно создаёт двойные события, такой detector нельзя сразу объединять с L01 во время Music mode.

## 8. Threshold tuning только если хлопки не ловятся

Сначала тестировать точные параметры GyverLamp2:

```
TRSH 250
TIMEOUT 500
```

Если обычные хлопки вообще не дают EVENT CLAP, снизить только derivative threshold:

```
TRSH 200
```

затем, если нужно:

```
TRSH 150
```

Timeout оставить 500 до отдельного решения.

Не снижать threshold ниже необходимого.

## 9. TRACE при проблеме

Только если хлопок всё ещё не распознаётся:

```
TRACE ON
```

Сделать один хлопок, затем:

```
TRACE OFF
```

Прислать TRACE вокруг хлопка.

## PASS

Для перехода к реальному L01 clap switch нужно:

1. одиночные хлопки распознаются;
2. 5 double-clap pairs дают стабильный DOUBLE_CLAP;
3. одиночные clicks не дают DOUBLE_CLAP;
4. речь не даёт стабильных DOUBLE_CLAP;
5. музыка оценивается отдельно;
6. определить минимальный рабочий TRSH.

После PASS final L01 control использует double-clap, как GyverLamp2.
