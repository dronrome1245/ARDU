package com.ardu.app

/**
 * Saving a CLAPCAL threshold confirms EEPROM persistence, but older Nano
 * firmware keeps the diagnostic "finished" flag set until a new session.
 * That flag is not a reason to reopen the wizard in Android.
 */
object ClapWizardUiPolicy {
    fun shouldShow(active: Boolean, finished: Boolean, dismissedAfterSave: Boolean): Boolean =
        active || (finished && !dismissedAfterSave)
}
