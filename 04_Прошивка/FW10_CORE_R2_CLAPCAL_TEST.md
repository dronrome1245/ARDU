# ARDU — FW10 CORE R2 CLAPCAL integration test

Дата: 2026-09-30

## Цель

Интегрировать уже подготовленную family clap calibration в общую Nano-прошивку без изменения текущей аппаратной архитектуры и EEPROM layout.

R2 наследует FW10 CORE R1:
- 44 LED, D6;
- L01 profile/persistence;
- double clap runtime;
- DS3231;
- Night;
- Alarm/Dawn;
- reset/power policy.

Новое:
- `CLAPCAL START <3..12>`;
- `CLAPCAL SAMPLE`;
- `CLAPCAL FINISH`;
- `CLAPCAL SAVE`;
- `CLAPCAL STATUS`;
- `CLAPCAL CANCEL`.

## Gate 0 — только Verify

До успешной компиляции R2 на Nano НЕ загружать.

Arduino IDE:
- Board: Arduino Nano;
- Processor: ATmega328P (Old Bootloader);
- Verify.

Прислать полный memory report.

PASS:
- compile без ошибок;
- flash <= 30720 bytes;
- SRAM globals <= 2048 bytes.

Практический запас также оценить отдельно. CORE R1 baseline:
- flash 26014 / 30720 = 84%;
- SRAM globals 1316 / 2048 = 64%.

Если flash не помещается либо запас SRAM становится опасно мал — Upload запрещён до оптимизации.

## Gate 1 — startup regression после будущего Upload

Только после отдельного compile-budget PASS:
- startup содержит `ARDU NANO FW10 CORE R2 READY`;
- STATUS: `FW=FW10_CORE REV=2`;
- `CLAP STATUS` содержит `CALIBRATION=READY`;
- `PING→PONG`;
- L01/Night/RTC smoke без полного повторения старого regression.

## Gate 2 — calibration batch

Рекомендуемый домашний тест: 9 успешных double-clap pairs.

1. `CLAPCAL START 9`;
2. после `WAIT_SAMPLE` девять раз выполнять `CLAPCAL SAMPLE` и делать одну пару хлопков;
3. `CLAPCAL FINISH`;
4. проверить suggested threshold без сохранения;
5. проверить double clap runtime;
6. `CLAPCAL SAVE`;
7. Reset/power-cycle;
8. `CLAP STATUS` — новый threshold сохранён;
9. `CLAPCAL CANCEL` используется только до SAVE для отката к предыдущей EEPROM calibration.

## Важно

R2 сохраняет существующий EEPROM block `CLAP_BASE=192` и формат threshold/timeout. Нового EEPROM layout нет.

ESP/Android calibration wizard готовить только после Gate 0 compile-budget PASS.
