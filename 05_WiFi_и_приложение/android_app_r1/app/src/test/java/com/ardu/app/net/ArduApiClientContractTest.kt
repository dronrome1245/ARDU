package com.ardu.app.net

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.net.InetSocketAddress
import java.nio.charset.StandardCharsets
import java.util.concurrent.ConcurrentHashMap

class ArduApiClientContractTest {
    private lateinit var server: HttpServer
    private lateinit var api: ArduApiClient
    private val received = ConcurrentHashMap<String, String>()

    @Before
    fun setUp() {
        server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)

        json("/api/ping", PING)
        json("/api/status", STATUS)
        json("/api/settings", SETTINGS)
        json("/api/time", TIME)

        applied("/api/mode")
        applied("/api/ambient/effect")
        applied("/api/night/settings")
        applied("/api/alarm/settings")
        applied("/api/light/settings")
        applied("/api/music/settings")
        applied("/api/system/current-limit")

        server.createContext("/api/dev/nano") { exchange ->
            received[exchange.requestURI.path] = readBody(exchange)
            respond(
                exchange,
                """{"ok":true,"command":"1","nano":"O 1","timed_out":false,"nano_error_code":0}"""
            )
        }

        server.start()

        api = ArduApiClient()
        api.setPreferredAddress("http://127.0.0.1:" + server.address.port)
        api.ping()
    }

    @After
    fun tearDown() {
        server.stop(0)
    }

    @Test
    fun parsesFinalReleaseSnapshot() {
        val ping = api.ping()
        val status = api.status()
        val settings = api.settings()
        val time = api.time()

        assertEquals("ARDU_ESP_V1", ping.firmware)
        assertEquals(1, ping.uartProtocol)
        assertEquals("music", status.mode)
        assertEquals("M08", status.musicMode)
        assertTrue(status.rtcValid)

        assertEquals(1, settings.schema)
        assertEquals(2700, settings.light.kelvin)
        assertEquals(70, settings.clap.threshold)
        assertEquals("22:00", settings.night.scheduleOn)
        assertEquals(7, settings.alarm.hour)
        assertEquals("F03", settings.ambient.effect)
        assertEquals(0.5, settings.ambient.effects.getValue("F03").rainbowStep!!, 0.001)
        assertEquals(128, settings.music.modes.getValue("M01").brightness)
        assertEquals(3000, settings.system.currentLimitMa)

        assertTrue(time.valid)
        assertEquals("2026-10-02", time.date)
        assertEquals("20:00:01", time.time)
    }

    @Test
    fun sendsSemanticWritesAndNumericDeveloperCommand() {
        api.setMode("music")
        api.selectAmbientEffect("F02")
        api.updateNightSettings(
            hue = 24,
            saturation = 180,
            brightness = 18,
            persist = true
        )
        api.updateAlarmSettings(hour = 7, minute = 15, fadeMinutes = 30)
        api.setLightBrightness(64)
        api.updateMusicSettings(activeBrightness = 120, persist = true)
        api.setCurrentLimit(3000)

        val raw = api.sendNanoCommand("1")

        assertTrue(received.getValue("/api/mode").contains(""mode":"music""))
        assertTrue(received.getValue("/api/ambient/effect").contains(""id":"F02""))
        assertTrue(received.getValue("/api/night/settings").contains(""hue":24"))
        assertTrue(received.getValue("/api/alarm/settings").contains(""hour":7"))
        assertTrue(received.getValue("/api/light/settings").contains(""brightness":64"))
        assertTrue(received.getValue("/api/music/settings").contains(""active_brightness":120"))
        assertTrue(received.getValue("/api/system/current-limit").contains(""milliamps":3000"))
        assertEquals("O 1", raw.nano)
        assertEquals("1", received.getValue("/api/dev/nano"))
    }

    private fun json(path: String, body: String) {
        server.createContext(path) { exchange ->
            respond(exchange, body)
        }
    }

    private fun applied(path: String) {
        server.createContext(path) { exchange ->
            received[path] = readBody(exchange)
            respond(exchange, """{"ok":true,"applied":true}""")
        }
    }

    private fun readBody(exchange: HttpExchange): String =
        exchange.requestBody.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }

    private fun respond(exchange: HttpExchange, body: String) {
        val bytes = body.toByteArray(StandardCharsets.UTF_8)
        exchange.responseHeaders.add("Content-Type", "application/json; charset=utf-8")
        exchange.sendResponseHeaders(200, bytes.size.toLong())
        exchange.responseBody.use { it.write(bytes) }
    }

    companion object {
        private const val PING =
            """{"ok":true,"device":"ARDU-ESP8266","fw":"ARDU_ESP_V1","uart_protocol":1,"wifi_connected":true,"ip":"192.168.0.4","rssi":-57,"ota_ready":true,"ota_hostname":"ardu","ota_port":8266}"""

        private const val STATUS =
            """{"ok":true,"nano_fw":"ARDU_V1","uart_protocol":1,"mode":"music","mode_id":5,"rtc_valid":true,"clap_enabled":true,"clap_threshold":70,"clap_timeout_ms":500,"current_limit_ma":3000,"music_mode":"M08","ambient_effect":"F03","reset_flags":0,"uptime_ms":123456,"rtc_time":"2026-10-02 20:00:01","nano":"D 2 5 1 1 70 500 3000 5 2 0 123456"}"""

        private const val TIME =
            """{"ok":true,"valid":true,"date":"2026-10-02","time":"20:00:01","nano":"D 4 1 2026 10 2 20 0 1"}"""

        private const val SETTINGS = """
        {
          "ok":true,
          "schema":1,
          "mode":"music",
          "light":{
            "color_mode":"kelvin",
            "kelvin":2700,
            "rgb":{"r":255,"g":170,"b":87},
            "brightness":64,
            "saved":true,
            "dirty":false
          },
          "clap":{
            "enabled":true,
            "threshold":70,
            "timeout_ms":500,
            "saved":true,
            "calibration":{
              "active":false,
              "finished":false,
              "good_pairs":0,
              "target_pairs":0,
              "quiet_p99_derivative":0,
              "suggested_threshold":0
            }
          },
          "night":{
            "enabled":false,
            "hue":24,
            "saturation":180,
            "brightness":18,
            "schedule_enabled":true,
            "schedule_on":"22:00",
            "schedule_off":"07:00"
          },
          "alarm":{
            "enabled":true,
            "hour":7,
            "minute":0,
            "fade_minutes":30,
            "max_brightness":120,
            "start_hue":8,
            "end_hue":32,
            "dawn_phase":"idle",
            "recovered":false
          },
          "ambient":{
            "effect":"F03",
            "auto_cycle":false,
            "auto_period_s":10,
            "F01":{"hue":32,"saturation":255,"brightness":80},
            "F02":{"hue":40,"saturation":220,"brightness":75,"speed":30},
            "F03":{"hue":0,"brightness":90,"speed":25,"rainbow_step":0.5}
          },
          "music":{
            "selected":"M08",
            "modes":{
              "M01":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":0},
              "M02":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":0},
              "M03":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":0},
              "M04":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":0},
              "M05":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":1},
              "M08":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":30,"aux":10,"submode":2},
              "M09":{"brightness":128,"background_brightness":20,"smoothing":40,"sensitivity":100,"speed":8,"aux":6,"submode":0}
            }
          },
          "system":{
            "current_limit_ma":3000,
            "audio_calibrated":true,
            "mic_dc":234,
            "vu_low_pass":280,
            "spectrum_low_pass":36
          }
        }
        """
    }
}
