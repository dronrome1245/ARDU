# Android v0.23 — concept parity pass

Дата: 2026-10-04  
Версия: `0.23-concept-parity-rc1`

**Статус: ОТКЛОНЁН ВЛАДЕЛЬЦЕМ 2026-10-07. Не использовать как текущий visual baseline.**

## Цель

Приблизить физическое Android-приложение к утверждённым concept screens, не ломая уже работающий реальный тракт:

`Android → Wi-Fi → ARDU_ESP_V1 → numeric UART v1 → ARDU_V1`.

Новые device functions не добавляются.

## Безопасный rollback

Предыдущая реализация полностью сохранена:

- branch: `archive/android-v0.22-homehub-2026-10-04`
- commit: `a9daf3839786414b264f32dfe0a680b68f8a7be3`
- manifest: `05_WiFi_и_приложение/ANDROID_V0_22_ARCHIVE.md`

## Изменения v0.23

### Свет
- Home Hub hero компактнее;
- quick scenes компактнее;
- app-side scenes теперь визуально соответствуют concept language:
  - Вечер;
  - Кино;
  - Гости;
  - Чтение;
- overview brightness и Kelvin больше не статические summary tiles: это реальные interactive SmartSlider cards;
- Focus Dial уменьшен до более компактной геометрии.

### Музыка
- fake media player не возвращается;
- семь реальных Music modes сохранены;
- mode chooser уплотнён;
- section language = «Реакция на музыку»;
- direct-start и stop-on-leave contract не изменён.

### Фон
- hero получает отдельный media-wall / RGB-TV visual context;
- F01/F02/F03 оставлены реальными device modes;
- effect tiles компактнее;
- Hue + brightness остаются primary controls;
- saturation/speed/rainbow-specific parameters скрыты под «Параметры эффекта»;
- auto-cycle остаётся отдельным real device control.

### Ночь
- hero визуально превращён в bedroom/night scene: окно, звёзды, кровать, тёплая лампа;
- brightness становится primary moon card с +/-;
- hue остаётся primary color control;
- saturation перенесена в «Точную настройку цвета»;
- schedule сохранён и уплотнён;
- unsupported sleep presets / night fade не добавлялись.

### Будильник / Рассвет
- hero получает отдельный dawn-bedroom context;
- HH:MM + enable остаются primary card;
- fade + max brightness остаются real dawn controls;
- end hue = primary «Цвет пробуждения»;
- start hue перенесён во вторичный раскрывающийся control;
- RTC sync и Stop Dawn сохранены;
- fake sound volume не добавлялся, потому что ARDU v1 не имеет audio-output subsystem.

### Общий polish
- успешные device writes больше не оставляют постоянную строку «...: готово»;
- firmware, HTTP API v1 и numeric UART v1 не изменены.

## Static validation

- layout IDs: 187;
- duplicate layout IDs: 0;
- missing Kotlin bindings: 0;
- duplicate private functions: 0;
- MainActivity Kotlin brace balance: clean;
- custom view brace balance: clean;
- XML container tag balance: clean.

Exact GitHub Actions result for this head is not yet visible through the connector.

## Phone smoke

1. `git pull`.
2. Android Studio → Run.
3. Keep phone on the same Wi-Fi as `192.168.0.4`.
4. Confirm Online and real state sync.
5. Visually compare all five tabs with the concept boards.
6. Functional smoke:
   - Light brightness/Kelvin;
   - one Music mode;
   - one Ambient effect;
   - Night brightness +/-;
   - Alarm enable/time readback.
7. Report only visual/layout defects unless a real control regression appears.
