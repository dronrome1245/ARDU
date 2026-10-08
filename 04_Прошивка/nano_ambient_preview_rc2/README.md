# ARDU Nano Ambient Preview RC2 — один файл Arduino IDE

**Исправление RC1:** в RC1 зависимость от `ambient_preset_probe.h` на компьютере владельца вызвала `No such file or directory`, хотя файл находился рядом. Точную причину поведения IDE не удалось подтвердить. **RC2 не имеет локальных include-зависимостей**: алгоритмы P01/P04/P05 встроены прямо в полную `.ino`.

## Как открыть

1. В `C:\ARDU` сделать `git pull`, открыть в Arduino IDE `04_Прошивка/nano_ambient_preview_rc2/nano_ambient_preview_rc2.ino`.
2. Если скачиваешь из GitHub в «Загрузки», достаточно **одного этого .ino** в папке `nano_ambient_preview_rc2`. Файл `ambient_preset_probe.h` для RC2 **не нужен**.
3. Выбрать `Arduino Nano`, `ATmega328P (Old Bootloader)` и сначала **Verify**. Перед физическим Upload прочитать [инструкцию RC2](../AMBIENT_PRESETS_PREVIEW_RC2_TEST.md): отключение ESP TX→Nano D0 при снятом питании, проверка USB/5 В, тест и откат.

## Что меняется

RC2, как и RC1, содержит **всю** прошивку Nano v1. На вкладке «Фон» выбор F01 «Цвет» запускает цикл P01 Северное сияние → P04 Космос → P05 Камин → обычный F01, по ~8,2 с. Обычный `nano_ardu_v1.ino` и ESP/Android не изменены; новых UART/API/EEPROM ID нет.

GitHub Actions сверяет RC2 с полной Nano v1 + кодом трёх эффектов и отдельно **копирует только один RC2 `.ino` в пустую папку** для проверки сборки — именно чтобы ошибка отсутствующего локального заголовка не повторилась.
