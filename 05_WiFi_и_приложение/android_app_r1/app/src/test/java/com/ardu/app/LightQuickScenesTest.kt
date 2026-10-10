package com.ardu.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LightQuickScenesTest {
    @Test fun eveningAndCinemaArePerceptuallyDifferent() {
        val evening = LightQuickScenes.EVENING
        val cinema = LightQuickScenes.CINEMA
        assertEquals(45, evening.percent)
        assertEquals(7, cinema.percent)
        assertTrue("Cinema needs to be much dimmer", evening.brightness >= cinema.brightness * 4)
        assertTrue("Color temperature must be visibly different", evening.kelvin - cinema.kelvin >= 700)
    }

    @Test fun otherApprovedLightScenesRemainUnchanged() {
        assertEquals(3500, LightQuickScenes.GUESTS.kelvin)
        assertEquals(178, LightQuickScenes.GUESTS.brightness)
        assertEquals(4300, LightQuickScenes.READING.kelvin)
        assertEquals(230, LightQuickScenes.READING.brightness)
    }
}
