# ARDU — Android L01 stage 1 on existing HTTP_BRIDGE_R1

Дата: 2026-09-30

## Цель

Проверить пользовательский L01 без повторной перепрошивки уже компактно спаянного ESP.

Физический ESP остаётся на hardware-tested `HTTP_BRIDGE_R1`. Android transport layer временно использует существующий `POST /api/dev/nano` внутри приложения, но обычный UI raw-команды не показывает.

## Подготовка

1. `cd C:\ARDU`
2. `git pull`
3. Android Studio → Sync Project with Gradle Files.
4. ESP не прошивать; проводку не менять.
5. Запустить приложение на том же физическом телефоне.

## Gate 1 — connection

Ожидается:
`Онлайн • HTTP_BRIDGE_R1`.

## Gate 2 — L01 read

Блок `Обычный свет L01` должен показать:
- Включён/Выключен;
- Kelvin или RGB;
- brightness.

Источник — Nano `STATUS` + `LIGHT STATUS`.

## Gate 3 — ON

При исходном `MODE=OFF`:
1. нажать `Включить`;
2. D6 ring должен включиться сохранённым L01 profile;
3. основной STATUS должен показать `MODE=L01`;
4. L01 block должен показать `Включён`.

## Gate 4 — OFF

1. нажать `Выключить`;
2. D6 ring должен погаснуть;
3. STATUS должен показать `MODE=OFF`;
4. L01 block должен показать `Выключен`.

## Gate 5 — repeat/reopen

- повторить ON/OFF минимум 3 цикла;
- `UPTIME_MS` должен расти, reset отсутствует;
- закрыть приложение полностью и открыть снова;
- UI должен перечитать фактическое состояние Nano, а не использовать старый local state.

## Gate 6 — transport regression

Developer Mode:
- `PING` → `PONG`;
- `TIME` → валидное DS3231 time;
- `LIGHT STATUS` → валидный profile.

Основные STATUS/TIME после нажатия `Обновить` остаются рабочими.

## PASS

Stage 1 PASS, если L01 read + ON/OFF + reopen-state проходят на физическом HTTP_BRIDGE_R1 без вмешательства в ESP hardware.

Следующий слой после PASS: brightness + Kelvin presets 2700/4000/6000 тем же app-side compatibility adapter.
