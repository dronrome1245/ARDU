package com.ardu.app.net

/**
 * Pure policy for choosing the local ARDU gateway. No SSID/location permissions.
 * Direct Wi-Fi is recognized by the DHCP IPv4 link 192.168.4.x.
 * A connected direct link pins discovery to one address; once the Wi-Fi link
 * changes, Station candidates are tried first and DIRECT remains last fallback.
 */
object DirectConnectionPolicy {
    const val DIRECT_BASE_URL = "http://192.168.4.1"

    fun isDirectWifiIpv4(bytes: ByteArray): Boolean =
        bytes.size == 4 &&
            (bytes[0].toInt() and 0xff) == 192 &&
            (bytes[1].toInt() and 0xff) == 168 &&
            (bytes[2].toInt() and 0xff) == 4

    fun candidates(
        preferredBaseUrl: String?,
        selectedBaseUrl: String?,
        directWifiConnected: Boolean
    ): List<String> {
        if (directWifiConnected) return listOf(DIRECT_BASE_URL)
        return listOfNotNull(
            selectedBaseUrl?.takeUnless { it == DIRECT_BASE_URL },
            preferredBaseUrl?.takeUnless { it == DIRECT_BASE_URL },
            "http://ardu.local",
            "http://192.168.0.4",
            DIRECT_BASE_URL
        ).distinct()
    }
}
