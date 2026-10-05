package com.ardu.app.net

data class PingResponse(
    val device: String,
    val firmware: String,
    val wifiConnected: Boolean,
    val networkMode: String,
    val softApActive: Boolean,
    val ip: String,
    val rssi: Int?,
    val otaReady: Boolean,
    val uartProtocol: Int
)

data class DeviceStatus(
    val nanoFirmware: String,
    val uartProtocol: Int,
    val mode: String,
    val modeId: Int,
    val rtcValid: Boolean,
    val rtcTime: String?,
    val clapEnabled: Boolean,
    val clapThreshold: Int,
    val clapTimeoutMs: Int,
    val currentLimitMa: Int,
    val musicMode: String,
    val ambientEffect: String,
    val resetFlags: Int,
    val uptimeMs: Long,
    val nanoRaw: String
)

data class TimeStatus(
    val valid: Boolean,
    val date: String?,
    val time: String?,
    val nanoRaw: String
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
    val dirty: Boolean,
    val clapEnabled: Boolean
)

data class LightSettings(
    val colorMode: String,
    val kelvin: Int,
    val red: Int,
    val green: Int,
    val blue: Int,
    val brightness: Int,
    val saved: Boolean,
    val dirty: Boolean
)

data class ClapCalibrationState(
    val active: Boolean,
    val finished: Boolean,
    val goodPairs: Int,
    val targetPairs: Int,
    val quietP99: Int,
    val suggestedThreshold: Int
)

data class ClapSettings(
    val enabled: Boolean,
    val threshold: Int,
    val timeoutMs: Int,
    val saved: Boolean,
    val calibration: ClapCalibrationState
)

data class NightSettings(
    val enabled: Boolean,
    val hue: Int,
    val saturation: Int,
    val brightness: Int,
    val scheduleEnabled: Boolean,
    val scheduleOn: String,
    val scheduleOff: String
)

data class AlarmSettings(
    val enabled: Boolean,
    val hour: Int,
    val minute: Int,
    val fadeMinutes: Int,
    val maxBrightness: Int,
    val startHue: Int,
    val endHue: Int,
    val dawnPhase: String,
    val recovered: Boolean
)

data class AmbientEffectSettings(
    val hue: Int,
    val saturation: Int? = null,
    val brightness: Int,
    val speed: Int? = null,
    val rainbowStep: Double? = null
)

data class AmbientSettings(
    val effect: String,
    val autoCycle: Boolean,
    val autoPeriodSec: Int,
    val effects: Map<String, AmbientEffectSettings>
)

data class MusicModeSettings(
    val brightness: Int,
    val backgroundBrightness: Int,
    val smoothing: Int,
    val sensitivity: Int,
    val speed: Int,
    val aux: Int,
    val submode: Int
)

data class MusicSettings(
    val selected: String,
    val modes: Map<String, MusicModeSettings>
)

data class SystemSettings(
    val currentLimitMa: Int,
    val audioCalibrated: Boolean,
    val micDc: Int,
    val vuLowPass: Int,
    val spectrumLowPass: Int
)

data class ArduSettings(
    val schema: Int,
    val mode: String,
    val light: LightSettings,
    val clap: ClapSettings,
    val night: NightSettings,
    val alarm: AlarmSettings,
    val ambient: AmbientSettings,
    val music: MusicSettings,
    val system: SystemSettings
)

data class ClapCalibrationSample(
    val accepted: Boolean,
    val goodPairs: Int?,
    val targetPairs: Int?,
    val detectedClaps: Int?,
    val weakStrength: Int?,
    val strongStrength: Int?
)

data class ClapCalibrationResult(
    val quietP99: Int,
    val clapP20: Int,
    val suggestedThreshold: Int
)

data class AudioCalibration(
    val micDc: Int,
    val vuLowPass: Int,
    val spectrumLowPass: Int
)

data class CurrentLimit(
    val milliamps: Int,
    val hardMaxMa: Int
)

data class ArduEvent(
    val type: String,
    val code: Int,
    val args: List<Int>,
    val raw: String
)

data class NanoResponse(
    val command: String,
    val nano: String,
    val timedOut: Boolean,
    val errorCode: Int
)
