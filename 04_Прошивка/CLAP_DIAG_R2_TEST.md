# ARDU — CLAP DIAG R2 raw capture test

Дата: 2026-09-27

## Почему R2

R1 реагировал на короткие щелчки мыши, но не ловил хлопки при default THRESH=180. В присланном логе:

- startup DC=255, DC_OK=YES;
- обычный ADC: AVG=264 MIN=246 MAX=281 P2P=35;
- щелчки давали события с P2P примерно 181..417;
- R1 CALCLAP не был выполнен: CAL=NO;
- R1 trigger использовал только P2P.

R2 не пытается сразу «угадывать» хлопок. Сначала команда CAPTURE измеряет событие вообще без порога.

## 1. Verify + Upload

Файл:

`04_Прошивка/nano_clap_diag_r2/nano_clap_diag_r2.ino`

Arduino Nano / ATmega328P (Old Bootloader), Serial 115200/Newline.

Startup:

```
ARDU NANO CLAP DIAG R2 READY
USE CAPTURE TO MEASURE SILENCE/CLICK/CLAP WITHOUT THRESHOLD
```

## 2. ADC + quiet calibration

```
ADC
CALCLAP
STATUS
```

Записать строки полностью.

R2 выдаёт два threshold:

- THRESH_P2P;
- THRESH_DEV.

## 3. CAPTURE тишины

Отправить:

```
CAPTURE
```

После строки `CAPTURE_GO` 3 секунды ничего не делать и не говорить.

Ожидается:

```
CAPTURE RESULT MAX_P2P=... MAX_DEV=... MIN=... MAX=... MAX_AVG_SHIFT=... BLOCKS=...
```

Сохранить строку как SILENCE.

## 4. CAPTURE щелчка мыши

Снова:

```
CAPTURE
```

После `CAPTURE_GO` один раз щёлкнуть мышью рядом с обычным местом использования.

Сохранить CAPTURE RESULT как CLICK.

## 5. CAPTURE хлопка

Снова:

```
CAPTURE
```

После `CAPTURE_GO` сделать один обычный хлопок ладонями.

Сохранить CAPTURE RESULT как CLAP.

Повторить этот тест хлопка 3 раза, чтобы не делать вывод по одному измерению.

## 6. CAPTURE речи

```
CAPTURE
```

После `CAPTURE_GO` 3 секунды говорить обычным голосом.

Сохранить CAPTURE RESULT как SPEECH.

## 7. CAPTURE музыки

```
CAPTURE
```

После `CAPTURE_GO` проиграть обычную музыку на типичной громкости.

Сохранить CAPTURE RESULT как MUSIC.

## 8. Что сравниваем

Нужны прежде всего:

- MAX_P2P;
- MAX_DEV;
- MAX_AVG_SHIFT.

Если CLAP отличается от CLICK/SPEECH/MUSIC по MAX_DEV, final detector можно строить по абсолютному отклонению от DC.

Если CLAP неотличим от CLICK по амплитуде, для final switch использовать не «один сильный импульс», а временной паттерн (например, double clap), чтобы одиночный mouse click/door click не переключал свет.

## 9. Event detector R2

После CAPTURE можно проверить автоматические кандидаты.

```
CLEAR
ARM ON
```

Сделать несколько хлопков.

R2 вызывает EVENT, если превышен THRESH_P2P ИЛИ THRESH_DEV.

При необходимости вручную:

```
THRESH 120 90
```

Формат:

```
THRESH <P2P> <DEV>
```

Не выбирать final threshold до сравнения CAPTURE SILENCE/CLICK/CLAP/SPEECH/MUSIC.

## Что прислать

1. ADC;
2. CALCLAP;
3. CAPTURE SILENCE;
4. CAPTURE CLICK;
5. три CAPTURE CLAP;
6. CAPTURE SPEECH;
7. CAPTURE MUSIC.

По этим значениям выбирается R3/final clap algorithm.
