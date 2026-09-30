package com.ardu.app.net

import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets

data class PingResponse(
    val device: String,
    val firmware: String,
    val wifiConnected: Boolean
)

data class NanoResponse(
    val command: String,
    val nano: String,
    val timedOut: Boolean
)

data class LightStatus(
    val enabled: Boolean,
    val colorMode: String,
    val kelvin: Int,
    val red: Int,
    val green: Int,
    val blue: Int,
    val brightness: Int,
    val saved: Boolean,
    val dirty: Boolean
)

class ArduApiClient {
    private val candidates = listOf(
        "http://ardu.local",
        "http://192.168.0.4"
    )

    @Volatile
    private var selectedBaseUrl: String? = null

    fun ping(): PingResponse {
        val orderedCandidates = buildList {
            selectedBaseUrl?.let(::add)
            candidates.forEach { if (it !in this) add(it) }
        }

        var lastError: Exception? = null

        for (baseUrl in orderedCandidates) {
            try {
                val body = request(baseUrl, "/api/ping", "GET", null)
                val response = parsePing(body)
                if (response.wifiConnected) {
                    selectedBaseUrl = baseUrl
                    return response
                }
                lastError = IOException("ARDU Wi-Fi не подключён")
            } catch (error: Exception) {
                lastError = error
            }
        }

        throw IOException(
            "ARDU не найден в локальной сети",
            lastError
        )
    }

    fun status(): NanoResponse =
        getNanoResponse("/api/status")

    fun time(): NanoResponse =
        getNanoResponse("/api/time")

    fun lightStatus(): LightStatus {
        val baseUrl = requireBaseUrl()
        val body = request(
            baseUrl = baseUrl,
            path = "/api/light/status",
            method = "GET",
            body = null
        )

        val json = JSONObject(body)
        if (!json.optBoolean("ok", false)) {
            throw IOException(json.optString("error", "LIGHT_STATUS_FAILED"))
        }

        val rgb = json.optJSONObject("rgb")
            ?: throw IOException("BAD_LIGHT_RGB")

        val colorMode = json.optString("color_mode")
        if (colorMode != "kelvin" && colorMode != "rgb") {
            throw IOException("BAD_LIGHT_COLOR_MODE")
        }

        return LightStatus(
            enabled = json.optBoolean("enabled", false),
            colorMode = colorMode,
            kelvin = json.optInt("kelvin", -1).also {
                if (it !in 1800..6500) throw IOException("BAD_LIGHT_KELVIN")
            },
            red = rgb.optInt("r", -1).also {
                if (it !in 0..255) throw IOException("BAD_LIGHT_RGB")
            },
            green = rgb.optInt("g", -1).also {
                if (it !in 0..255) throw IOException("BAD_LIGHT_RGB")
            },
            blue = rgb.optInt("b", -1).also {
                if (it !in 0..255) throw IOException("BAD_LIGHT_RGB")
            },
            brightness = json.optInt("brightness", -1).also {
                if (it !in 0..255) throw IOException("BAD_LIGHT_BRIGHTNESS")
            },
            saved = json.optBoolean("saved", false),
            dirty = json.optBoolean("dirty", false)
        )
    }

    fun setLightEnabled(enabled: Boolean) {
        val json = postLightSettings(
            JSONObject().put("enabled", enabled)
        )

        if (json.optBoolean("enabled", !enabled) != enabled) {
            throw IOException("LIGHT_ENABLED_NOT_APPLIED")
        }
    }

    fun setLightBrightness(brightness: Int) {
        require(brightness in 0..255) {
            "Яркость должна быть в диапазоне 0..255"
        }

        val json = postLightSettings(
            JSONObject().put("brightness", brightness)
        )

        if (json.optInt("brightness", -1) != brightness) {
            throw IOException("LIGHT_BRIGHTNESS_NOT_APPLIED")
        }
    }

    fun setLightKelvinPreset(kelvin: Int) {
        require(kelvin == 2700 || kelvin == 4000 || kelvin == 6000) {
            "Неподдерживаемый Kelvin preset"
        }

        val json = postLightSettings(
            JSONObject().put("kelvin", kelvin)
        )

        if (json.optInt("kelvin", -1) != kelvin ||
            json.optString("color_mode") != "kelvin") {
            throw IOException("LIGHT_KELVIN_NOT_APPLIED")
        }
    }

    private fun postLightSettings(payload: JSONObject): JSONObject {
        val baseUrl = requireBaseUrl()
        val body = request(
            baseUrl = baseUrl,
            path = "/api/light/settings",
            method = "POST",
            body = payload.toString()
        )

        val json = JSONObject(body)
        if (!json.optBoolean("ok", false)) {
            throw IOException(json.optString("error", "LIGHT_SETTINGS_FAILED"))
        }

        if (!json.optBoolean("applied", false)) {
            throw IOException("LIGHT_SETTINGS_NOT_APPLIED")
        }

        return json
    }

    fun sendNanoCommand(command: String): NanoResponse {
        val clean = command.trim()

        require(clean.isNotEmpty()) { "Команда пуста" }
        require(clean.none { it == '\n' || it == '\r' || it.code < 0x20 }) {
            "Разрешена одна текстовая команда без управляющих символов"
        }

        val baseUrl = requireBaseUrl()
        val body = request(
            baseUrl = baseUrl,
            path = "/api/dev/nano",
            method = "POST",
            body = clean,
            contentType = "text/plain; charset=utf-8"
        )
        return parseNano(body)
    }

    private fun getNanoResponse(path: String): NanoResponse {
        val baseUrl = requireBaseUrl()
        val body = request(baseUrl, path, "GET", null)
        return parseNano(body)
    }

    private fun requireBaseUrl(): String =
        selectedBaseUrl ?: throw IOException(
            "Сначала требуется успешный /api/ping"
        )

    private fun parsePing(body: String): PingResponse {
        val json = JSONObject(body)
        if (!json.optBoolean("ok", false)) {
            throw IOException(json.optString("error", "PING_FAILED"))
        }

        return PingResponse(
            device = json.optString("device", "ARDU"),
            firmware = json.optString("fw", "UNKNOWN"),
            wifiConnected = json.optBoolean("wifi_connected", false)
        )
    }

    private fun parseNano(body: String): NanoResponse {
        val json = JSONObject(body)
        if (!json.optBoolean("ok", false)) {
            val error = json.optString("error")
                .ifBlank { json.optString("nano", "NANO_REQUEST_FAILED") }
            throw IOException(error)
        }

        return NanoResponse(
            command = json.optString("command"),
            nano = json.optString("nano"),
            timedOut = json.optBoolean("timed_out", false)
        )
    }

    private fun request(
        baseUrl: String,
        path: String,
        method: String,
        body: String?,
        contentType: String = "application/json; charset=utf-8"
    ): String {
        val connection = URL(baseUrl + path).openConnection() as HttpURLConnection

        try {
            connection.requestMethod = method
            connection.connectTimeout = 1800
            connection.readTimeout = 3000
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
            val stream = if (code in 200..299) {
                connection.inputStream
            } else {
                connection.errorStream
            }

            val responseBody = stream?.bufferedReader(StandardCharsets.UTF_8)
                ?.use { it.readText() }
                .orEmpty()

            if (code !in 200..299) {
                val message = runCatching {
                    JSONObject(responseBody).optString("error")
                }.getOrNull().orEmpty().ifBlank { "HTTP $code" }
                throw IOException(message)
            }

            return responseBody
        } finally {
            connection.disconnect()
        }
    }
}
