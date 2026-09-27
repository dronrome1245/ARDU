# ARDU — CLAP DIAG R3 delayed-arm test

Дата: 2026-09-27

## Исправление

R2 показал, что щелчок мыши при отправке команды может попасть в calibration window.

R3:

- CALCLAP ждёт 1500 ms перед измерением;
- CLEAR ждёт 1500 ms перед ARM;
- ARM ON ждёт 1500 ms;
- threshold считается по quiet average, absolute max печатается только как INFO;
- candidate требует одновременно P2P и DEV;
- lockout = 250 ms.

## Тест

После Upload:

```
ADC
CALCLAP
```

После CALCLAP дождаться:

```
EVENT ARMED
```

Ожидается threshold существенно ниже прежних 732/408.

### 1. Пять одиночных хлопков

```
CLEAR
```

Дождаться `EVENT ARMED`.

Сделать 5 одиночных хлопков с паузой около 2 s.

После:

```
STATS
```

Нужны все EVENT IMPULSE строки и STATS.

### 2. Тишина

```
CLEAR
```

Дождаться EVENT ARMED и 30 s ничего не делать.

```
STATS
```

### 3. Речь

CLEAR → дождаться EVENT ARMED → 30 s обычной речи → STATS.

### 4. Музыка

CLEAR → дождаться EVENT ARMED → 30 s музыки → STATS.

### 5. Двойные хлопки

CLEAR → дождаться EVENT ARMED.

Сделать 5 естественных пар хлоп-хлоп.

Сохранить EVENT lines с GAP_MS и STATS.

## Что прислать

- CALCLAP;
- EVENT/STATS для 5 одиночных хлопков;
- STATS тишины;
- STATS речи;
- STATS музыки;
- EVENT lines двойных хлопков.

После этого выбирается final gesture и временное окно.
