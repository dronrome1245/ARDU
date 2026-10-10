package com.ardu.app

/**
 * Kelvin/brightness pair sent as separate existing Light API commands.
 * Keep preview tile percentages derived from these same values.
 */
data class LightQuickScene(val kelvin: Int, val brightness: Int) {
    val percent: Int get() = (brightness * 100 + 127) / 255
}

object LightQuickScenes {
    // Evening: comfortably lit warm room, not the barely lit cinema profile.
    val EVENING = LightQuickScene(kelvin = 2700, brightness = 115)
    // Cinema: dim amber light that preserves a dark viewing atmosphere.
    val CINEMA = LightQuickScene(kelvin = 1800, brightness = 18)
    val GUESTS = LightQuickScene(kelvin = 3500, brightness = 178)
    val READING = LightQuickScene(kelvin = 4300, brightness = 230)
}
