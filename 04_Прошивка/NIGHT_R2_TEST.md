# ARDU — NIGHT R2 schedule test

Дата: 2026-09-26

## Scope

NIGHT R2 replaces the not-yet-hardware-tested NIGHT R1 and includes all of its functions plus an RTC schedule.

Behavior:

- schedule is OFF by default;
- default stored schedule: 22:00 -> 07:00;
- schedule supports crossing midnight;
- interval semantics are [ON, OFF): ON time is included, OFF time is excluded;
- when schedule is ON, effective POWER is derived from DS3231;
- manual ON/OFF disables schedule and returns to manual control;
- HUE/SAT/BRIGHT do not disable schedule;
- if schedule is enabled but RTC is invalid, fail-safe effective POWER is OFF.

## First runtime

After Verify/Upload:

```
STATUS
TIME
```

Expected:

- FW=NIGHT REV=2;
- RTC_PRESENT=YES;
- RTC_VALID=YES;
- defaults HUE=24 SAT=180 BRIGHTNESS=18;
- on a clean R2 EEPROM: SCHEDULE=OFF, SCHED_ON=22:00, SCHED_OFF=07:00.

## Visual/manual regression

```
ON
HUE 24
SAT 180
BRIGHT 18
STATUS
```

Then:

```
HUE 0
HUE 96
HUE 24
SAT 255
SAT 80
SAT 180
BRIGHT 2
BRIGHT 40
BRIGHT 18
```

All changes should be immediate.

## Schedule test without waiting overnight

Use TIME and choose a two-minute window around the current RTC time.

Example: if TIME is 21:50, run:

```
OFF
SCHEDULESET 21:51 21:53
SCHEDULE ON
STATUS
```

OFF first establishes MANUAL_POWER=OFF. SCHEDULE ON must not change that saved manual preference.

Expected:

- before 21:51: POWER=OFF, WINDOW=INACTIVE;
- at 21:51: one event `EVENT NIGHT_SCHEDULE POWER=ON`, ring turns on;
- at 21:53: one event `EVENT NIGHT_SCHEDULE POWER=OFF`, ring turns off.

During the active window send STATUS several times; no Serial corruption is expected.

## Persistence

While schedule is ON, press Reset Nano.

After startup:

- SCHEDULE=ON;
- schedule times are preserved;
- POWER immediately matches the RTC window;
- no manual command is required to restore state.

## Cross-midnight logic

No overnight waiting is required for the first hardware pass. Static logic supports e.g.:

```
SCHEDULESET 22:00 07:00
```

which means active when current minute is >=22:00 OR <07:00.

## Manual override rule

With SCHEDULE=ON, run:

```
ON
STATUS
```

Expected:

- SCHEDULE=OFF;
- CONTROL=MANUAL;
- MANUAL_POWER=ON;
- POWER=ON.

Then:

```
OFF
```

Expected manual OFF.

## Restore desired normal configuration

For a typical night schedule:

```
SCHEDULESET 22:00 07:00
SCHEDULE ON
```

If you do not want automatic operation yet, leave `SCHEDULE OFF`.
