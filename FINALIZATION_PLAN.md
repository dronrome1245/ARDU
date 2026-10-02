# ARDU — план финализации v1

Дата: 2026-10-01  
Статус: КАНОНИЧЕСКИЙ ПЛАН ЗАВЕРШЕНИЯ  
Основание: решение владельца 2026-10-01 прекратить мелкие firmware-итерации и перейти к одной финальной сборке прошивок, затем завершить Android-приложение, дизайн и физический запуск.

## 1. Цель

Закончить ARDU как воспроизводимо работающую систему:

`Android → домашний Wi-Fi → ESP8266 → UART → Arduino Nano → MAX9814 / DS3231 / 2×44 WS2812B`.

После финализации устройство должно работать автономно без телефона для уже заданного режима, Night schedule и Alarm/Dawn, а телефон используется для настройки и управления.

## 2. Release policy

С 2026-10-01 прежняя стратегия «маленькая новая firmware-функция → отдельный upload → отдельный hardware test» для уже проверенных подсистем прекращается.

До финальной физической загрузки разрешены:
- анализ и рефакторинг кода;
- объединение проверенных модулей;
- compile/size/static проверки;
- API/schema review;
- Android development против зафиксированного API;
- подготовка единого acceptance checklist.

На Nano физически загружается только полная release-кандидат прошивка v1.

На ESP8266 после неё загружается только полная release-кандидат прошивка v1; штатный путь загрузки ESP — OTA, wired programmer остаётся recovery-only.

После firmware acceptance прошивки считаются замороженными. Новая прошивка допускается только при блокирующем дефекте, который нельзя исправить на уровне приложения/настроек.

## 3. Зафиксированный функциональный scope v1

### 3.1 Nano

Обязательно входят:
- L01 ordinary light;
- approximate Kelvin 1800..6500 K;
- presets 2700 / 4000 / 6000 K;
- arbitrary RGB;
- brightness;
- cold power-on → сохранённый L01 startup profile;
- local double-clap switch;
- app enable/disable clap;
- family CLAPCAL wizard backend: START/SAMPLE/FINISH/SAVE/CANCEL;
- MAX9814/A0 shared audio frontend;
- Music: M01, M02, M03, M04, M05, M08, M09;
- Ambient: F01, F02, F03 + NEXT/PREV/AUTO/PERIOD;
- Night: HSV/brightness + schedule;
- RTC DS3231;
- Alarm;
- Dawn renderer;
- Dawn STOP;
- Dawn reset/power recovery;
- persistence всех пользовательских настроек;
- D6 ring A + D7 ring B;
- 44 LED на кольцо;
- одинаковый кадр на оба кольца;
- общий программный current limit;
- UART v1;
- диагностический STATUS/PING и минимально необходимый Developer interface.

Не входят:
- M06 strobe;
- M07 как отдельный Music ID;
- SoftAP;
- Bluetooth;
- cloud;
- отдельная логика/эффекты второго кольца.

### 3.2 ESP8266

Обязательно входят:
- Station-only Wi-Fi;
- local HTTP API;
- password-protected OTA;
- UART bridge к Nano;
- semantic API всех пользовательских разделов;
- чтение полного состояния и сохранённых настроек из Nano;
- Developer endpoint отдельно от пользовательского API;
- polling/event endpoint для async Nano EVENT;
- reconnect без потери автономной работы Nano.

### 3.3 Android

Обязательно входят пять пользовательских разделов:
1. Обычный свет.
2. Светомузыка.
3. Фон.
4. Ночник.
5. Будильник/рассвет.

Отдельно:
- connection/state layer;
- Settings;
- Developer Mode;
- family clap calibration wizard.

## 4. Этап A — финальная Nano-прошивка v1

### A1. Не сливать старые скетчи механически

Финальная прошивка собирается из общих подсистем, а не копированием всех standalone sketch целиком.

Требуемая внутренняя структура:
- один framebuffer на 44 LED;
- два FastLED output controller: D6 и D7 показывают один и тот же framebuffer;
- один audio sampling/FHT pipeline для всех Music режимов;
- один clap envelope/detector, переиспользуемый runtime и CLAPCAL;
- общие helpers HSV/RGB/Kelvin;
- общая state machine верхних режимов;
- общая persistence layer;
- общая UART parser/dispatcher;
- mode-specific render functions без дублирования frontend/serial/persistence.

