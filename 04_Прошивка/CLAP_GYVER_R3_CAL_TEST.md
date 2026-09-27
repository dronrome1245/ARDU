# ARDU — CLAP GYVER R3 family calibration test

Дата: 2026-09-27

## Цель

Проверить калибровку хлопкового выключателя не под одного взрослого, а под нескольких пользователей разного возраста/силы хлопка.

R3 сохраняет Gyver-style double-clap detector, но добавляет family calibration.

## Рекомендуемая домашняя калибровка

Для трёх детей: по 3 double-clap samples от каждого = 9 samples.

Итого:

```
CLAPCAL START 9
```

Nano сначала 3 секунды измеряет фон и печатает:

```
EVENT CLAPCAL QUIET_DONE P99_DER=... TARGET_PAIRS=9
EVENT CLAPCAL WAIT_SAMPLE
```

## Каждый sample

На каждую пару:

```
CLAPCAL SAMPLE
```

После:

```
EVENT CLAPCAL SAMPLE_GO
```

пользователь делает один обычный `хлоп-хлоп`.

Успех:

```
EVENT CLAPCAL SAMPLE_OK PAIR=1/9 WEAK=... STRONG=... RAW_CANDIDATES=...
```

Если пара распознана недостаточно уверенно:

```
EVENT CLAPCAL SAMPLE_FAIL CLAPS=...
```

Такой sample не засчитывается; повторить команду.

Для трёх пользователей сделать по 3 успешных sample каждым.

## Расчёт порога

После 9 успешных пар:

```
CLAPCAL FINISH
```

Ожидается:

```
EVENT CLAPCAL RESULT QUIET_P99=... CLAP_P20=... THRESH=... CLAPS=18 PAIRS=9
```

Nano сразу применяет suggested threshold в RAM, но ещё не сохраняет.

## Проверка до сохранения

Проверить каждым пользователем 5 double-clap pairs.

Затем:
- 30 s тишины;
- 30 s речи;
- 30 s музыки.

Критерий:
- пользовательские double clap должны срабатывать стабильно;
- false DOUBLE_CLAP должны отсутствовать либо быть неприемлемо редкими для реального использования.

## Сохранение

Если результат устраивает:

```
CLAPCAL SAVE
STATUS
```

Ожидается:
- `OK CLAPCAL_SAVED`;
- `SAVED=YES DIRTY=NO`;
- threshold переживает Reset/power cycle.

Если не устраивает:

```
CLAPCAL CANCEL
```

возвращает предыдущий сохранённый threshold.

## App mapping

Будущее приложение должно провести пользователя по тем же фазам:
1. тишина;
2. «Пользователь 1: хлопните дважды» ×3;
3. пользователь 2 ×3;
4. пользователь 3 ×3;
5. показать suggested sensitivity;
6. тест;
7. «Сохранить калибровку».

Количество пользователей приложению не принципиально: Nano видит только 3..12 sample pairs.
