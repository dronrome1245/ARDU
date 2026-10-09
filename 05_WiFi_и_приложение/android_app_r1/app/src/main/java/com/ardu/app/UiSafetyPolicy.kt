package com.ardu.app

/**
 * Small, testable guard rules for the ARDU interface.
 * Never infer device state from initial XML defaults or from local slider previews.
 */
object UiSafetyPolicy {
    const val UNTESTED_LIMIT_MA = 3000

    fun canSendCommand(snapshotConnected: Boolean): Boolean = snapshotConnected

    fun needsCurrentLimitConfirmation(requestedMa: Int): Boolean =
        requestedMa > UNTESTED_LIMIT_MA

    // Nano dawn phase is IDLE, RUNNING or HOLD. HOLD still drives the LEDs.
    fun dawnCanBeStopped(phase: String): Boolean =
        phase.equals("RUNNING", ignoreCase = true) ||
            phase.equals("HOLD", ignoreCase = true)
}
