package com.ardu.app

/** Foreground monitoring state; a reachable ESP does not imply that Nano works. */
enum class DeviceConnectionHealth {
    ONLINE, ESP_ONLY, SETTINGS_UNAVAILABLE, DISCONNECTED
}

object DeviceConnectionPolicy {
    const val HEARTBEAT_PERIOD_MS = 5_000L

    fun classify(
        espResponded: Boolean,
        nanoResponded: Boolean,
        settingsAvailable: Boolean = true
    ): DeviceConnectionHealth = when {
        !espResponded -> DeviceConnectionHealth.DISCONNECTED
        !nanoResponded -> DeviceConnectionHealth.ESP_ONLY
        !settingsAvailable -> DeviceConnectionHealth.SETTINGS_UNAVAILABLE
        else -> DeviceConnectionHealth.ONLINE
    }
}
