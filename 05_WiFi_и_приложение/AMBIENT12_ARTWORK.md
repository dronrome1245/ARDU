# ARDU / Ambient12 — одобренные изображения маленьких плиток

**Дата:** 2026-10-08. **Решение:** D-111.  
**Статус:** Все 12 оригинальных иллюстраций из трёх утверждённых пользователем коллажей встроены в Android; [CI PASS](https://github.com/dronrome1245/ARDU/actions/runs/37838263956), локальный визуальный smoke на телефоне ожидается.

## Соответствие изображений пресетам

| Preset | Сцена | Ресурс drawable-nodpi |
| --- | --- | --- |
| P01 | Северное сияние | `ardu_preset_p01.webp` |
| P02 | Закат на Бали | `ardu_preset_p02.webp` |
| P03 | Океан | `ardu_preset_p03.webp` |
| P04 | Космос | `ardu_preset_p04.webp` |
| P05 | Камин | `ardu_preset_p05.webp` |
| P06 | Свечи | `ardu_preset_p06.webp` |
| P07 | Лунный свет | `ardu_preset_p07.webp` |
| P08 | Лес | `ardu_preset_p08.webp` |
| P09 | Неон | `ardu_preset_p09.webp` |
| P10 | Лава | `ardu_preset_p10.webp` |
| P11 | Дыхание | `ardu_preset_p11.webp` |
| P12 | Радуга | `ardu_preset_p12.webp` |

**Размер:** 192×192 WebP (lossy, quality 72), `drawable-nodpi`, никаких сетевых запросов во время работы. В среднем 6–8 КБ на файл, суммарно около 76 КБ. Исходные утверждённые изображения были представлены владельцем тремя 2×2 коллажами; каждое фото обрезано отдельно, без фона коллажа, затем уменьшено и упаковано для небольших картинок. Мебели/интерьеров нет.

**Где подключается:**
- `app/src/main/res/layout/activity_main.xml`: P01…P04 фиксированные `ImageButton`;
- `app/src/main/java/com/ardu/app/MainActivity.kt`: P05…P12 добавлены в `ambientPresetExtraGrid` по 4 колонки, 2 ряда, все изображения/подписи сопоставлены P-ID и выделяются при выборе;
- `app/src/main/res/drawable/ardu_preset_thumb.xml`: сохранена рамка выбранного изображения.

**Протокол/устройство:** Изменён только Android. Nano v2/ESP v2 и REST API не тронуты. Следующее действие владельца — `git pull` → Android Studio Run → оценить маленькие плитки, читаемость подписей, работу выбранной сцены, сохранение яркости/динамики. Скриншот экрана нужен для визуальной приёмки, CI подтверждает лишь source/build PASS.

**Готовый APK** (если нужен вместо Android Studio): [CI artifact 11576287564](https://github.com/dronrome1245/ARDU/actions/runs/37838263956/artifacts/11576287564). Для обычной проверки рекомендуем Android Studio Run, чтобы не столкнуться с различиями debug signing keys.
