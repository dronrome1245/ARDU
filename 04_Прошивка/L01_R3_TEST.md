# ARDU — L01 R3 2700K warm mapping + 44 LED test

Дата: 2026-09-27

## Изменения относительно R2

- физическое кольцо теперь содержит 44 WS2812B;
- L01 использует LED_COUNT=44;
- preset 2700 K теперь использует тот же тёплый RGB, который в R2 использовался для 2200 K: RGB=255,170,87;
- остальная логика Kelvin/RGB/EEPROM startup profile сохраняется.

## 1. Verify + Upload

Файл:

04_Прошивка/nano_l01_r3/nano_l01_r3.ino

Arduino Nano / ATmega328P (Old Bootloader), Serial 115200/Newline.

Startup должен содержать:

ARDU NANO L01 R3 READY

STATUS должен показывать:

FW=L01 REV=3
LEDS=44

Все 44 LED должны участвовать в заливке; отдельного тёмного/застывшего 44-го пикселя быть не должно.

## 2. Проверка нового 2700 K

PRESET 2700
STATUS

Ожидается:

COLOR_MODE=KELVIN
KELVIN=2700
RGB=255,170,87

Визуально 2700 K должен соответствовать тому тёплому оттенку, который в R2 давала команда KELVIN 2200.

## 3. Сохранение остальных пресетов

Проверить:

PRESET 4000
PRESET 6000

Они должны оставаться нейтральнее/холоднее соответственно.

## 4. RGB regression

Проверить:

RGB 255 0 0
RGB 0 255 0
RGB 0 0 255

Все 44 LED должны менять цвет одинаково.

## 5. Startup persistence

Задать:

PRESET 2700
BRIGHT 64
SAVE
STATUS

Ожидается SAVED=YES, DIRTY=NO.

Выполнить Reset Nano.

После startup должны восстановиться:

POWER=ON
COLOR_MODE=KELVIN
KELVIN=2700
BRIGHTNESS=64
RGB=255,170,87
LEDS=44

## 6. Full power cycle

Полностью снять питание ARDU второй клавишей и снова подать его.

L01 должен автоматически включиться с сохранённым профилем 2700 K / brightness 64 на всех 44 LED.

## PASS

R3 считается пройденным, если:

1. FW=L01 REV=3;
2. STATUS показывает LEDS=44;
3. физически работают все 44 LED;
4. новый 2700 K визуально соответствует прежнему R2 2200 K;
5. RGB regression проходит;
6. SAVE/Reset restore проходит;
7. полный power cycle восстанавливает тот же 2700 K profile.

После PASS следующий слой — clap diagnostic MAX9814.
