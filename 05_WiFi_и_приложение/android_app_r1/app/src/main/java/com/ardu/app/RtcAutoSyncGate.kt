package com.ardu.app

/**
 * One forced RTC write per foreground connection, including each reconnect.
 * The caller must establish both a compatible ESP ping and Nano status before
 * claiming the write. In particular, heartbeat ticks must not re-send POSTs.
 */
internal class RtcAutoSyncGate {
    private var pending = true

    @Synchronized fun onForegroundStarted() {
        pending = true
    }

    @Synchronized fun onConnectionLost() {
        pending = true
    }

    @Synchronized fun needsSync(): Boolean = pending

    @Synchronized fun claimAfterNanoContact(): Boolean {
        if (!pending) return false
        pending = false
        return true
    }
}
