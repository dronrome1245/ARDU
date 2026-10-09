package com.ardu.app

import org.junit.Assert.assertEquals
import org.junit.Test

class DeviceConnectionPolicyTest {
    @Test fun distinguishesGatewayFromFullDevice() {
        assertEquals(DeviceConnectionHealth.ONLINE,
            DeviceConnectionPolicy.classify(espResponded = true, nanoResponded = true))
        assertEquals(DeviceConnectionHealth.ESP_ONLY,
            DeviceConnectionPolicy.classify(espResponded = true, nanoResponded = false))
        assertEquals(DeviceConnectionHealth.DISCONNECTED,
            DeviceConnectionPolicy.classify(espResponded = false, nanoResponded = false))
        assertEquals(DeviceConnectionHealth.SETTINGS_UNAVAILABLE,
            DeviceConnectionPolicy.classify(espResponded = true, nanoResponded = true,
                settingsAvailable = false))
    }

    @Test fun intervalIsFiveSeconds() {
        assertEquals(5_000L, DeviceConnectionPolicy.HEARTBEAT_PERIOD_MS)
    }
}
