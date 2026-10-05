package com.ardu.app.net

import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.Calendar

class ArduApiClient {
    @Volatile
    private var preferredBaseUrl: String? = null

    @Volatile
    private var selectedBaseUrl: String? = null

    fun setPreferredAddress(address: String?) {
        preferredBaseUrl = normalizeBaseUrl(address)
        selectedBaseUrl = preferredBaseUrl
    }

    fun selectedAddress(): String? = selectedBaseUrl

    fun ping(): PingResponse {
        val candidates = buildList {
            preferredBaseUrl?.let(::add)
            selectedBaseUrl?.let { if (it !in this) add(it) }
            add("http://192.168.4.1")
            add("http://ardu.local")
            add("http://192.168.0.4")
        }.distinct()

        var lastError: Exception? = null

        for (baseUrl in candidates) {
            try {
                val json = getJson(baseUrl, "/api/ping")
                val response = parsePing(json)
                if (response.device == "ARDU-ESP8266" && response.uartProtocol == 1) {
                    selectedBaseUrl = baseUrl
                    return response
                }
                lastError = IOException("Ответ получен не от совместимого ARDU")
            } catch (error: Exception) {
                lastError = error
            }
        }

        throw IOException("ARDU не найден в домашней сети или ARDU-DIRECT", lastError)
    }

    fun status(): DeviceStatus =
        parseStatus(getJson(requireBaseUrl(), "/api/status"))

    fun settings(): ArduSettings =
        parseSettings(getJson(requireBaseUrl(), "/api/settings"))

    fun time(): TimeStatus =
        parseTime(getJson(requireBaseUrl(), "/api/time"))

    fun syncTime(calendar: Calendar = Calendar.getInstance()) {
        postApplied(
            "/api/time/sync",
            JSONObject()
                .put("year", calendar.get(Calendar.YEAR))
                .put("month", calendar.get(Calendar.MONTH) + 1)
                .put("day", calendar.get(Calendar.DAY_OF_MONTH))
                .put("hour", calendar.get(Calendar.HOUR_OF_DAY))
                .put("minute", calendar.get(Calendar.MINUTE))
                .put("second", calendar.get(Calendar.SECOND))
        )
    }

    fun setPower(on: Boolean) {
        postApplied("/api/power", JSONObject().put("on", on))
    }

    fun setMode(mode: String) {
        require(mode in setOf("off", "light", "night", "ambient", "music")) {
            "Неизвестный режим"
        }
        postApplied("/api/mode", JSONObject().put("mode", mode))
    }

    fun lightStatus(): LightStatus {
        val json = getJson(requireBaseUrl(), "/api/light/status")
        requireOk(json)
        val rgb = json.optJSONObject("rgb") ?: throw IOException("BAD_LIGHT_RGB")
        return LightStatus(
            enabled = json.optBoolean("enabled"),
            colorMode = json.requiredString("color_mode"),
            kelvin = json.requiredInt("kelvin", 1800..6500),
            red = rgb.requiredInt("r", 0..255),
            green = rgb.requiredInt("g", 0..255),
            blue = rgb.requiredInt("b", 0..255),
            brightness = json.requiredInt("brightness", 0..255),
            saved = json.optBoolean("saved"),
            dirty = json.optBoolean("dirty"),
            clapEnabled = json.optBoolean("clap_enabled")
        )
    }

    fun setLightEnabled(enabled: Boolean) =
        postApplied("/api/light/settings", JSONObject().put("enabled", enabled))

    fun setLightBrightness(brightness: Int) {
        require(brightness in 0..255)
        postApplied("/api/light/settings", JSONObject().put("brightness", brightness))
    }

    fun setLightKelvin(kelvin: Int) {
        require(kelvin in 1800..6500)
        postApplied("/api/light/settings", JSONObject().put("kelvin", kelvin))
    }

    fun setLightRgb(red: Int, green: Int, blue: Int) {
        require(red in 0..255 && green in 0..255 && blue in 0..255)
        postApplied(
            "/api/light/settings",
            JSONObject().put(
                "rgb",
                JSONObject().put("r", red).put("g", green).put("b", blue)
            )
        )
    }

