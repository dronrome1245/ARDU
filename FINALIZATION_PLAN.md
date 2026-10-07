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


## 17. Owner-approved Android final UI checkpoint — 2026-10-03

Final design direction is frozen by owner:
`Home Hub + Focus Dial`, mint-dark.

Implementation RC: `0.14-ios-homehub-focus-rc1`.

Source/CI gate:
- native Home Hub context hero: implemented;
- native Focus Dial: implemented;
- Light overview/detail split: implemented;
- contextual heroes Music/Ambient/Night/Alarm: implemented;
- HTTP/UART/firmware changes: none;
- contract tests: PASS;
- debug APK: PASS;
- Actions run: 37062432020.

Remaining Android work before hardware end-to-end is visual smoke/polish only. Do not redesign the navigation/interaction model without a new explicit owner decision.


## 18. Premium Light visual gate — 2026-10-03

Android `0.15-premium-light-rc1` is the first product-level implementation of the owner-approved concept.

PASS:
- visual Light scene tiles implemented;
- Home Hub hero depth pass implemented;
- Focus Dial premium rendering implemented;
- HTTP/UART/firmware unchanged;
- contract tests PASS;
- debug APK PASS;
- Actions run 37102197040.

Next: owner visual smoke. If accepted, propagate this exact quality level to Music/Ambient/Night/Alarm; do not redesign the approved Home Hub + Focus Dial architecture.


## 19. Real-asset Light gate — 2026-10-03

Android `0.16-real-assets-light-rc1` closes the gap between the native prototype and the approved premium concept for the Light tab.

Implemented:
- real WebP hero;
- real WebP scene tiles;
- hero-overlay status/header controls;
- glass ordinary-light card;
- Focus Dial retained;
- API/firmware unchanged.

CI PASS: run 37105664138.

Next software gate is owner-phone visual review. If accepted, generate/integrate matching real photo assets for Music/Ambient/Night/Alarm while preserving their existing functionality.


## 20. Contextual photo hero checkpoint — 2026-10-03

Owner-phone visual check Light v0.17: PASS.

Android `0.18-photo-heroes-rc1`:
- synthetic Canvas room geometry removed from `RoomHeroView`;
- Music/Ambient/Night/Alarm render from the approved real-photo high-resolution master;
- each context has an independent focal crop and scene grade;
- semantic controls/functionality unchanged;
- HTTP/UART/firmware unchanged;
- Android Verify run 37111268646 PASS: contract tests + debug APK.

This is a deliberate intermediate visual gate rather than a claim that four unique context photos are already final. Next software action while replacement Nano is pending: owner-phone smoke of all four contextual heroes. If composition passes, replace the shared master with separate optimized generated WebP assets only where the unique room context materially improves the approved Home Hub design.

Physical gate remains replacement Nano → final Nano upload → ESP OTA → firmware acceptance → Android end-to-end acceptance.


## 21. Music/header polish checkpoint — 2026-10-03

Owner review of Android v0.18 accepted the contextual photo screens overall and identified two release-polish defects.

Android `0.19-music-header-polish-rc1`:
- Music two-line mode cards enlarged and text layout normalized to prevent bottom clipping;
- shared top status/header outer rectangle removed on non-Light normal tabs;
- Light hero/status treatment unchanged;
- HTTP/UART/firmware unchanged;
- Android Verify run 37112061533 PASS.

Next software action: one phone smoke confirming Music labels and frameless top status. Physical gate remains replacement Nano → final Nano upload → ESP OTA → firmware acceptance → Android end-to-end acceptance.


## 22. Temporary firmware-source exception + Music v0.20 plan — 2026-10-04

Latest owner decision temporarily relaxes the source-only firmware freeze while replacement Nano is pending. Nano/ESP source may be changed when justified, but every change must retain exact-target compile/size/regression gates. Physical final upload still waits for the replacement Nano and remains governed by `RELEASE_V1_UPLOAD_ACCEPTANCE.md`. Frozen UART/API should not be changed without a concrete blocker.

Current Android next implementation is documented in `05_WiFi_и_приложение/MUSIC_V0_20_PLAN.md`. The planned Music UX uses existing API/UART capabilities and therefore does not currently require firmware changes.


## 23. Music Home Hub implementation checkpoint — 2026-10-04

