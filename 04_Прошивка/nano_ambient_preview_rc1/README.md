# ВАЖНО: RC1 заменена на RC2

Владелец сообщил ошибку компиляции RC1 `ambient_preset_probe.h: No such file or directory` при наличии заголовка в той же папке. Причина обработки файлов на его ПК не подтверждена. **Для следующей попытки используй [nano_ambient_preview_rc2](../nano_ambient_preview_rc2/README.md)** — все алгоритмы встроены в один `.ino`, второй файл не нужен. RC1 сохранена только для истории и не рекомендуется для дальнейшей загрузки.

---

# ARDU Ambient preview RC1 — полный Nano-скетч

- Открыть `nano_ambient_preview_rc1.ino` из этой папки в Arduino IDE. `ambient_preset_probe.h` должен находиться рядом.
- Этот исходник побайтно соответствует текущему `../nano_ardu_v1/nano_ardu_v1.ino` плюс `#define ARDU_AMBIENT_PRESET_PROBE 1`; CI контролирует синхронизацию копий. Встроены L01, Music, Night, RTC, Alarm/Dawn, Clap и старые F01/F02/F03.
- В PREVIEW RC выбор «Фон» → F01 показывает P01 Северное сияние → P04 Космос → P05 Камин → стандартный F01 по ~8,2 с, с повтором. Новых wire ID нет.
- [CI сборка PASS и HEX](https://github.com/dronrome1245/ARDU/actions/runs/37809369365) / [artifact 11564965835](https://github.com/dronrome1245/ARDU/actions/runs/37809369365/artifacts/11564965835).
- **До Upload обязательно прочитать [AMBIENT_PRESETS_PREVIEW_RC1_TEST.md](../AMBIENT_PRESETS_PREVIEW_RC1_TEST.md):** Nano Old Bootloader, отключение ESP TX от D0, безопасное питание и rollback. Это испытательная полная прошивка, не финальный релиз.