    fun setClapEnabled(enabled: Boolean) =
        postApplied("/api/light/settings", JSONObject().put("clap_enabled", enabled))

    fun saveLightStartupProfile() =
        postApplied(
            "/api/light/settings",
            JSONObject().put("persist_startup_profile", true)
        )

    fun clapCalibrationStatus(): ClapCalibrationState {
        val json = getJson(requireBaseUrl(), "/api/light/clap/calibration")
        requireOk(json)
        return ClapCalibrationState(
            active = json.optBoolean("active"),
            finished = json.optBoolean("finished"),
            goodPairs = json.optInt("good_pairs"),
            targetPairs = json.optInt("target_pairs"),
            quietP99 = json.optInt("quiet_p99_derivative"),
            suggestedThreshold = json.optInt("suggested_threshold")
        )
    }

    fun startClapCalibration(pairs: Int): ClapCalibrationState {
        require(pairs in 3..12)
        val json = postJson(
            "/api/light/clap/calibration/start",
            JSONObject().put("pairs", pairs),
            readTimeoutMs = 12_000
        )
        requireOk(json)
        return ClapCalibrationState(
            active = true,
            finished = false,
            goodPairs = 0,
            targetPairs = json.optInt("target_pairs", pairs),
            quietP99 = json.optInt("quiet_p99_derivative"),
            suggestedThreshold = 0
        )
    }

    fun captureClapSample(): ClapCalibrationSample {
        val json = postJson(
            "/api/light/clap/calibration/sample",
            JSONObject(),
            readTimeoutMs = 12_000
        )
        requireOk(json)
        return ClapCalibrationSample(
            accepted = json.optBoolean("accepted"),
            goodPairs = json.optNullableInt("good_pairs"),
            targetPairs = json.optNullableInt("target_pairs"),
            detectedClaps = json.optNullableInt("detected_claps"),
            weakStrength = json.optNullableInt("weak_strength"),
            strongStrength = json.optNullableInt("strong_strength")
        )
    }

    fun finishClapCalibration(): ClapCalibrationResult {
        val json = postJson(
            "/api/light/clap/calibration/finish",
            JSONObject(),
            readTimeoutMs = 12_000
        )
        requireOk(json)
        return ClapCalibrationResult(
            quietP99 = json.requiredInt("quiet_p99_derivative", 0..65535),
            clapP20 = json.requiredInt("clap_p20", 0..65535),
            suggestedThreshold = json.requiredInt("suggested_threshold", 0..65535)
        )
    }

    fun saveClapCalibration() {
        val json = postJson("/api/light/clap/calibration/save", JSONObject())
        requireOk(json)
        if (!json.optBoolean("saved")) throw IOException("CLAPCAL_NOT_SAVED")
    }

    fun cancelClapCalibration() {
        val json = postJson("/api/light/clap/calibration/cancel", JSONObject())
        requireOk(json)
        if (!json.optBoolean("cancelled")) throw IOException("CLAPCAL_NOT_CANCELLED")
    }

    fun selectMusicMode(id: String) {
        require(id in MUSIC_IDS)
        postApplied("/api/music/mode", JSONObject().put("id", id))
    }

    fun updateMusicSettings(
        activeBrightness: Int? = null,
        backgroundBrightness: Int? = null,
        smoothing: Int? = null,
        sensitivity: Int? = null,
        submode: String? = null,
        speed: Int? = null,
        rainbowStep10: Int? = null,
        hueStart: Int? = null,
        hueStep: Int? = null,
        persist: Boolean = true
    ) {
        val json = JSONObject()
        activeBrightness?.let { json.put("active_brightness", it) }
        backgroundBrightness?.let { json.put("background_brightness", it) }
        smoothing?.let { json.put("smoothing", it) }
        sensitivity?.let { json.put("sensitivity", it) }
        submode?.let { json.put("submode", it) }
        speed?.let { json.put("speed", it) }
        rainbowStep10?.let { json.put("rainbow_step10", it) }
        hueStart?.let { json.put("hue_start", it) }
        hueStep?.let { json.put("hue_step", it) }
        json.put("persist", persist)
        postApplied("/api/music/settings", json)
    }