Причина: CORE R3 уже занимает 27822/30720 flash до Music/Ambient. Полная v1 требует рефакторинга и устранения дублированного кода до upload.

### A2. 44-LED geometry

Все Music renderer пересчитать на чётные 44 LED:
- M01/M02 — симметрия относительно центральной пары;
- M03 — 5 зон без потери крайних пикселей;
- M04 — 3 зоны с детерминированным распределением остатка;
- M05 — whole-ring band renderer;
- M08 — central pair вместо прежнего single center 43-LED;
- M09 — симметричный spectrum renderer для 44 LED.

Старые 43-LED hardware-tested файлы не переписывать.

### A3. Two-ring renderer

Финальная release должна сразу содержать:
- D6 = Ring A;
- D7 = Ring B;
- один логический кадр;
- зеркальный/идентичный вывод без отдельной mode state второго кольца.

Release должна корректно работать и при физически неподключённом D7, чтобы финальный Nano можно было прошить до потолочного монтажа.

### A4. Current limit без будущей перепрошивки

Нужны два уровня:
- hard ceiling, который нельзя превысить пользовательским API;
- persistent service setting внутри безопасного диапазона для подстройки после подключения обоих колец.

Исходная верхняя граница проекта — не более 4.5 A для LED-части при S-30-5 5 V / 6 A, с запасом на Nano/ESP/RTC и реальные провода.

Финальная прошивка должна позволять уменьшить рабочий limit после двухкольцевого power test без новой прошивки Nano.

### A5. Persistence v1

Сохранить требования `PERSISTENCE_V1.md`:
- Nano = источник истины;
- независимые настройки L01, каждого Music mode, Ambient, Night, Alarm/Dawn;
- clap enable/threshold/timeout;
- audio calibration, если используется;
- current limit service setting;
- magic/version/checksum;
- defaults при невалидном блоке;
- EEPROM.update/эквивалент;
- никаких EEPROM writes на каждом slider tick.

По возможности сохранить совместимость с уже записанными CORE R1 L01/Clap/Night/Alarm/Dawn областями. Если структура v1 меняется, сделать одноразовую миграцию либо безопасный import старых значений, а не молча уничтожать profile.

### A6. UART v1 freeze

До Nano upload зафиксировать окончательные команды как внутренний контракт ESP↔Nano.

Минимально нужны:
- PING;
- STATUS;
- SETTINGS/эквивалент полного saved state;
- TIME / SET;
- MODE;
- POWER;
- LIGHT ...;
- CLAP ...;
- CLAPCAL ...;
- MUSIC ...;
- AMBIENT ...;
- NIGHT ...;
- ALARM ...;
- DAWN ...;
- service current-limit command;
- async EVENT lines.

Правила:
- fixed input buffer;
- range validation;
- unknown command не меняет state;
- критическая команда возвращает ACK/ERR;
- пользовательский semantic HTTP API не зависит от raw command strings.

### A7. Release optimization

До физического upload:
1. убрать дублированные diagnostic/help строки;
2. оставить только полезные STATUS/ERR/EVENT;
3. использовать F()/PROGMEM там, где это реально экономит SRAM;
4. не держать два 44-LED framebuffer;
5. не дублировать FHT/audio buffers;
6. mode settings хранить компактно;
7. не использовать динамический String на Nano;
8. проверить большие локальные массивы/stack;
9. сохранить exact upstream FHT из repo.

Compile gate:
- Arduino Nano / ATmega328P / Old Bootloader;
- flash обязан помещаться в 30720 bytes;
- SRAM globals обязаны оставлять реальный запас stack/locals;
- целевой ориентир: flash ≤95%, globals ≤70%;
- превышение ориентира не ведёт к автоматическому удалению функций: сначала code-size/stack review и оптимизация;
- никакого физического Upload до завершения полного scope v1.

### A8. Единственная штатная загрузка Nano

Подготовка:
1. сохранить текущую рабочую CORE R1 как recovery baseline;
2. закрыть Serial Monitor;
3. временно разорвать `ESP TX → Nano D0/RX` — предыдущий опыт показал, что ESP Reset LOW недостаточен;
4. ESP можно удерживать Reset LOW;
5. Nano RESET оставить свободным;
6. Board = Arduino Nano;
7. Processor = ATmega328P (Old Bootloader);
8. Upload release v1;
9. после успешной записи вернуть ESP TX→Nano D0;
10. больше Nano не перепрошивать без release-blocker.

