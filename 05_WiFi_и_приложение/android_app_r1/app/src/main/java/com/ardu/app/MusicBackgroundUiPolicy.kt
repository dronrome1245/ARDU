package com.ardu.app

/**
 * Current Nano UART/API only exposes background brightness, not an
 * independently configurable background hue. Expose this truth in Android.
 */
object MusicBackgroundUiPolicy {
    fun usesBackgroundBrightness(mode: String): Boolean = mode != "M09"

    fun colorExplanation(mode: String): String = when (mode) {
        "M01", "M02", "M05", "M08" ->
            "Цвет фона: фиолетовый, задан прошивкой Nano"
        "M03", "M04" ->
            "Цвет фона: красный / зелёный / жёлтый по частотам"
        else ->
            "Отдельный цвет фона в этом режиме не поддерживается"
    }
}
