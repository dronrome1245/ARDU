package com.ardu.app

/** Foreground monitoring state; a reachable ESP does not imply that Nano works. */
enum class DeviceConnectionHealth {
    ONLINE, ESP_ONLY, DISCONNECTED
}

object DeviceConnectionPolicy {
    const val HEARTBEAT_PERIOD_MS = 5_000L

    fun classify(espResponded: Boolean, nanoResponded: Boolean): DeviceConnectionHealth =
        when {
            !espResponded -> DeviceConnectionHealth.DISCONNECTED
            !nanoResponded -> DeviceConnectionHealth.ESP_ONLY
            else -> DeviceConnectionHealth.ONLINE
        }
}
