# ARDU Nano ↔ ESP8266 UART v1

Дата: 2026-10-01  
Статус: RELEASE v1 CONTRACT; backward-compatible owner-requested extension 2026-10-07

Этот протокол используется только внутри устройства между ESP8266 и Arduino Nano. Android-приложение его не знает и работает только с semantic HTTP API ESP8266.

## 1. Формат

Запрос ESP→Nano:

```text
<opcode> [arg0 ... arg5]\n
```

Все значения — беззнаковые десятичные целые числа.

Ответы Nano:

- `O <opcode>` — команда применена;
- `E <code>` — ошибка;
- `D <group> ...` — данные;
- `V <event> [args...]` — асинхронное событие;
- `ARDU1 1` — startup banner Nano v1.

ESP фильтрует `V ...` из синхронных транзакций и сохраняет их для `GET /api/events`.

## 2. Ошибки

| Код | Значение |
|---:|---|
| 1 | parse / malformed command |
| 2 | value out of range |
| 3 | RTC unavailable/invalid |
| 4 | invalid state |
| 5 | parameter not applicable to current mode |

## 3. Общие команды

| Opcode | Аргументы | Ответ | Смысл |
|---:|---|---|---|
| 1 | — | `O 1` | PING |
| 2 | — | `D 2 ...` | compact status |
| 3 | — | несколько `D 3 <item> ...` + `O 3` | полный settings snapshot |
| 4 | — | `D 4 ...` | DS3231 time |
| 5 | Y M D h m s | `O 5` | установить RTC |
| 10 | mode | `O 10` | top mode: 0 off, 1 light, 2 night, 3 ambient, 4 music |
| 120 | mA | `O 120` | service current limit, 500..4500 mA, persistent |
| 130 | — | `O 130` | сброс пользовательских настроек к v1 defaults; RTC и audio calibration сохраняются |

## 4. L01 / clap

| Opcode | Аргументы | Ответ | Смысл |
|---:|---|---|---|
| 20 | 0/1 | `O 20` | L01 OFF/ON |
| 21 | 0..255 | `O 21` | brightness RAM preview |
| 22 | 1800..6500 | `O 22` | Kelvin RAM preview |
| 23 | r g b | `O 23` | RGB RAM preview |
| 24 | — | `O 24` | сохранить L01 startup profile |
| 26 | 0/1 | `O 26` | clap enable, persistent |

### Family CLAPCAL

| Opcode | Аргументы | Ответ |
|---:|---|---|
| 40 | pairs 3..12 | `D 40 0 quiet_p99 target_pairs` |
| 41 | — | `D 41 0 detected_count` или `D 41 1 good target weak strong` |
| 42 | — | `D 42 0 quiet_p99 clap_p20 suggested` |
| 43 | — | `O 43` — save |
| 44 | — | `O 44` — cancel |
| 45 | — | `D 45 0 active finished good target quiet suggested` |

## 5. Night

| Opcode | Аргументы | Ответ | Persistence |
|---:|---|---|---|
| 50 | 0/1 | `O 50` | manual power + save |
| 51 | hue | `O 51` | RAM preview |
| 52 | saturation | `O 52` | RAM preview |
| 53 | brightness | `O 53` | RAM preview |
| 54 | 0/1 | `O 54` | schedule enable + save |
| 55 | on_h on_m off_h off_m | `O 55` | schedule times + save |
| 56 | — | `O 56` | commit current Night HSV/brightness |

Night sliders MUST use 51/52/53 during live preview and 56 once at edit completion.

## 6. Alarm / Dawn

| Opcode | Аргументы | Ответ | Persistence |
|---:|---|---|---|
| 60 | 0/1 | `O 60` | alarm enable + save |
| 61 | h m | `O 61` | alarm time + save |
| 71 | — | `O 71` | stop active dawn |
| 73 | 1..120 | `O 73` | fade minutes RAM preview |
| 74 | 1..255 | `O 74` | max brightness RAM preview |
| 75 | 0..255 | `O 75` | start hue RAM preview |
| 76 | 0..255 | `O 76` | end hue RAM preview |
| 77 | — | `O 77` | commit Dawn parameters |

## 7. Music

Stable index mapping:

- 0=M01
- 1=M02
- 2=M03
- 3=M04
- 4=M05
- 5=M08
- 6=M09

| Opcode | Аргументы | Ответ |
|---:|---|---|
| 80 | mode index | `O 80` |
| 81 | brightness 0..255 | `O 81` |
| 82 | background 0..255 | `O 82` |
| 83 | smoothing 5..100 | `O 83` |
| 84 | sensitivity 50..200 | `O 84` |
| 85 | submode 0..3 | `O 85`, only M05/M08 |
| 86 | speed 1..255 | `O 86`, only M08 |
| 87 | value | `O 87`: M02 rainbow_step10 5..200 or M09 hue_step 1..255 |
| 88 | hue_start 0..255 | `O 88`, only M09 |
| 89 | — | `O 89` — commit Music settings |
| 90 | — | `D 90 0 dc vu_low_pass spectrum_low_pass` — MAX9814 calibration |

Submode mapping: 0 three, 1 low, 2 mid, 3 high.

## 8. Ambient

Effect mapping: 0=F01, 1=F02, 2=F03.

| Opcode | Аргументы | Ответ |
|---:|---|---|
| 100 | effect | `O 100` |
| 103 | auto 0/1 | `O 103` |
| 104 | period_s 1..255 | `O 104` |
| 105 | hue 0..255 | `O 105` |
| 106 | saturation 0..255 | `O 106`, not F03 |
| 107 | brightness 0..255 | `O 107` |
| 108 | speed 1..255 | `O 108`, F02/F03; большее значение = быстрее |
| 109 | step10 5..100 | `O 109`, F03 |
| 110 | — | `O 110` — commit Ambient settings |

