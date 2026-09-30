# ARDU — FW10 CORE R3 compact CLAPCAL compile gate

Дата: 2026-09-30

## Причина R3

CORE R2 compile:
- flash 29788 / 30720 = 96%;
- globals 1445 / 2048 = 70%.

R2 не загружался на Nano. По revision policy оптимизация выполняется новой ревизией R3.

## Что оптимизировано без изменения D-068

- один ClapDetector переиспользуется runtime/calibration;
- хранится один weak strength на успешную double-clap pair;
- два strongest candidates sample считаются на лету;
- quiet P99 использует compact 32-bin histogram и 20 ms sampling;
- UART calibration responses сокращены;
- HELP сокращён;
- EEPROM layout и threshold/timeout format не менялись.

Команды пользователя остаются:
- `CLAPCAL START <3..12>`
- `SAMPLE`
- `FINISH`
- `SAVE`
- `STATUS`
- `CANCEL`

Compact responses:
- `CAL Q <quietP99> <targetPairs>`
- `CAL S <good>/<target> <weak> <strong>`
- `CAL F <candidateCount>`
- `CAL R <quietP99> <weakP20> <threshold>`
- `CAL ST ...`

## Gate — Verify only

Arduino Nano / ATmega328P (Old Bootloader) → Verify.

До анализа нового memory report Upload запрещён.

Прислать:
- Sketch uses ...
- Global variables use ...

Цель R3 — существенно улучшить запас относительно R2; итоговое решение об Upload принимается после фактического compiler report.
