package com.ardu.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MusicBackgroundUiPolicyTest {
    @Test fun spectrumHasNoBackgroundBrightnessControl() {
        assertFalse(MusicBackgroundUiPolicy.usesBackgroundBrightness("M09"))
    }

    @Test fun vuAndBandModesExplainRealFirmwareColors() {
        assertTrue(MusicBackgroundUiPolicy.usesBackgroundBrightness("M01"))
        assertTrue(MusicBackgroundUiPolicy.colorExplanation("M01").contains("фиолетовый"))
        assertTrue(MusicBackgroundUiPolicy.colorExplanation("M03").contains("частотам"))
    }

    @Test fun noFakeColorPickerForFirmwareWithoutBackgroundHue() {
        assertFalse(MusicBackgroundUiPolicy.colorExplanation("M09").contains("настраиваемый"))
    }
}
