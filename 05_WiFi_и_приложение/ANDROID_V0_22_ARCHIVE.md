# Android v0.22 Home Hub archive

Дата: 2026-10-04

Перед началом следующего visual-parity pass текущая рабочая Android-реализация сохранена отдельной Git-веткой:

- branch: `archive/android-v0.22-homehub-2026-10-04`
- source commit: `a9daf3839786414b264f32dfe0a680b68f8a7be3`
- Android version: `0.22-night-alarm-homehub-rc1`

Эта ветка сохраняет предыдущий вариант приложения целиком вместе с состоянием проекта на момент архивации.

**2026-10-07:** владелец отверг v0.23 и вернулся к этому visual baseline. Канонический `main` теперь также восстановлен к этому UI как `0.22.1-homehub-restored-rc1`, при этом более новые сетевые изменения ARDU-DIRECT сохранены.

Назначение: безопасный rollback/reference, если следующий visual-parity pass окажется хуже.

Firmware/API/UART contracts в архиве соответствуют текущему frozen ARDU v1.
