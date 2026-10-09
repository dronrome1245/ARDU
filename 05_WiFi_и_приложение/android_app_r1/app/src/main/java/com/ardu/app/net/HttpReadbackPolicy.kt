package com.ardu.app.net

import java.io.EOFException
import java.io.IOException
import java.util.Locale

/**
 * Retry only interrupted read-only HTTP GET requests.
 * Never repeat a POST: the Nano may have applied its command before the
 * network closed, and replaying it would duplicate an external side effect.
 */
object HttpReadbackPolicy {
    fun shouldRetryGet(error: IOException): Boolean {
        if (error is EOFException) return true
        val message = error.message.orEmpty().lowercase(Locale.ROOT)
        return message.contains("unexpected end of stream") ||
            message.contains("connection reset")
    }
}
