# ARDU — L01 R2 Kelvin / RGB / startup persistence test

Дата: 2026-09-27

## Цель

L01 R2 расширяет уже пройденный L01 R1:

- приблизительная цветовая температура 1800..6500 K;
- пресеты 2700 K / 4000 K / 6000 K;
- произвольный RGB-цвет;
- яркость 0..255;
- отдельный SAVE в EEPROM;
- после reset/power cycle L01 включается с последним сохранённым цветом и яркостью.

Важно: WS2812B — RGB, а не настоящая CCT-лента. Kelvin здесь означает приблизительную визуальную имитацию температуры белого, а не измеренную колориметрическую CCT.

## 1. Verify + Upload

Файл:

04_Прошивка/nano_l01_r2/nano_l01_r2.ino

Arduino Nano / ATmega328P (Old Bootloader), Serial 115200/Newline.

Startup:

ARDU NANO L01 R2 READY
KELVIN+RGB+EEPROM STARTUP PROFILE; CLAP NOT IN R2

## 2. Presets

Отправить:

PRESET 2700
STATUS

Визуально: тёплый белый.

Затем:

PRESET 4000
STATUS

Визуально: более нейтральный белый.

Затем:

PRESET 6000
STATUS

Визуально: холодный белый.

STATUS должен показывать COLOR_MODE=KELVIN и соответствующий KELVIN.

## 3. Произвольный Kelvin

Проверить:

KELVIN 2200
KELVIN 3500
KELVIN 5000
KELVIN 6500

Оттенок должен последовательно идти от тёплого к холодному.

Проверить защиту:

KELVIN 1700
KELVIN 6600

Ожидается ERR BAD_KELVIN RANGE=1800..6500.

## 4. Произвольный цвет

Проверить:

RGB 255 0 0
RGB 0 255 0
RGB 0 0 255
RGB 255 80 0
RGB 255 0 180

Каждая команда должна сразу менять весь L01.

STATUS должен показывать COLOR_MODE=RGB и фактический RGB.

## 5. Яркость

На любом цвете:

BRIGHT 16
BRIGHT 64
BRIGHT 128
BRIGHT 255

Яркость должна меняться, current limit = 500 mA остаётся активен.

## 6. SAVE и reset — Kelvin

Задать:

PRESET 2700
BRIGHT 48
SAVE
STATUS

Ожидается:

SAVED=YES
DIRTY=NO
COLOR_MODE=KELVIN
KELVIN=2700
BRIGHTNESS=48

Нажать Reset Nano.

После startup L01 должен автоматически включиться тем же тёплым 2700 K и яркостью 48.

STATUS должен снова показать KELVIN=2700 BRIGHTNESS=48 SAVED=YES.

## 7. SAVE и reset — произвольный цвет

Задать:

RGB 255 0 0
BRIGHT 32
SAVE

Нажать Reset.

После startup кольцо должно автоматически включиться красным с BRIGHTNESS=32.

## 8. Dirty / LOAD

После сохранённого красного:

RGB 0 0 255
BRIGHT 80
STATUS

Ожидается DIRTY=YES.

Отправить:

LOAD

Должен вернуться сохранённый красный и BRIGHTNESS=32 без reset.

## 9. Финальный профиль для дальнейшей работы

Задать разумный обычный свет, например:

PRESET 4000
BRIGHT 64
SAVE
STATUS

После Reset должны восстановиться 4000 K / 64.

## PASS L01 R2

R2 считается пройденным если:

1. 2700/4000/6000 визуально различаются по теплоте;
2. произвольный Kelvin 1800..6500 меняет белый от тёплого к холодному;
3. RGB позволяет любой цвет;
4. яркость регулируется;
5. SAVE сохраняет Kelvin/RGB mode, цвет и brightness;
6. reset восстанавливает сохранённый startup profile;
7. LOAD откатывает несохранённые изменения;
8. UART остаётся стабильным.

После PASS L01 R2 следующий слой — clap diagnostic MAX9814.