    fun calibrateAudio(): AudioCalibration {
        val json = postJson(
            "/api/music/calibrate",
            JSONObject(),
            readTimeoutMs = 12_000
        )
        requireOk(json)
        return AudioCalibration(
            micDc = json.requiredInt("mic_dc", 0..1023),
            vuLowPass = json.requiredInt("vu_low_pass", 0..1023),
            spectrumLowPass = json.requiredInt("spectrum_low_pass", 0..255)
        )
    }

    fun selectAmbientEffect(id: String) {
        require(id in AMBIENT_IDS)
        postApplied("/api/ambient/effect", JSONObject().put("id", id))
    }

    fun updateAmbientSettings(
        autoCycle: Boolean? = null,
        autoPeriodSec: Int? = null,
        hue: Int? = null,
        saturation: Int? = null,
        brightness: Int? = null,
        speed: Int? = null,
        rainbowStep: Double? = null,
        persist: Boolean = true
    ) {
        val json = JSONObject()
        autoCycle?.let { json.put("auto_cycle", it) }
        autoPeriodSec?.let { json.put("auto_period_s", it) }
        hue?.let { json.put("hue", it) }
        saturation?.let { json.put("saturation", it) }
        brightness?.let { json.put("brightness", it) }
        speed?.let { json.put("speed", it) }
        rainbowStep?.let { json.put("rainbow_step", it) }
        json.put("persist", persist)
        postApplied("/api/ambient/settings", json)
    }

    fun updateNightSettings(
        enabled: Boolean? = null,
        hue: Int? = null,
        saturation: Int? = null,
        brightness: Int? = null,
        scheduleEnabled: Boolean? = null,
        scheduleOn: String? = null,
        scheduleOff: String? = null,
        persist: Boolean = true
    ) {
        val json = JSONObject()
        enabled?.let { json.put("enabled", it) }
        hue?.let { json.put("hue", it) }
        saturation?.let { json.put("saturation", it) }
        brightness?.let { json.put("brightness", it) }
        scheduleEnabled?.let { json.put("schedule_enabled", it) }
        scheduleOn?.let { json.put("schedule_on", it) }
        scheduleOff?.let { json.put("schedule_off", it) }
        json.put("persist", persist)
        postApplied("/api/night/settings", json)
    }

    fun updateAlarmSettings(
        enabled: Boolean? = null,
        hour: Int? = null,
        minute: Int? = null,
        fadeMinutes: Int? = null,
        maxBrightness: Int? = null,
        startHue: Int? = null,
        endHue: Int? = null,
        persist: Boolean = true
    ) {
        val json = JSONObject()
        enabled?.let { json.put("enabled", it) }
        hour?.let { json.put("hour", it) }
        minute?.let { json.put("minute", it) }
        fadeMinutes?.let { json.put("fade_minutes", it) }
        maxBrightness?.let { json.put("max_brightness", it) }
        startHue?.let { json.put("start_hue", it) }
        endHue?.let { json.put("end_hue", it) }
        json.put("persist", persist)
        postApplied("/api/alarm/settings", json)
    }

    fun stopDawn() {
        val json = postJson("/api/alarm/stop-dawn", JSONObject())
        requireOk(json)
        if (!json.optBoolean("stopped")) throw IOException("DAWN_NOT_STOPPED")
    }

    fun currentLimit(): CurrentLimit {
        val json = getJson(requireBaseUrl(), "/api/system/current-limit")
        requireOk(json)
        return CurrentLimit(
            milliamps = json.requiredInt("milliamps", 500..4500),
            hardMaxMa = json.requiredInt("hard_max_ma", 500..10000)
        )
    }

    fun setCurrentLimit(milliamps: Int) {
        require(milliamps in 500..4500)
        postApplied(
            "/api/system/current-limit",
            JSONObject().put("milliamps", milliamps)
        )
    }

    fun ringProfile(): RingProfile {
        val json = getJson(requireBaseUrl(), "/api/system/rings")
        requireOk(json)
        return RingProfile(
            normal = json.requiredString("normal"),
            emergency = json.requiredString("emergency"),
            active = json.requiredString("active"),
            powerSource = json.requiredString("power_source")
        )
    }

