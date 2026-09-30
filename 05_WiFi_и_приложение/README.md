# Wi‑Fi и мобильное приложение ARDU

Читать в порядке:

1. `ФУНКЦИОНАЛЬНАЯ_МОДЕЛЬ.md` — что должен уметь новый клиент;
2. `МОДЕЛЬ_ЭКРАНОВ.md` — структура интерфейса;
3. `API_V1.md` — предварительный HTTP-контракт;
4. `../06_Интерфейс_управления/ПРОТОКОЛ_КОМАНД.md` — разделение внешнего HTTP и внутреннего UART;
5. `../08_Исходники_и_референсы/Ardublock2_РАЗБОР.md` — почему принято именно так.

Старое приложение LumpV3.1 не является реализационным шаблоном. Оно используется как источник функций и истории поведения.

## Android app R1

- исходники: `android_app_r1/`;
- transport test: `ANDROID_APP_R1_TEST.md`;
- первый gate: `/api/ping` → `/api/status` → `/api/time` → Developer raw `PING`.


## ESP8266 HTTP BRIDGE R2 / L01 stage 1

- код: `esp8266_http_bridge_r2/esp8266_http_bridge_r2.ino`;
- тест: `ESP8266_HTTP_BRIDGE_R2_TEST.md`;
- новый semantic API: `GET /api/light/status`, `POST /api/light/settings` (пока только `enabled`);
- R1 diagnostic endpoints сохранены;
- R2 включает ранее добавленный startup UART framing fix.

## ESP8266 HTTP BRIDGE R4 / OTA

- код: `esp8266_http_bridge_r4/esp8266_http_bridge_r4.ino`;
- тест: `ESP8266_HTTP_BRIDGE_R4_OTA_TEST.md`;
- Wi-Fi SSID/password и OTA password вводятся вручную прямо в локальную копию скетча перед первой wired прошивкой;
- после первого wired upload R4 последующие ESP updates выполняются ArduinoOTA по Wi-Fi;
- OTA hostname = `ardu`, port = `8266`;
- OTA progress намеренно не печатается в ESP Serial, потому что Serial используется как transport к Nano.

## ESP8266 HTTP BRIDGE R6 / L01 brightness + Kelvin

- код: `esp8266_http_bridge_r6/esp8266_http_bridge_r6.ino`;
- OTA update поверх hardware-tested R5;
- сохраняет R5 Wi-Fi/HTTP/UART/OTA;
- `POST /api/light/settings` дополнительно принимает atomic `brightness` 0..255;
- Kelvin slice принимает presets 2700/4000/6000;
- Android `0.4-l01-bright-kelvin-r1` добавляет brightness slider и три preset buttons;
- RGB, произвольный Kelvin slider и persistence остаются следующими слоями.