ESP v1 automatically calls 110 after a user changes the selected Ambient effect.

## 9. Status data

`D 2`:

```text
D 2 current_mode rtc_valid clap_enabled clap_threshold clap_timeout_ms
    current_limit_ma selected_music ambient_effect reset_flags uptime_ms
```

Internal `current_mode` enum:

- 0 off
- 1 light
- 2 night
- 3 dawn
- 4 ambient
- 5 music

## 10. Settings snapshot

Opcode 3 emits:

- item 0 — L01;
- item 1 — clap + calibration state;
- item 2 — Night;
- item 3 — Alarm/Dawn;
- item 4 — Ambient;
- items 10..16 — M01/M02/M03/M04/M05/M08/M09;
- item 20 — current limit + audio calibration values;
- final `O 3`.

This is the source for `GET /api/settings`.

## 11. Async events

| Event | Смысл | Args |
|---:|---|---|
| V 1 | Night schedule changed effective power | power 0/1 |
| V 2 | Dawn started | source, duration_s |
| V 3 | Dawn stopped | — |
| V 4 | Dawn completed | — |
| V 5 | Dawn recovered | phase, source |
| V 6 | Alarm triggered | — |
| V 7 | Clap toggled L01 | power 0/1 |
| V 8 | cold power-on → L01 | — |
| V 9 | warm reset restore | restored internal mode |

## 12. Freeze rule

Этот контракт считается frozen для ARDU v1 после compile PASS Nano+ESP. До физической загрузки допускается только исправление найденного release-blocker. Android не должен формировать эти opcodes напрямую.


### Extension rule 2026-10-07

Opcode 130 добавлен по прямому запросу владельца на пользовательскую кнопку «Сброс настроек по умолчанию». Это backward-compatible extension: существующие opcodes и их значения не изменены, `uart_protocol` остаётся `1`.

Одновременно исправлена только внутренняя интерпретация Ambient `speed` для F02/F03: wire range `1..255` не изменён; ранее F03 использовал raw 8-bit hue step и мог alias-иться (например 247 ≈ -9), теперь значение монотонно управляет интервалом кадра.

### D-104: No UART change for proposed Ambient presets (2026-10-08)

Future Ambient UI will keep F01 and omit F02/F03 (owner decision). The current v1 numeric UART still supports their existing mappings, and no firmware/source changes were made in this documentation-only step. Proposed `P01…P12`, UART opcodes, persisted schema and any new statuses are **IDEAS / NOT FROZEN**. See `04_Прошивка/AMBIENT_PRESETS_CONCEPT.md`.

## D-109 — P01..P12 additive internal UART contract for full Nano v2 (2026-10-08)

Other UART v1 numeric commands for L01/Music/Night/RTC/Clap remain unchanged. Opcode `100 0` selects F01 manual and clears P. Old `100 1` and `100 2` (retired F02/F03) return `E 2` and are NOT reused.

| Numeric opcode | Args | Normal response | Meaning |
| ---: | --- | --- | --- |
| `111` | `1..12` | `O 111` | Select P01..P12 (activate Ambient) |
| `112` | `0..255` | `O 112` | Update selected P brightness in Nano RAM |
| `113` | `0..255` | `O 113` | Update selected P dynamics in Nano RAM |
| `114` | no args | `O 114` | Persist selected scene and all 12 profiles |
| `115` | no args | `D 115 1 selected active br1 dyn1 ... br12 dyn12` | Authoritative Nano readback |

After `D 115 1` exactly **26** numbers: selected=0 for F01 or 1..12, active=1 iff Nano mode=AMBIENT and selected P, then 12 brightness/dynamics pairs. Independent versioned+checksummed EEPROM block 29 bytes at 512..540, magic 0xA1D2 version1; old settings untouched.

ESP v2 bridges numeric UART to semantic HTTP, Android never sends raw opcode. Old Nano returning E1 for 115 causes ESP to report presets supported:false. [Nano CI 37818601346](https://github.com/dronrome1245/ARDU/actions/runs/37818601346) compile PASS; physical UART/EEPROM smoke pending.

## D-121 (2026-10-10) — additive warm-Kelvin dawn capability on Nano R5

- Full Nano `nano_warm_dawn_v5/nano_warm_dawn_v5.ino`, banner `ARDU1 6`. `D 3 3` now contains **10 numeric values**: the previous nine (alarmEnabled, hour, minute, fadeMinutes, maxBrightness, startStep, endStep, phase, recovered) plus final `1` capability marker. Old Nano has only 9 values.
- Existing `75 step`, `76 step` are now **warm Kelvin 20K steps 0..110** and are no longer HSV values on Nano R5; `78 startStep endStep` adds **atomic** pair validation `0<=start<=end<=110`, responds `O 78`; `77` persists this pair in Dawn EEPROM version 2 at the unchanged 16..23 byte block. Converting `K` to wire step: `(K-1800)/20`, K must be divisible by 20, 1800..4000.
- Old HSV EEPROM version1 ignored on R5 and replaced with default warm 2000→3600K when loading; alarm enabled/HH:MM and RTC stored independently and preserved.
- When Nano R5 is not present, ESP V3 **must not** send opcode78 and returns `DAWN_KELVIN_REQUIRES_NANO_R5`; Android only enables Kelvin presets when `/api/settings` reports `alarm.dawn_kelvin_supported:true`. No repurposing of old ESP firmware's ambiguous `start_hue` fields into Kelvin.
- Alarm HH:MM is the wake/end time; Nano starts the ramp at modulo-day `wakeMinute - fadeMinutes`, including the previous day. `DAWN STOP` opcode71 and recovery behaviour remain.
