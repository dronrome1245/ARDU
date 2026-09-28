# ARDU — компактная рабочая схема FW-10 CORE R1 + ESP8266 HTTP R1

Дата: 2026-09-28

Это каноническая стендовая схема **обычной работы**, не схема прошивки ESP8266.

## 1. Общая земля

Все GND соединены вместе:

- S-30-5 V-
- Nano GND
- YP-8 GND
- ESP8266 GND
- MAX9814 GND
- DS3231 GND
- WS2812B GND

## 2. Питание 5 В

S-30-5 V+ (5 В) идет параллельно на:

- Nano 5V
- YP-8 VIN
- MAX9814 Vdd
- DS3231 VCC
- WS2812B +5V

S-30-5 V- идет на общую GND.

## 3. ESP8266 питание

YP-8:
- VIN -> +5V
- GND -> common GND
- VOUT -> ESP VCC (3.3 В)

Непосредственно возле ESP:
- 100 µF / 10 V между ESP VCC и GND
- плюс электролита -> ESP VCC
- минус/полоса -> ESP GND

Белая adapter board проекта уже имеет CH_PD pull-up и GPIO15 pull-down.

Для надежного normal boot:
- ESP GPIO0 -> 4.7 kΩ -> 3.3 V
- ESP RST -> 4.7 kΩ -> 3.3 V

GPIO0 НЕ соединять с GND в рабочем режиме.

## 4. Рабочий UART ESP <-> Nano

ESP TX0 / GPIO1 -> Nano D0 / RX напрямую.

Nano D1 / TX -> 1.5 kΩ -> UART_NODE -> ESP RX0 / GPIO3.

UART_NODE -> 1.5 kΩ -> 1.5 kΩ -> GND.

То есть нижнее плечо = 3.0 kΩ.

При Nano TX HIGH около 5 В узел должен быть около 3.3 В.

Общий GND обязателен.

При штатной работе не использовать временное прямое Nano D1/TX -> ESP RX без делителя.

## 5. MAX9814

Конкретный модуль: AR | Out | Gain | Vdd | GND.

- Vdd -> +5V
- GND -> common GND
- Out -> Nano A0 напрямую
- Gain -> Vdd, режим 40 dB
- AR -> оставить неподключенным (floating)

Не ставить 1 µF/10 nF/4.7 kΩ в текущий A0 тракт: эти детали относятся к отмененной A2/A3 ветви.

## 6. DS3231

- VCC -> +5V
- GND -> common GND
- SDA -> Nano A4
- SCL -> Nano A5

Резистор 201 зарядной цепи уже удален; CR2032 остается установленной.

## 7. WS2812B ring A — текущий CORE R1

- Nano D6 -> 220 Ω -> DIN первого WS2812B
- +5V -> ring +5V
- GND -> ring GND
- 1000 µF / 6.3 V между +5V и GND возле входа кольца
  - плюс -> +5V
  - минус/полоса -> GND

Текущая геометрия = 44 LED.

DOUT последнего LED не соединять обратно с DIN первого.

FW-10 CORE R1 держит D7 LOW:
- второе кольцо к D7 сейчас НЕ подключать.

Для будущего второго кольца:
- Nano D7 -> отдельный 220 Ω -> DIN второго кольца
- отдельный 1000 µF возле питания второго кольца предпочтителен
- питание второго кольца отдельной ветвью +5V/GND от S-30-5.

## 8. Nano

- 5V -> S-30-5 +5V
- GND -> common GND
- D0/RX <- ESP TX GPIO1
- D1/TX -> divider -> ESP RX GPIO3
- D6 -> 220 Ω -> ring A DIN
- D7 -> currently no ring, FW drives LOW
- A0 <- MAX9814 Out
- A4 <-> DS3231 SDA
- A5 <-> DS3231 SCL

Nano RESET в рабочем режиме НИ С ЧЕМ не соединять с GND.

## 9. Что не подключать в рабочем режиме

- ESP GPIO0 -> GND: только для прошивки ESP, в работе запрещено
- Nano RESET -> GND: только когда Nano используется как USB-UART bridge для прошивки ESP
- прямой 5 V Nano TX -> ESP RX: не оставлять
- ESP VCC -> 5 V: запрещено, конкретная adapter board проекта требует 3.3 V
- A2/A3 audio chain: не используется
- D7 second ring: пока не используется в FW-10 CORE R1

## 10. Контрольные напряжения

Система уже измерена владельцем:
- S-30-5 output: 5.01 V
- YP-8 input: 5.01 V
- YP-8 output: 3.32 V
- ESP VCC: 3.24 V
- ground drop YP-8 GND -> ESP GND: 0.00 V
- GPIO0 LOW during flash: 0.06 V
- ESP RST idle: 3.25 V

После компактной пересборки проверить снова:
- +5V bus
- YP-8 VOUT
- ESP VCC
- Nano D1/TX idle
- UART_NODE / ESP RX idle
- ESP TX idle
- Nano D0/RX idle
