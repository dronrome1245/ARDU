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
        // Temporary compatibility adapter for the already-installed
        // HTTP_BRIDGE_R1. The ordinary UI stays semantic, while this client
        // translates it to the proven raw Nano transport internally.
        val systemLine = status().nano
        val lightLine = sendNanoCommand("LIGHT STATUS").nano

        val systemMode = statusToken(systemLine, "MODE=")
        val colorMode = statusToken(lightLine, "MODE=")
        val kelvin = statusToken(lightLine, "KELVIN=").toIntOrNull()
            ?: throw IOException("BAD_LIGHT_KELVIN")
        val brightness = statusToken(lightLine, "BRIGHTNESS=").toIntOrNull()
            ?: throw IOException("BAD_LIGHT_BRIGHTNESS")
        val rgb = statusToken(lightLine, "RGB=")
            .split(',')
            .map { it.toIntOrNull() }

        if (colorMode != "KELVIN" && colorMode != "RGB") {
            throw IOException("BAD_LIGHT_COLOR_MODE")
        }

        if (rgb.size != 3 || rgb.any { it == null }) {
            throw IOException("BAD_LIGHT_RGB")
        }

        val saved = statusToken(lightLine, "SAVED=")
        val dirty = statusToken(lightLine, "DIRTY=")

        return LightStatus(
            enabled = systemMode == "L01",
            colorMode = colorMode.lowercase(),
            kelvin = kelvin,
            red = rgb[0]!!,
            green = rgb[1]!!,
            blue = rgb[2]!!,
            brightness = brightness,
            saved = saved == "YES",
            dirty = dirty == "YES"
        )
    }

    fun setLightEnabled(enabled: Boolean) {
        val command = if (enabled) "LIGHT ON" else "LIGHT OFF"
        val expected = if (enabled) "OK LIGHT=ON" else "OK LIGHT=OFF"
        val response = sendNanoCommand(command)

        if (response.nano != expected) {
            throw IOException("UNEXPECTED_NANO_ACK: " + response.nano)
        }
    }

    private fun statusToken(line: String, key: String): String {
        val keyPos = line.indexOf(key)
        if (keyPos < 0) {
            throw IOException("STATUS_FIELD_MISSING: " + key)
        }

        val start = keyPos + key.length
        val end = line.indexOf(' ', start).let {
            if (it < 0) line.length else it
        }

        return line.substring(start, end)
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
