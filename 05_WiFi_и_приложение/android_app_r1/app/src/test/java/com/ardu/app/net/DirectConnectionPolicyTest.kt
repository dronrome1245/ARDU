package com.ardu.app.net

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DirectConnectionPolicyTest {
    private fun ip(a: Int, b: Int, c: Int, d: Int) =
        byteArrayOf(a.toByte(), b.toByte(), c.toByte(), d.toByte())

    @Test fun recognizesDirectDhcpSubnetOnly() {
        assertTrue(DirectConnectionPolicy.isDirectWifiIpv4(ip(192,168,4,2)))
        assertTrue(DirectConnectionPolicy.isDirectWifiIpv4(ip(192,168,4,254)))
        assertFalse(DirectConnectionPolicy.isDirectWifiIpv4(ip(192,168,0,4)))
        assertFalse(DirectConnectionPolicy.isDirectWifiIpv4(ip(10,0,0,1)))
        assertFalse(DirectConnectionPolicy.isDirectWifiIpv4(byteArrayOf(1,2,3)))
    }

    @Test fun directWiFiLocksDiscoveryAgainstOldStationAddresses() {
        assertEquals(
            listOf("http://192.168.4.1"),
            DirectConnectionPolicy.candidates(
                preferredBaseUrl = "http://192.168.0.4",
                selectedBaseUrl = "http://ardu.local",
                directWifiConnected = true
            )
        )
    }

    @Test fun stationResumesWithoutSavingDirectAsPermanentPreference() {
        assertEquals(
            listOf("http://ardu.local", "http://192.168.0.4", "http://192.168.4.1"),
            DirectConnectionPolicy.candidates(
                preferredBaseUrl = "http://192.168.4.1",
                selectedBaseUrl = "http://ardu.local",
                directWifiConnected = false
            )
        )
    }

    @Test fun rememberedStationAddressTakesPriorityAfterLeavingDirectWiFi() {
        assertEquals(
            listOf("http://192.168.0.8", "http://ardu.local",
                "http://192.168.0.4", "http://192.168.4.1"),
            DirectConnectionPolicy.candidates(
                preferredBaseUrl = "http://192.168.0.8",
                selectedBaseUrl = "http://192.168.4.1",
                directWifiConnected = false
            )
        )
    }
}