### A9. Один acceptance batch Nano

Это не серия новых firmware-итераций, а единый финальный приёмочный прогон:
- startup + STATUS;
- L01 ON/OFF/Kelvin/RGB/brightness/save;
- cold power cycle → saved L01;
- clap ON/OFF + double-clap;
- CLAPCAL полный цикл;
- RTC TIME;
- Night manual + schedule;
- Alarm + accelerated Dawn + STOP;
- Dawn recovery;
- F01/F02/F03;
- M01/M02/M03/M04/M05/M08/M09;
- persistence минимум L01 + один Music + Ambient + Night + Alarm;
- rapid UART commands при активном FastLED;
- D6 rendering;
- D7 output readiness.

Если найден только UI/API defect, Nano не перепрошивается.

## 5. Этап B — финальная ESP8266-прошивка v1

### B1. База

Использовать проверенные:
- Wi-Fi Station;
- HTTP transport;
- OTA;
- UART framing;
- L01 semantic behavior;
- `HTTP_BRIDGE_R7` как последнюю hardware-tested линию развития.

### B2. Полный semantic HTTP API

До ESP upload заморозить API v1.

Обязательно:
- `GET /api/ping`;
- `GET /api/status`;
- `GET /api/settings`;
- `GET /api/time`;
- `POST /api/power`;
- `POST /api/mode`;
- `GET/POST /api/light/...`;
- clap enable/status;
- clap calibration start/sample/finish/save/cancel/status;
- Music mode/settings/calibration;
- Ambient effect/settings;
- Night settings;
- Alarm settings;
- Dawn stop;
- time sync;
- events;
- Developer raw Nano command.

После каждого write ESP возвращает success только после ACK Nano и, где уместно, reread фактического state.

### B3. Discovery/reconnect

Финальный ESP сохраняет mDNS hostname `ardu`, но приложение не должно зависеть только от `ardu.local`, потому что Windows mDNS resolution уже оказался необязательным/нестабильным.

App strategy должна поддерживать:
- последний успешный адрес;
- `ardu.local` как optional path;
- ручной IP fallback;
- возможность позже добавить app-side LAN discovery без прошивки ESP.

Таким образом discovery не является причиной ещё одной ESP firmware revision.

### B4. OTA и credentials

- SSID/password/OTA password не коммитить;
- Station-only решение сохраняется;
- OTA остаётся включён как recovery/maintenance path;
- wired ESP programmer после v1 используется только если OTA недоступен.

### B5. Единственная финальная загрузка ESP

После закрытия Nano UART v1:
1. собрать ESP release против финального UART/API;
2. compile;
3. OTA upload;
4. reboot;
5. проверить /api/ping;
6. проверить STATUS/SETTINGS/TIME;
7. Developer PING→PONG;
8. один write каждого крупного раздела.

После этого ESP firmware freeze.

## 6. Этап C — Android functional completion

Приложение строится только после freeze Nano/ESP contracts.

### C1. Connection layer
- saved device address;
- connect/reconnect;
- online/offline state;
- timeout/errors;
- optional ardu.local;
- manual IP;
- retry;
- no IP/UART details in normal UI.

### C2. Initial synchronization
После connect:
1. GET /api/status;
2. GET /api/settings;
3. GET /api/time;
4. Nano state заменяет local cache;
5. UI рисуется по фактическим значениям устройства.

### C3. Обычный свет
Довести существующий экран:
- power;
- brightness;
- Kelvin slider;
- presets;
- RGB picker;
- saved/dirty;
- save startup profile;
- clap enable;
- clap calibration wizard.

### C4. Светомузыка
- M01/M02/M03/M04/M05/M08/M09;
- dynamic controls only where applicable;
- active/background brightness;
- smoothing;
- sensitivity;
- submode;
- speed;
- hue parameters;
- microphone calibration;
- текущий mode/state reread after change.

### C5. Фон
- F01/F02/F03;
- next/prev;
- auto-cycle;
- period;
- hue/saturation/brightness/speed/rainbow step.