    fun setRingProfile(normal: String, emergency: String) {
        require(normal in RING_SELECTIONS && emergency in RING_SELECTIONS)
        postApplied(
            "/api/system/rings",
            JSONObject()
                .put("normal", normal)
                .put("emergency", emergency)
        )
    }

    fun events(): List<ArduEvent> {
        val json = getJson(requireBaseUrl(), "/api/events")
        requireOk(json)
        val array = json.optJSONArray("events") ?: JSONArray()
        return buildList {
            for (i in 0 until array.length()) {
                val item = array.optJSONObject(i) ?: continue
                val argsArray = item.optJSONArray("args") ?: JSONArray()
                val args = buildList {
                    for (j in 0 until argsArray.length()) add(argsArray.optInt(j))
                }
                add(
                    ArduEvent(
                        type = item.optString("type", "unknown"),
                        code = item.optInt("code"),
                        args = args,
                        raw = item.optString("raw")
                    )
                )
            }
        }
    }

    fun sendNanoCommand(command: String): NanoResponse {
        val clean = command.trim().replace(Regex("\\s+"), " ")
        require(clean.matches(Regex("\\d+( \\d+){0,6}"))) {
            "Developer UART v1 принимает только numeric opcode и до 6 числовых аргументов"
        }

        val body = request(
            baseUrl = requireBaseUrl(),
            path = "/api/dev/nano",
            method = "POST",
            body = clean,
            contentType = "text/plain; charset=utf-8",
            readTimeoutMs = 12_000
        )
        val json = JSONObject(body)
        if (!json.optBoolean("ok")) {
            throw IOException(json.optString("error", "NANO_REQUEST_FAILED"))
        }
        return NanoResponse(
            command = json.optString("command"),
            nano = json.optString("nano"),
            timedOut = json.optBoolean("timed_out"),
            errorCode = json.optInt("nano_error_code")
        )
    }

    private fun parsePing(json: JSONObject): PingResponse {
        requireOk(json)
        return PingResponse(
            device = json.optString("device", "ARDU-ESP8266"),
            firmware = json.optString("fw", "UNKNOWN"),
            wifiConnected = json.optBoolean("wifi_connected"),
            networkMode = json.optString("network_mode").ifBlank {
                if (json.optBoolean("wifi_connected")) "station" else "unknown"
            },
            softApActive = json.optBoolean("softap_active"),
            ip = json.optString("ip"),
            rssi = if (json.has("rssi")) json.optInt("rssi") else null,
            otaReady = json.optBoolean("ota_ready"),
            uartProtocol = json.optInt("uart_protocol")
        )
    }

    private fun parseStatus(json: JSONObject): DeviceStatus {
        requireOk(json)
        return DeviceStatus(
            nanoFirmware = json.optString("nano_fw", "UNKNOWN"),
            uartProtocol = json.optInt("uart_protocol"),
            mode = json.requiredString("mode"),
            modeId = json.optInt("mode_id", -1),
            rtcValid = json.optBoolean("rtc_valid"),
            rtcTime = json.optString("rtc_time").ifBlank { null },
            clapEnabled = json.optBoolean("clap_enabled"),
            clapThreshold = json.optInt("clap_threshold"),
            clapTimeoutMs = json.optInt("clap_timeout_ms"),
            currentLimitMa = json.optInt("current_limit_ma"),
            musicMode = json.optString("music_mode"),
            ambientEffect = json.optString("ambient_effect"),
            resetFlags = json.optInt("reset_flags"),
            uptimeMs = json.optLong("uptime_ms"),
            nanoRaw = json.optString("nano")
        )
    }

    private fun parseTime(json: JSONObject): TimeStatus {
        requireOk(json)
        return TimeStatus(
            valid = json.optBoolean("valid"),
            date = json.optString("date").ifBlank { null },
            time = json.optString("time").ifBlank { null },
            nanoRaw = json.optString("nano")
        )
    }