Android `0.20-music-homehub-rc1` implemented and CI-green.

- dedicated Music hero asset;
- Home Hub header/status inside hero;
- no fake player;
- no separate Play/Stop;
- mode card = direct select/start;
- leave active Music through another bottom user tab = existing top-level OFF;
- mode chooser = equal-height 4+3 grid, labels without VU;
- reusable SmartSlider endpoint clipping fix;
- icon-only bottom navigation;
- firmware/API/UART unchanged;
- static ID/function validation clean;
- Actions run 37194558467 PASS.

Next software gate: owner-phone visual/function smoke on the stateful mock. Physical gate remains replacement Nano → final Nano upload → ESP OTA → firmware acceptance → Android end-to-end acceptance.


## 24. Music visual hotfix checkpoint — 2026-10-04

Android `0.20.1-music-visual-hotfix-rc1` implemented after owner phone review.

- hero over-zoom corrected by fit-centered primary image + subdued full-bleed backdrop;
- illustrated Music mode tiles implemented;
- bottom navigation changed to centered ImageButtons inside separate dock;
- Music behavior from v0.20 unchanged;
- firmware/API/UART unchanged;
- static ID/function validation clean;
- Android Verify run 37196929089 PASS.

Next software action: owner-phone visual smoke v0.20.1. Physical gate remains replacement Nano → final Nano upload → ESP OTA → firmware acceptance → Android end-to-end acceptance.

## 25. Ambient Home Hub visual checkpoint — 2026-10-04

Android `0.21-ambient-homehub-rc1` advances the approved Home Hub visual language to Ambient without changing firmware/API contracts.

Implemented:
- Ambient status/actions integrated into 250dp hero;
- no separate global status header on Ambient;
- illustrated F01/F02/F03 native tiles;
- existing semantic ON/OFF/settings behavior retained.

Static source validation is clean. The exact v0.21 GitHub Actions result is not yet visible through the current connector and must not be reported as PASS until observed.

Software next gate: owner-phone visual smoke of Ambient. If accepted, Night is the next small premium-polish iteration.
Physical next gate: final ESP v1 OTA, then firmware acceptance and Android end-to-end acceptance.

## 26. Normal-tab Home Hub completion — 2026-10-04

Android `0.22-night-alarm-homehub-rc1` completes the owner-approved premium presentation across all five normal user tabs.

- Light: Home Hub + Focus Dial.
- Music: approved room hero + illustrated modes.
- Ambient: Home Hub + illustrated F01/F02/F03.
- Night: Home Hub + dynamic night/schedule summary.
- Alarm/Dawn: Home Hub + alarm/RTC summary.

The iteration is presentation-only; frozen HTTP API v1, numeric UART v1 and firmware remain unchanged.

Static source validation is clean. Exact v0.22 CI/Actions status has not yet been observed through the current GitHub connector.

Software gate: owner-phone visual smoke Night + Alarm, then only release-blocker UI polish.
Physical gate has advanced beyond ESP OTA: real status/settings transport PASS; sync invalid RTC next, then continue firmware + Android acceptance.

## 27. Real-device visual parity checkpoint — 2026-10-04 — REJECTED 2026-10-07

Owner confirmed real Android → Wi‑Fi → ESP → Nano connection and supplied live-device video/screenshots.

A reversible visual iteration was authorized:
- baseline preserved in `archive/android-v0.22-homehub-2026-10-04`;
- new source = `0.23-concept-parity-rc1`;
- firmware/API/UART unchanged;
- unsupported concept-only functions explicitly excluded.

Static source validation is clean. Exact CI result not visible yet.

Next: owner-phone Run against the real device and compare v0.23 to both the concept boards and archived v0.22.

## 28. Owner rollback to v0.22 visual baseline — 2026-10-07

Owner rejected Android v0.23 and selected the previous v0.22 UI.

Canonical source is now `0.22.1-homehub-restored-rc1`:
- exact v0.22 visual files restored;
- ARDU-DIRECT Android/network improvements retained;
- v0.23 stays only in Git history/reference.

Next gates:
1. restored UI build/run smoke from `main`;
2. physical ARDU-DIRECT smoke;
3. normal two-ring power acceptance.

No broad Android redesign is planned.