### C6. Ночник
- ON/OFF;
- HSV;
- brightness;
- schedule enable;
- ON/OFF time;
- schedule state.

### C7. Будильник/рассвет
- alarm enable;
- HH:MM;
- fade minutes;
- max brightness;
- start/end colors;
- current DS3231 time;
- sync from phone;
- STOP active dawn;
- active/recovery state.

### C8. Developer Mode
- versions/status;
- raw command;
- full Nano response;
- events;
- audio diagnostics;
- current limit;
- RTC/Alarm/Dawn/Night diagnostics;
- destructive actions with confirmation.

## 7. Этап D — дизайн и UX

Дизайн начинается после полного functional UI, чтобы не переделывать экраны из-за changing API.

Работы:
- единая navigation model;
- main dashboard;
- five mode cards/sections;
- consistent sliders;
- color controls;
- schedule/time pickers;
- connection indicator;
- clear offline/error states;
- calibration wizard;
- typography/spacing;
- iconography;
- dark/light decision;
- app icon/name/version;
- Developer Mode hidden from normal flow.

Критерий: обычный пользователь не должен видеть UART, raw commands, firmware internals или необходимость понимать Nano/ESP.

## 8. Этап E — второй круг и силовая часть

Финальная Nano v1 уже поддерживает D7; физическое подключение выполняется после firmware/app functional freeze.

### E1. Ring B
- 44 WS2812B;
- D7 DATA;
- отдельный series DATA resistor 200..500 Ω;
- common GND;
- local bulk capacitor у входа кольца;
- mirror frame.

### E2. Power
На двух кольцах:
- измерить S-30-5 output under load;
- измерить 5 V непосредственно у каждого кольца;
- измерить падение по GND/+5V;
- проверить нагрев БП;
- проверить провода/клеммы;
- подобрать persistent working current limit через service setting, не меняя firmware;
- hard limit не превышать.

### E3. Physical topology — уточнено 2026-10-01

Подтверждено владельцем:
- расстояние между корпусами ≈ 2 м;
- доступа к проводке внутри стен нет;
- у первого/ближайшего ко входу корпуса доступна потолочная точка бывшей люстры;
- существующая линия к корпусам = 2×2,5 мм² от одного выключателя;
- ко второму корпусу владелец прокладывает новую 5-V low-voltage трассу за потолком.

Принятая топология:
- общий S-30-5 + Nano/ESP/RTC/MAX9814 размещаются у Ring A;
- Ring A: D6 и короткое локальное питание;
- Ring B: D7 и отдельная ~2 м low-voltage ветвь;
- к Ring B обязательно идут +5V, GND и DATA; DATA должен иметь рядом reference GND;
- существующую 230-V линию не использовать как DATA/signal path;
- если неиспользуемая 230-V жила остаётся у второго корпуса, она должна быть безопасно изолирована/терминирована как mains wiring.

Предварительный ориентир для новой 5-V ветви Ring B:
- питание +5V/GND — медь порядка 1,5 мм² или больше;
- DATA — отдельная сигнальная жила рядом с GND;
- окончательно подтвердить падение напряжения реальным двухкольцевым power test.

Открытый монтажный gate — уточнено по фото 2026-10-01:
- на конкретном S-30-5 явно есть клеммы `L / N / ⏚ / -V / +V`;
- корпус БП металлический, клемма `⏚` сейчас пустая;
- владелец подтвердил: отдельного PE в потолочной точке нет;
- поэтому S-30-5 исключён из финального потолочного монтажа и остаётся стендовым БП;
- нейтраль N не использовать как PE;
- финальный источник питания: Class II / double-insulated 230→5 V достаточной мощности, не требующий защитного заземления.

Эти данные больше не блокируют сборку final firmware/app.

## 9. Этап F — потолочный монтаж

До монтажа:
- безопасный закрытый корпус/место S-30-5;
- 230 V terminals;
- PE решение по фактическому корпусу/месту;
- low-voltage branch protection при необходимости;
- service access;
- strain relief;
- separation mains/low voltage.

После монтажа:
- Ring A/B visual sync;
- voltage under load;
- Wi-Fi connection;
- L01;
- clap;
- Music;
- Ambient;
- Night;
- Alarm/Dawn.

## 10. Этап G — финальная приёмка и release

