# ARDU — AlexGyver clap reference

Дата: 2026-09-27

## Проверенные исходники

### GyverLibs/Clap

Репозиторий: `GyverLibs/Clap`.

Clap не сравнивает абсолютную громкость с одним порогом. Каждые 10 ms он считает derivative входного уровня:

`der = val - prevVal`

Далее ищет форму:

1. положительный резкий front;
2. отрицательный резкий front не позже 200 ms;
3. возврат derivative к нулевой зоне не позже 200 ms;
4. это считается одним clap;
5. несколько clap собираются в sequence до timeout.

Default library parameters:
- derivative threshold 150;
- sequence timeout 700 ms.

README рекомендует передавать `VolAnalyzer.getRawMax()`.

### GyverLamp2

В `firmware/GyverLamp2/analog.ino`:

- `clap.setTimeout(500)`;
- `clap.setTrsh(250)`;
- `vol.setDt(700)`;
- `vol.setPeriod(5)`;
- при MAX_LEDS=300 `vol.setWindow(...)=20`;
- `clap.tick(vol.getRawMax())`;
- управление лампой выполняется по `clap.hasClaps(2)`.

То есть GyverLamp2 использует **двойной хлопок**.

Вендоренный `VolAnalyzer.h` GyverLamp2 формирует `rawMax` как максимум ADC-window внутри примерно 150 ms amplitude frame. Из-за периодического сброса frame резкий звук создаёт положительный и затем отрицательный derivative, который и распознаёт Clap state machine.

## Решение для ARDU diagnostic

Не использовать текущую GyverLibs/Clap как внешнюю зависимость напрямую, потому что её README предупреждает о несовместимости с актуальной VolAnalyzer.

Для hardware test перенесён минимальный согласованный алгоритм из пары версий, фактически используемых в GyverLamp2:

`nano_clap_gyver_r1`

Первый test использует параметры GyverLamp2:
- derivative threshold 250;
- sequence timeout 500 ms;
- double clap target.

После hardware result параметры могут быть скорректированы отдельной ревизией.