    private fun parseSettings(json: JSONObject): ArduSettings {
        requireOk(json)

        val light = json.requiredObject("light")
        val rgb = light.requiredObject("rgb")
        val clap = json.requiredObject("clap")
        val calibration = clap.requiredObject("calibration")
        val night = json.requiredObject("night")
        val alarm = json.requiredObject("alarm")
        val ambient = json.requiredObject("ambient")
        val music = json.requiredObject("music")
        val musicModes = music.requiredObject("modes")
        val system = json.requiredObject("system")

        val effects = mapOf(
            "F01" to parseAmbientEffect(ambient.requiredObject("F01"), false, false),
            "F02" to parseAmbientEffect(ambient.requiredObject("F02"), true, false),
            "F03" to parseAmbientEffect(ambient.requiredObject("F03"), true, true)
        )

        val modes = MUSIC_IDS.associateWith { id ->
            val item = musicModes.requiredObject(id)
            MusicModeSettings(
                brightness = item.requiredInt("brightness", 0..255),
                backgroundBrightness = item.requiredInt("background_brightness", 0..255),
                smoothing = item.requiredInt("smoothing", 0..255),
                sensitivity = item.requiredInt("sensitivity", 0..255),
                speed = item.requiredInt("speed", 0..255),
                aux = item.requiredInt("aux", 0..255),
                submode = item.requiredInt("submode", 0..3)
            )
        }

        return ArduSettings(
            schema = json.optInt("schema"),
            mode = json.requiredString("mode"),
            light = LightSettings(
                colorMode = light.requiredString("color_mode"),
                kelvin = light.requiredInt("kelvin", 1800..6500),
                red = rgb.requiredInt("r", 0..255),
                green = rgb.requiredInt("g", 0..255),
                blue = rgb.requiredInt("b", 0..255),
                brightness = light.requiredInt("brightness", 0..255),
                saved = light.optBoolean("saved"),
                dirty = light.optBoolean("dirty")
            ),
            clap = ClapSettings(
                enabled = clap.optBoolean("enabled"),
                threshold = clap.requiredInt("threshold", 0..65535),
                timeoutMs = clap.requiredInt("timeout_ms", 0..65535),
                saved = clap.optBoolean("saved"),
                calibration = ClapCalibrationState(
                    active = calibration.optBoolean("active"),
                    finished = calibration.optBoolean("finished"),
                    goodPairs = calibration.optInt("good_pairs"),
                    targetPairs = calibration.optInt("target_pairs"),
                    quietP99 = calibration.optInt("quiet_p99_derivative"),
                    suggestedThreshold = calibration.optInt("suggested_threshold")
                )
            ),
            night = NightSettings(
                enabled = night.optBoolean("enabled"),
                hue = night.requiredInt("hue", 0..255),
                saturation = night.requiredInt("saturation", 0..255),
                brightness = night.requiredInt("brightness", 0..255),
                scheduleEnabled = night.optBoolean("schedule_enabled"),
                scheduleOn = night.requiredString("schedule_on"),
                scheduleOff = night.requiredString("schedule_off")
            ),
            alarm = AlarmSettings(
                enabled = alarm.optBoolean("enabled"),
                hour = alarm.requiredInt("hour", 0..23),
                minute = alarm.requiredInt("minute", 0..59),
                fadeMinutes = alarm.requiredInt("fade_minutes", 1..120),
                maxBrightness = alarm.requiredInt("max_brightness", 1..255),
                startHue = alarm.requiredInt("start_hue", 0..255),
                endHue = alarm.requiredInt("end_hue", 0..255),
                dawnPhase = alarm.requiredString("dawn_phase"),
                recovered = alarm.optBoolean("recovered")
            ),
            ambient = AmbientSettings(
                effect = ambient.requiredString("effect"),
                autoCycle = ambient.optBoolean("auto_cycle"),
                autoPeriodSec = ambient.requiredInt("auto_period_s", 1..255),
                effects = effects
            ),
            music = MusicSettings(
                selected = music.requiredString("selected"),
                modes = modes
            ),
            system = SystemSettings(
                currentLimitMa = system.requiredInt("current_limit_ma", 500..4500),
                audioCalibrated = system.optBoolean("audio_calibrated"),
                micDc = system.requiredInt("mic_dc", 0..1023),
                vuLowPass = system.requiredInt("vu_low_pass", 0..1023),
                spectrumLowPass = system.requiredInt("spectrum_low_pass", 0..255)
            )
        )
    }

