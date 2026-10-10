package com.ardu.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class DawnPresetsTest {
    @Test fun allPresetsUseWarmKelvinAndPracticalDurations() {
        assertEquals(4, DawnPresets.all.size)
        for (p in DawnPresets.all) {
            assertTrue(p.minutes in 20..40)
            assertTrue(p.startKelvin in 1800..4000)
            assertTrue(p.endKelvin in 1800..4000)
            assertTrue(p.startKelvin <= p.endKelvin)
            assertEquals(0, (p.startKelvin - 1800) % 20)
            assertEquals(0, (p.endKelvin - 1800) % 20)
            assertTrue(p.percent in 20..60)
            assertEquals(
                p, DawnPresets.matching(p.minutes, p.maxBrightness, p.startKelvin, p.endKelvin)
            )
        }
    }

    @Test fun manualAdjustmentMakesItCustom() {
        val p = DawnPresets.all[2]
        assertNull(DawnPresets.matching(31, p.maxBrightness, p.startKelvin, p.endKelvin))
        assertEquals(1800, DawnPresets.quantizeKelvin(1250))
        assertEquals(2000, DawnPresets.quantizeKelvin(1992))
        assertEquals(4000, DawnPresets.quantizeKelvin(4800))
    }

    @Test fun standardWakeIsThirtyMinutesBeforeSetTime() {
        val p = DawnPresets.all.first { it.id == "standard" }
        assertEquals(30, p.minutes)
        assertEquals(2000, p.startKelvin)
        assertEquals(3600, p.endKelvin)
        assertEquals(40, p.percent)
        val wakeMinute = 7 * 60
        assertEquals(6 * 60 + 30, (wakeMinute + 1440 - p.minutes) % 1440)
    }
}
