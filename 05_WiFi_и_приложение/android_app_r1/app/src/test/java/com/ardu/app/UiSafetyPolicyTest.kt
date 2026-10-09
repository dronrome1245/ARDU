package com.ardu.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class UiSafetyPolicyTest {
    @Test fun offlineActionsAreRejected() {
        assertFalse(UiSafetyPolicy.canSendCommand(false))
        assertTrue(UiSafetyPolicy.canSendCommand(true))
    }

    @Test fun currentLimitRequiresExplicitSafetyConfirmationAbove3000() {
        assertFalse(UiSafetyPolicy.needsCurrentLimitConfirmation(500))
        assertFalse(UiSafetyPolicy.needsCurrentLimitConfirmation(3000))
        assertTrue(UiSafetyPolicy.needsCurrentLimitConfirmation(3001))
        assertTrue(UiSafetyPolicy.needsCurrentLimitConfirmation(4500))
    }

    @Test fun stopDawnOnlyEnabledWhenDawnActuallyRunsOrHolds() {
        assertFalse(UiSafetyPolicy.dawnCanBeStopped("IDLE"))
        assertFalse(UiSafetyPolicy.dawnCanBeStopped(""))
        assertTrue(UiSafetyPolicy.dawnCanBeStopped("RUNNING"))
        assertTrue(UiSafetyPolicy.dawnCanBeStopped("running"))
        assertTrue(UiSafetyPolicy.dawnCanBeStopped("HOLD"))
    }
}
