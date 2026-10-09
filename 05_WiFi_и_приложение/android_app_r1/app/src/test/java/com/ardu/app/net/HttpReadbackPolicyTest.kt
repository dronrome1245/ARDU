package com.ardu.app.net

import java.io.EOFException
import java.io.IOException
import java.net.SocketException
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HttpReadbackPolicyTest {
    @Test fun oneMoreAttemptForBrokenGetStream() {
        assertTrue(HttpReadbackPolicy.shouldRetryGet(
            IOException("unexpected end of stream on Connection")))
        assertTrue(HttpReadbackPolicy.shouldRetryGet(SocketException("Connection reset")))
        assertTrue(HttpReadbackPolicy.shouldRetryGet(EOFException()))
    }

    @Test fun noBlindRetryForDeviceOrApplicationFailures() {
        assertFalse(HttpReadbackPolicy.shouldRetryGet(IOException("NANO_TIMEOUT")))
        assertFalse(HttpReadbackPolicy.shouldRetryGet(IOException("HTTP 503")))
        assertFalse(HttpReadbackPolicy.shouldRetryGet(IOException("ARDU_REQUEST_FAILED")))
        assertFalse(HttpReadbackPolicy.shouldRetryGet(IOException("Read timed out")))
    }
}
