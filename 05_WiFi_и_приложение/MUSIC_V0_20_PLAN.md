# ARDU Android — Music v0.20 implementation plan

Дата: 2026-10-04  
Статус: OWNER-APPROVED PLAN  
Целевая версия: `0.20-music-homehub-rc1`

## 1. Цель

Перестроить вкладку **Музыка** по утверждённому Home Hub language, сохранив реальные функции ARDU и убрав элементы, которые выглядят как управление медиаплеером.

Итоговая логика:

`tap Music mode card → selected mode starts immediately`

`leave Music through bottom user navigation → active Music stops (mode=off)`

Новый firmware/API contract для этого не требуется.

## 2. Что подтверждено текущим кодом

- Android `selectMusicMode(id)` вызывает `POST /api/music/mode`.
- ESP mapping переводит M01..M09 в UART opcode 80.
- Nano opcode 80 сохраняет selected Music mode и вызывает `applyMode(MUSIC, true)`.
- Поэтому карточка режима уже может быть единственным start action.
- Existing `POST /api/mode {"mode":"off"}` достаточно для остановки Music при уходе с вкладки.
- Current `SmartSliderView` рисует thumb radius 17dp при horizontal padding 15dp; поэтому на min/max thumb/glow физически может выходить за границы View.

## 3. Итерация A — отдельный Music hero asset

### Asset
Создать новый файл:

`app/src/main/res/drawable-nodpi/ardu_music_hero.webp`

Исходная композиция:
- photorealistic modern living room;
- большая акустическая колонка справа;
- тонкие cyan/teal светящиеся элементы на колонке;
- диван/комната слева и в центре;
- тёплый торшер как контраст;
- dark teal / cyan / warm amber palette;
- достаточно спокойного пространства сверху слева под ARDU/status/title overlays;
- без людей;
- без текста;
- без телефона;
- без album art;
- без media-player UI.

Рабочий master: не меньше 768×512 до WebP optimization. Финальный drawable оптимизировать без заметных артефактов.

### Layout
Music top area становится таким же композиционным типом, как Light Home Hub:
- `FrameLayout` full-width hero;
- dedicated `FocalCropImageView`;
- readability overlay;
- ARDU / Online / Refresh / Settings внутри hero;
- title `Музыка`;
- room/context subtitle;
- короткий context copy;
- **никакой отдельной hero-card ниже заголовка**;
- **никакого media player**.

При открытой Music вкладке общий `globalHeaderPanel` скрывается так же, как на Light.

## 4. Итерация B — Music mode chooser и вариант A

Удалить из Music layout:
- `musicOnButton`;
- `musicOffButton`;
- визуальный Play/Stop row.

Карточка режима становится единственным основным start action.

Имена:
- M01 — `Градиент`;
- M02 — `Радуга`;
- M03 — `5 полос`;
- M04 — `3 полосы`;
- M05 — `Частота`;
- M08 — `Бегущие`;
- M09 — `Спектр`.

### Геометрия
Заменить текущий `HorizontalScrollView` на равномерную grid-композицию:
- 3 колонки на типичном телефоне;
- одинаковая ширина внутри ряда;
- одинаковая высота 64dp;
- text maxLines=2, но текущие названия должны помещаться без обрезания;
- selected state остаётся mint;
- последний ряд визуально центрируется, а не прилипает к левому краю.

Критерий: все 7 режимов видны без горизонтального scroll и без clipped text.

## 5. Итерация C — stop Music при уходе через bottom navigation

В `MainActivity` добавить актуальный UI-level snapshot текущего top mode, например `latestMode`.

Navigation rule:
1. Если текущая вкладка = Music.
2. Если фактический `latestMode == "music"`.
3. Если пользователь нажал любую другую **нижнюю пользовательскую вкладку**:
   - показать выбранную вкладку без лишней задержки;
   - асинхронно отправить `api.setMode("off")`;
   - reread status/settings/time;
   - обновить UI фактическим state.
