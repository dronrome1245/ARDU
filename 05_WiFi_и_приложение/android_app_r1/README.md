# ARDU Android app R1

Первый native Android/Kotlin слой для проверки уже закрытого транспорта ARDU:

- `GET /api/ping`;
- `GET /api/status` — показывает фактический raw `STATUS` Nano;
- `GET /api/time` — показывает фактическое время DS3231;
- Developer Mode: raw `POST /api/dev/nano` и полный ответ Nano.

Nano остаётся источником истины. R1 не пытается превращать текущий raw transport bridge в окончательный semantic API.

## Подключение

R1 сначала пробует локальное имя `ardu.local`, затем текущий проверенный адрес стенда как fallback. Адрес не показывается в обычном пользовательском интерфейсе. Это временный transport-layer механизм; позже его заменит нормальное обнаружение/настройка устройства.

Обычный экран показывает только:

- `Онлайн / Нет связи`;
- версию ESP bridge;
- `STATUS` Nano;
- время DS3231.

Техническая raw-команда скрыта в Developer Mode.

## Открытие

1. В Android Studio выбрать **Open**.
2. Открыть каталог `05_WiFi_и_приложение/android_app_r1`.
3. Дождаться Gradle Sync. Проект использует AGP 9.4.1, Gradle 9.6, JDK 17 и `compileSdk 36`.
4. Запустить на Android-телефоне, подключённом к той же домашней Wi-Fi сети, что и ARDU.

Подробный hardware/app smoke-test: `../ANDROID_APP_R1_TEST.md`.

## Следующий слой

После физического app transport PASS:

1. добавить обычный свет L01;
2. выполнить сквозной regression L01 через приложение;
3. затем продолжать по D-071 малыми интеграционными слоями.

`/api/events` намеренно оставлен на следующую малую итерацию Developer Mode.
