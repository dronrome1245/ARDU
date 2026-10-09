package com.ardu.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ClapWizardUiPolicyTest {
    @Test fun savedSessionDoesNotReopenAfterStatusReadback() {
        assertFalse(ClapWizardUiPolicy.shouldShow(false, true, true))
        assertFalse(ClapWizardUiPolicy.shouldShow(false, false, true))
    }

    @Test fun activeOrUnsavedFinishedSessionIsVisible() {
        assertTrue(ClapWizardUiPolicy.shouldShow(true, false, false))
        assertTrue(ClapWizardUiPolicy.shouldShow(false, true, false))
        assertTrue(ClapWizardUiPolicy.shouldShow(true, false, true))
    }
}