4. Если Music уже не активна — ничего лишнего не отправлять.
5. Нажатие Settings не считается выбором другого пользовательского режима и само по себе Music не выключает.
6. Переход на Light/Ambient/Night/Alarm не включает целевой режим автоматически.

Offline/error case:
- навигация остаётся доступной;
- если stop command не дошла, UI показывает ошибку связи и не утверждает, что устройство выключилось.

## 6. Итерация D — глобальный fix SmartSliderView

Причина current bug:
- `horizontalPadding = 15dp`;
- `thumbRadius = 17dp`;
- плюс shadow/glow;
- min/max center оказывается слишком близко к View edge.

Исправление в **одном reusable component**:
- ввести `edgeInset` не меньше `thumbRadius + shadow/glow reserve`;
- track left/right строить по `edgeInset`;
- `xForValue()` использовать тот же диапазон;
- touch mapping `updateFromX()` использовать тот же диапазон;
- сохранить min/max value semantics;
- проверить vertical shadow clearance.

Regression matrix:
- Light Kelvin;
- Music effect/background/smoothing/sensitivity/speed/aux/hue;
- Ambient hue/saturation/brightness/speed/rainbow/period;
- Night hue/saturation/brightness;
- Alarm fade/max brightness/start hue/end hue.

Для каждого: min, max и несколько промежуточных значений.

Stock technical SeekBars (precise RGB/current-limit) проверить отдельно; менять только если баг воспроизводится и на них.

## 7. Итерация E — icon-only bottom navigation

Заменить текстовые bottom buttons на native vector-icon navigation без сторонней библиотеки.

Иконки:
- Свет — bulb;
- Музыка — music note;
- Фон — sparkle/sun;
- Ночь — moon;
- Будильник — alarm clock.

Правила:
- selected item = существующий mint pill/background;
- unselected = secondary icon tint;
- no visible text labels;
- у каждого элемента есть `contentDescription`;
- fixed five-item bar, no horizontal scrolling;
- hit target не меньше 48dp.

## 8. Firmware scope для v0.20

Для описанных Music UX изменений firmware менять **не нужно**:
- mode-card start уже обеспечен opcode 80;
- leave-tab stop уже обеспечен top-level mode OFF.

Разрешение владельца менять firmware во время ожидания replacement Nano зафиксировано отдельно. Если при реализации найдётся реальный firmware/API blocker, изменение выполняется отдельной маленькой итерацией с compile/size gate.

## 9. Порядок реализации

1. Сгенерировать и оптимизировать `ardu_music_hero.webp`.
2. Перевести Music header/hero на Light-like Home Hub composition.
3. Удалить Play/Stop.
4. Перестроить mode chooser в equal-height grid и убрать `VU`.
5. Реализовать stop-on-leave Music в bottom navigation.
6. Исправить `SmartSliderView` edge geometry глобально.
7. Перевести bottom nav на vector icons.
8. Static layout/ID checks.
9. JVM HTTP contract tests.
10. Debug APK build.
11. Owner-phone visual/function smoke на stateful mock.
12. Только после PASS обновить следующий UI gate.

## 10. Acceptance v0.20

PASS если:
- Music hero выглядит как часть Home Hub, а не отдельная картинка-карточка;
- hero использует отдельную music-room фотографию;
- player отсутствует;
- Play/Stop controls отсутствуют;
- tap каждой из 7 mode cards включает соответствующий Music mode;
- названия без `VU`;
- все mode cards равной высоты и ничего не обрезается;
- уход Music → Light/Ambient/Night/Alarm останавливает активную светомузыку;
- целевая вкладка при навигации сама не включается;
- все SmartSlider thumbs полностью видны на min/max на всех user tabs;
- bottom navigation использует icons, не visible text labels;
- firmware/API regression отсутствует;
- contract tests PASS;
- debug APK build PASS.