    private fun parseAmbientEffect(
        item: JSONObject,
        hasSpeed: Boolean,
        hasRainbowStep: Boolean
    ): AmbientEffectSettings = AmbientEffectSettings(
        hue = item.requiredInt("hue", 0..255),
        saturation = if (item.has("saturation")) item.requiredInt("saturation", 0..255) else null,
        brightness = item.requiredInt("brightness", 0..255),
        speed = if (hasSpeed) item.requiredInt("speed", 0..255) else null,
        rainbowStep = if (hasRainbowStep) item.optDouble("rainbow_step") else null
    )

    private fun postApplied(path: String, payload: JSONObject) {
        val json = postJson(path, payload)
        requireOk(json)
        if (!json.optBoolean("applied")) throw IOException("NOT_APPLIED")
    }

    private fun postJson(
        path: String,
        payload: JSONObject,
        readTimeoutMs: Int = 5_000
    ): JSONObject = JSONObject(
        request(
            baseUrl = requireBaseUrl(),
            path = path,
            method = "POST",
            body = payload.toString(),
            readTimeoutMs = readTimeoutMs
        )
    )

    private fun getJson(baseUrl: String, path: String): JSONObject =
        JSONObject(request(baseUrl, path, "GET", null))

    private fun requireOk(json: JSONObject) {
        if (!json.optBoolean("ok")) {
            throw IOException(json.optString("error", "ARDU_REQUEST_FAILED"))
        }
    }

    private fun requireBaseUrl(): String =
        selectedBaseUrl ?: throw IOException("Сначала подключитесь к ARDU")

    private fun normalizeBaseUrl(address: String?): String? {
        val clean = address?.trim()?.trimEnd('/')?.takeIf { it.isNotBlank() } ?: return null
        return if (clean.startsWith("http://") || clean.startsWith("https://")) {
            clean
        } else {
            "http://$clean"
        }
    }

    private fun request(
        baseUrl: String,
        path: String,
        method: String,
        body: String?,
        contentType: String = "application/json; charset=utf-8",
        readTimeoutMs: Int = 5_000
    ): String {
        val connection = URL(baseUrl + path).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = method
            connection.connectTimeout = 1_800
            connection.readTimeout = readTimeoutMs
            connection.useCaches = false
            connection.setRequestProperty("Accept", "application/json")

            if (body != null) {
                val bytes = body.toByteArray(StandardCharsets.UTF_8)
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", contentType)
                connection.setFixedLengthStreamingMode(bytes.size)
                connection.outputStream.use { it.write(bytes) }
            }

            val code = connection.responseCode
            val stream = if (code in 200..299) connection.inputStream else connection.errorStream
            val responseBody = stream?.bufferedReader(StandardCharsets.UTF_8)
                ?.use { it.readText() }
                .orEmpty()

            if (code !in 200..299) {
                val error = runCatching {
                    JSONObject(responseBody).optString("error")
                }.getOrNull().orEmpty().ifBlank { "HTTP $code" }
                throw IOException(error)
            }

            return responseBody
        } finally {
            connection.disconnect()
        }
    }

    private fun JSONObject.requiredObject(key: String): JSONObject =
        optJSONObject(key) ?: throw IOException("BAD_$key")

    private fun JSONObject.requiredString(key: String): String =
        optString(key).takeIf { it.isNotBlank() } ?: throw IOException("BAD_$key")

    private fun JSONObject.requiredInt(key: String, range: IntRange): Int {
        if (!has(key)) throw IOException("BAD_$key")
        val value = optInt(key, Int.MIN_VALUE)
        if (value !in range) throw IOException("BAD_$key")
        return value
    }

    private fun JSONObject.optNullableInt(key: String): Int? =
        if (has(key) && !isNull(key)) optInt(key) else null

    companion object {
        val MUSIC_IDS = listOf("M01", "M02", "M03", "M04", "M05", "M08", "M09")
        val AMBIENT_IDS = listOf("F01", "F02", "F03")
        val RING_SELECTIONS = setOf("both", "a", "b")
    }
}
