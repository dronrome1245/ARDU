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
 * A restrained cyan/mint illuminated capsule behind the currently selected bottom tab.
 * The draw path is purely local UI. It never sends commands to the lamp.
 * The inactive state draws nothing, so the five destinations remain quiet.
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
        val cx = w / 2f

        paint.style = Paint.Style.FILL
        paint.strokeWidth = 0f
        // Diffuse halo at the edges; it is confined to the chosen tab's cell.
        val haloRadius = max(w * 0.72f, dp(36f))
        paint.shader = RadialGradient(
            cx, h * 0.57f, haloRadius,
            intArrayOf(
                Color.argb(65, 64, 215, 204),
                Color.argb(24, 39, 155, 187),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.55f, 1f),
            Shader.TileMode.CLAMP
        )
        bounds.set(cx - haloRadius, dp(5f), cx + haloRadius, h + dp(3f))
        canvas.drawOval(bounds, paint)

        // A dark glass capsule instead of a solid accent rectangle.
        bounds.set(dp(2f), dp(4f), w - dp(2f), h - dp(3.5f))
        val radius = dp(17f)
        paint.shader = LinearGradient(
            0f, bounds.top, 0f, bounds.bottom,
            intArrayOf(
                Color.argb(210, 24, 61, 64),
                Color.argb(190, 15, 38, 44),
                Color.argb(222, 13, 27, 34)
            ),
            null,
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(bounds, radius, radius, paint)

        // The soft mint center sits behind the icon; vector icon/label remain crisp.
        paint.shader = RadialGradient(
            cx, h * 0.36f, dp(27f),
            intArrayOf(
                Color.argb(94, 90, 242, 210),
                Color.argb(30, 63, 210, 204),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.52f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(cx, h * 0.36f, dp(27f), paint)

        // Subtle mint glass outline.
        paint.shader = null
        paint.color = Color.argb(75, 138, 239, 222)
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = dp(0.8f)
        canvas.drawRoundRect(bounds, radius, radius, paint)

        // Flattened radial bloom at the capsule's lower rim (no software blur layer).
        paint.style = Paint.Style.FILL
        val state = canvas.save()
        canvas.translate(cx, h - dp(6f))
        canvas.scale(1f, 0.22f)
        paint.shader = RadialGradient(
            0f, 0f, w * 0.49f,
            intArrayOf(
                Color.argb(188, 113, 253, 226),
                Color.argb(65, 50, 205, 222),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.40f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(0f, 0f, w * 0.49f, paint)
        canvas.restoreToCount(state)

        // A fine line gives a luminous lower edge without a thick neon strip.
        paint.shader = LinearGradient(
            dp(7f), 0f, w - dp(7f), 0f,
            intArrayOf(
                Color.TRANSPARENT,
                Color.argb(200, 113, 253, 226),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.5f, 1f),
            Shader.TileMode.CLAMP
        )
        bounds.set(dp(7f), h - dp(5f), w - dp(7f), h - dp(3.8f))
        canvas.drawRoundRect(bounds, dp(2f), dp(2f), paint)
        paint.shader = null
    }
}
