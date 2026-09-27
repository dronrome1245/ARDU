# ARDU — FW-10 CORE R1 integration test

Дата: 2026-09-27

## Что объединено

FW-10 CORE R1 впервые объединяет в одном Nano sketch:

- 44 LED на D6;
- L01 Kelvin/RGB/startup profile;
- double-clap control;
- DS3231;
- Alarm + Dawn recovery;
- Night manual/schedule;
- reset/power-on state policy.

Пока намеренно НЕ входят Ambient/Music и физический D7 ring.

Файл:

`04_Прошивка/nano_fw10_core_r1/nano_fw10_core_r1.ino`

Arduino Nano / ATmega328P (Old Bootloader), Serial Monitor 115200/Newline.

---

## 1. Verify + Upload

После Upload ожидается:

```
ARDU NANO FW10 CORE R1 READY
44 LED / ONE-RING SAFE LIMIT / L01+CLAP+RTC+ALARM+DAWN+NIGHT
...
STATUS FW=FW10_CORE REV=1 ...
```

На первом cold boot ожидается:

```
EVENT COLD_POWER_ON MODE=L01
```

STATUS должен содержать:

- `LEDS=44`;
- `RING_A=D6`;
- `RING_B=DISCONNECTED_TEST`;
- `LIMIT_MA=500`;
- `MODE=L01`;
- `RTC_VALID=YES`;
- `CLAP_TRSH=70` если сохранённой clap calibration ещё нет.

Если Verify не проходит — дальше тест не выполнять, прислать полный compiler error.

---

## 2. L01 regression + EEPROM compatibility

```
LIGHT STATUS
```

Ожидается сохранённый L01 profile из R3, если он уже был сохранён.

Проверить:

```
LIGHT PRESET 2700
LIGHT BRIGHT 64
LIGHT SAVE
LIGHT STATUS
```

Норма:

- 44 LED;
- тёплый принятый 2700 K;
- сохранение работает.

Затем:

```
LIGHT RGB 255 0 0
LIGHT BRIGHT 32
LIGHT STATUS
LIGHT LOAD
LIGHT STATUS
```

После LOAD должен вернуться сохранённый 2700 K / 64.

---

## 3. Clap default

```
CLAP STATUS
```

До будущей app calibration ожидается:

```
GESTURE=DOUBLE
TRSH=70
TIMEOUT_MS=500
```

Если EEPROM содержит уже валидную сохранённую calibration, STATUS может показать её вместо defaults.

Убедиться:

```
LIGHT CLAP ON
MODE L01
```

Сделать `хлоп-хлоп`.

Норма:

```
EVENT CLAP_TOGGLE LIGHT=OFF
```

Ещё один `хлоп-хлоп`:

```
EVENT CLAP_TOGGLE LIGHT=ON
```

Одиночный хлопок свет переключать не должен.

---

## 4. Service Reset не равен power-on

Сначала:

```
MODE OFF
STATUS
```

Нажать физический Reset Nano.

Ожидается:

```
EVENT WARM_RESET RESTORE_MODE=OFF
STATUS ... MODE=OFF RESET=EXTERNAL...
```

Главное: Reset не должен сам включить L01.

Если `RESET=WARM_UNKNOWN`, но MODE=OFF восстановлен правильно, сохранить строку STATUS: это означает, что old bootloader очистил MCUSR, но RAM-cookie различил warm reset.

---

## 5. Настоящий power cycle

При MODE=OFF полностью снять питание ARDU второй клавишей на несколько секунд, затем подать снова.

Ожидается:

```
EVENT COLD_POWER_ON MODE=L01
STATUS ... MODE=L01 RESET=POWER...
```

Должен включиться сохранённый L01 profile.

Это главный тест D-058.

---

## 6. RTC

```
TIME
STATUS
```

Норма:

- текущее время идёт;
- RTC_VALID=YES.

---

## 7. Night manual regression

```
NIGHT ON
NIGHT HUE 24
NIGHT SAT 180
NIGHT BRIGHT 18
NIGHT STATUS
```

Визуально — прежний night light.

Проверить:

```
NIGHT HUE 96
NIGHT SAT 80
NIGHT BRIGHT 40
NIGHT STATUS
```

Вернуть:

```
NIGHT HUE 24
NIGHT SAT 180
NIGHT BRIGHT 18
```

---

## 8. Night schedule basic integration

Посмотреть:

```
TIME
```

Задать тестовое окно на ближайшие минуты так же, как в NIGHT R2.

Пример только по форме:

```
NIGHT SCHEDULESET 17:01 17:03
NIGHT SCHEDULE ON
NIGHT STATUS
```

Должны сохраниться прежние semantics:
- interval [ON, OFF);
- schedule transition event;
- manual NIGHT ON/OFF выключает schedule.

После теста вернуть желаемый интервал, например:

```
NIGHT SCHEDULESET 22:00 07:00
NIGHT SCHEDULE OFF
```

---

## 9. Dawn manual integration

```
DAWN STATUS
DAWN TEST 20
```

Должен начаться прежний рассвет на 20 s:

```
EVENT DAWN_START SOURCE=TEST DURATION_S=20
```

и завершиться:

```
EVENT DAWN_COMPLETE
```

После:

```
DAWN STOP
```

Режим должен стать OFF.

---

## 10. Dawn warm-reset recovery

```
DAWN TEST 30
```

Примерно через 8–12 s нажать Reset Nano.

Ожидается:

```
EVENT DAWN_RECOVER PHASE=RUNNING SOURCE=TEST ...
```

Рассвет не начинается с нуля, а продолжается по DS3231.

---

## 11. Cold power interruption DURING active dawn

Снова:

```
DAWN TEST 30
```

Через 8–12 s полностью снять питание ARDU на несколько секунд и вернуть.

Пока сохранённый dawn runtime ещё находится внутри своих 30 s, приоритет имеет recovery:

```
EVENT DAWN_RECOVER PHASE=RUNNING ...
```

То есть активный незавершённый рассвет сильнее правила cold-power L01.

---

## 12. Cold power-on после DAWN HOLD

Дать DAWN TEST завершиться до HOLD.

Теперь полностью снять питание и снова подать.

Ожидается НЕ восстановление старого HOLD, а:

```
EVENT COLD_POWER_ON MODE=L01
```

Это новое правило интеграции: stale HOLD не вытесняет обычный свет после настенной клавиши.

---

## 13. Alarm integration

Поставить короткий FADE для теста:

```
DAWN FADEMIN 1
ALARM CLEARLAST
```

Посмотреть TIME и поставить ALARM на следующую минуту:

```
ALARM SET HH:MM
ALARM ON
```

В момент alarm:

```
EVENT ALARM_TRIGGER RTC=...
EVENT DAWN_START SOURCE=ALARM DURATION_S=60
```

Повторного trigger в ту же дату быть не должно.

После:

```
DAWN STOP
DAWN FADEMIN 30
```

---

## 14. UART/FastLED regression

При L01 ON быстро отправить:

```
STATUS
LIGHT STATUS
CLAP STATUS
TIME
NIGHT STATUS
ALARM STATUS
DAWN STATUS
PING
STATUS
```

Критерии:

- PING→PONG;
- нет ERR UNKNOWN_COMMAND;
- строки не повреждаются;
- UPTIME растёт;
- свет не получает случайных вспышек.

---

## PASS CORE R1

FW-10 CORE R1 считается пройденным, если:

1. compile/upload проходит;
2. L01 profile совместим с EEPROM R3;
3. double clap переключает L01 при defaults/calibrated threshold;
4. service Reset восстанавливает OFF и не имитирует wall switch;
5. cold power-on включает L01;
6. RTC работает;
7. Night manual/schedule работает;
8. Dawn renderer/recovery работает;
9. active dawn переживает power interruption;
10. stale Dawn HOLD не побеждает cold-power L01;
11. Alarm запускает Dawn;
12. UART остаётся стабильным.

После PASS:
- FW-10 CORE R2: подключение второго D7 ring + двухкольцевой current-limit/power test;
- затем по одному добавлять Ambient и Music с адаптацией геометрии на 44 LED.
