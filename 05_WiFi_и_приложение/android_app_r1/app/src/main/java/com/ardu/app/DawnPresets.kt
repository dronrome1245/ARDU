package com.ardu.app

/**
 * Warm-only alarm scenes. Kelvin is approximated by the existing Nano L01 LUT.
 * The wake time is the END of the ramp; fade begins this many minutes earlier.
 */
data class DawnPreset(
    val id: String,
    val title: String,
    val minutes: Int,
    val startKelvin: Int,
    val endKelvin: Int,
    val maxBrightness: Int
) {
    val percent: Int get() = (maxBrightness * 100 + 127) / 255
}

object DawnPresets {
    const val MIN_KELVIN = 1800
    const val MAX_KELVIN = 4000

    val all = listOf(
        DawnPreset("extra_gentle", "Очень мягкий", 40, 1800, 3000, 64),
        DawnPreset("gentle", "Мягкий", 25, 1800, 3200, 77),
        DawnPreset("standard", "Стандарт", 30, 2000, 3600, 102),
        DawnPreset("energizing", "Бодрый", 20, 2200, 4000, 140)
    )

    fun quantizeKelvin(kelvin: Int): Int =
        MIN_KELVIN +
            ((kelvin.coerceIn(MIN_KELVIN, MAX_KELVIN) - MIN_KELVIN + 10) / 20) * 20

    fun matching(
        minutes: Int,
        brightness: Int,
        startKelvin: Int,
        endKelvin: Int
    ): DawnPreset? = all.firstOrNull {
        it.minutes == minutes && it.maxBrightness == brightness &&
            it.startKelvin == startKelvin && it.endKelvin == endKelvin
    }
}
