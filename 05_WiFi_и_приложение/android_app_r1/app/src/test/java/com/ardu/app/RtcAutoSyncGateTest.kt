package com.ardu.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RtcAutoSyncGateTest {
    @Test fun initialConnectionSyncsOnceEvenDuringHeartbeat() {
        val gate = RtcAutoSyncGate()
        assertTrue(gate.needsSync())
        assertTrue(gate.claimAfterNanoContact())
        assertFalse(gate.claimAfterNanoContact())
        assertFalse(gate.needsSync())
    }

    @Test fun returningToForegroundStartsNewSyncSession() {
        val gate = RtcAutoSyncGate()
        assertTrue(gate.claimAfterNanoContact())
        gate.onForegroundStarted()
        assertTrue(gate.claimAfterNanoContact())
        assertFalse(gate.claimAfterNanoContact())
    }

    @Test fun connectionLossEnablesExactlyOneReconnectSync() {
        val gate = RtcAutoSyncGate()
        assertTrue(gate.claimAfterNanoContact())
        gate.onConnectionLost()
        assertTrue(gate.claimAfterNanoContact())
        assertFalse(gate.claimAfterNanoContact())
    }

    @Test fun failedPostMustNotBeAutomaticallyReplayed() {
        val gate = RtcAutoSyncGate()
        assertTrue(gate.claimAfterNanoContact())
        // Even a failed POST might have reached Nano: its ACK can be lost.
        assertFalse(gate.claimAfterNanoContact())
        gate.onConnectionLost()
        assertTrue(gate.claimAfterNanoContact())
    }
}
