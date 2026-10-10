package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.widget.LinearLayout
import kotlin.math.max

/**
 * Selected-tab lighting without any enclosing capsule, fill or border.
 * Unselected tabs are painted transparent; icon and label stay native views.
 */
class NavGlowItemView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : LinearLayout(context, attrs) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val bounds = RectF()

    init {
        setWillNotDraw(false)
    }

    private fun dp(value: Float): Float = value * resources.displayMetrics.density

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!isSelected) return

        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return
        val cx = w * 0.5f

        paint.reset()
        paint.isAntiAlias = true
        paint.color = Color.WHITE
        paint.style = Paint.Style.FILL

        // Wide atmospheric bloom with no rectangular/capsule edge.
        val haloRadius = max(w * 0.70f, dp(36f))
        paint.shader = RadialGradient(
            cx, h * 0.55f, haloRadius,
            intArrayOf(
                Color.argb(48, 62, 232, 199),
                Color.argb(18, 43, 172, 209),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.50f, 1f),
            Shader.TileMode.CLAMP
        )
        bounds.set(cx - haloRadius, dp(5f), cx + haloRadius, h + dp(2f))
        canvas.drawOval(bounds, paint)

        // Brighter but soft glow just behind the icon.
        val iconRadius = dp(26f)
        paint.shader = RadialGradient(
            cx, h * 0.37f, iconRadius,
            intArrayOf(
                Color.argb(108, 98, 247, 216),
                Color.argb(40, 58, 227, 212),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.40f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(cx, h * 0.37f, iconRadius, paint)

        // Diffused reflection under the label, not a button outline.
        val state = canvas.save()
        canvas.translate(cx, h - dp(6f))
        canvas.scale(1f, 0.23f)
        paint.shader = RadialGradient(
            0f, 0f, w * 0.45f,
            intArrayOf(
                Color.argb(140, 113, 253, 226),
                Color.argb(48, 50, 205, 222),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.38f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(0f, 0f, w * 0.45f, paint)
        canvas.restoreToCount(state)

        // Thin floating mint underline: no stroke around the icon/tab.
        paint.shader = LinearGradient(
            dp(8f), 0f, w - dp(8f), 0f,
            intArrayOf(
                Color.TRANSPARENT,
                Color.argb(185, 113, 253, 226),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.5f, 1f),
            Shader.TileMode.CLAMP
        )
        bounds.set(dp(8f), h - dp(5f), w - dp(8f), h - dp(3.8f))
        canvas.drawRoundRect(bounds, dp(2f), dp(2f), paint)
        paint.shader = null
    }
}