Финальный сценарий:
1. полный power off;
2. wall switch ON;
3. saved L01 стартует;
4. app подключается;
5. state/settings/time читаются;
6. L01 controls;
7. clap;
8. все Music modes;
9. Ambient;
10. Night schedule;
11. Alarm/Dawn автономно без телефона;
12. Dawn STOP;
13. reset recovery;
14. Wi-Fi disconnect/reconnect;
15. ESP OTA availability;
16. оба кольца синхронны;
17. длительный прогон без reset/glitches;
18. после power cycle все saved settings восстановлены.

После PASS:
- firmware версии фиксируются как ARDU v1;
- Android версия фиксируется как v1.0;
- обновляются STATUS/HISTORY;
- создаётся release/tag;
- старые test sketches остаются history/reference;
- проект считается запущенным.

## 11. Что может реально потребовать ещё одну прошивку после freeze

Только release-blocker:
- crash/reset;
- corruption EEPROM;
- неверная работа Alarm/Dawn;
- повреждение UART/API contract;
- невозможность управления обязательным режимом;
- опасная ошибка current limiting/power behavior.

Не являются причиной firmware revision:
- цвет/размер кнопок;
- порядок экранов;
- app theme;
- wording;
- обычный network retry UX;
- ручной IP;
- визуальные app improvements.

## 12. Текущий следующий шаг

Не загружать CORE R3.

Следующая инженерная работа:
1. собрать исходник полной Nano v1 из проверенных подсистем;
2. интегрировать Music/Ambient/CLAPCAL/two-ring/persistence;
3. выполнить code-size/SRAM optimization;
4. заморозить UART v1;
5. Verify до приемлемого release budget;
6. только затем выполнить единственный финальный Nano upload.


## 13. Release-candidate checkpoint — 2026-10-01

### Nano v1
- path: `04_Прошивка/nano_ardu_v1/nano_ardu_v1.ino`;
- full target scope integrated;
- compact UART v1 frozen;
- CI PASS: 28024/30720 flash (91%), 1070/2048 globals (52%), 978 bytes free SRAM;
- physical upload pending.

### ESP8266 v1
- path: `05_WiFi_и_приложение/esp8266_ardu_v1/esp8266_ardu_v1.ino`;
- full semantic HTTP API + Station Wi-Fi + OTA;
- static Nano opcode mapping PASS;
- CI PASS: RAM 39%, IRAM 92%, IROM code 324376 bytes;
- physical OTA pending.

### Следующий gate
Никаких новых feature layers до загрузки. Выполнить `RELEASE_V1_UPLOAD_ACCEPTANCE.md`, затем firmware freeze и переход к Android.


## 14. Parallel Android checkpoint — 2026-10-02

Physical Nano final upload временно заблокирован текущей платой; replacement Nano заказана владельцем.

До её прихода без изменения frozen firmware/API выполнено:
- Android final API client;
- все 5 functional sections;
- clap calibration wizard;
- connection persistence/manual address;
- service/events/current-limit/numeric developer tools;
- JVM mock-ESP contract tests;
- debug APK CI build.

Android functional RC: `0.9-functional-v1-rc1`.

После новой Nano:
1. `RELEASE_V1_UPLOAD_ACCEPTANCE.md`;
2. `05_WiFi_и_приложение/ANDROID_V1_FUNCTIONAL_TEST.md`;
3. только после end-to-end PASS — Android visual design/UX polish.


## 15. Android design checkpoint — 2026-10-02

Android visual/UX polish was completed early while replacement Nano is in transit.

Release candidate: `0.10-design-v1-rc1`.

CI: contract tests PASS + debug APK PASS.

This does not advance hardware acceptance. When replacement Nano arrives, continue directly with final Nano/ESP upload acceptance and Android end-to-end test; only release-blocker UI/API issues justify further functional changes.


## 16. Android unified smart-home UI checkpoint — 2026-10-02

Android `0.13-smart-home-tabs-rc1` propagates the approved Light UI language across all five normal user tabs.

Source/static/CI gate:
- missing layout IDs: 0;
- duplicate layout IDs: 0;
- API contract tests: PASS;
- debug APK build: PASS (Actions run 37056022090).

Firmware/API remain frozen. Next owner-side software action is visual smoke of all five tabs on the stateful ESP mock; physical end-to-end still waits for replacement Nano.
