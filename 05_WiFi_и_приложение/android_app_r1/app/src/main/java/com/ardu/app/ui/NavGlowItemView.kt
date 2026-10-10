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
import kotlin.math.min

/**
 * Borderless selected-tab light. Every shader reaches alpha=0 BEFORE the
 * cell edge, so neighbouring tabs show no hard clipping seam.
 */
class NavGlowItemView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : LinearLayout(context, attrs) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = RectF()

    init { setWillNotDraw(false) }

    private fun dp(v: Float) = v * resources.displayMetrics.density

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!isSelected) return
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return

        val cx = w / 2f
        // The entire glow is strictly INSIDE the selected cell. Never draw
        // a large oval clipped by the parent at x=0 or x=w.
        val safeRadius = min(w * 0.43f, h * 0.36f)
        paint.reset()
        paint.isAntiAlias = true
        paint.color = Color.WHITE
        paint.style = Paint.Style.FILL
        paint.shader = RadialGradient(
            cx, h * 0.38f, safeRadius,
            intArrayOf(
                Color.argb(114, 81, 241, 212),
                Color.argb(39, 41, 184, 207),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.48f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(cx, h * 0.38f, safeRadius, paint)

        // Second, flatter reflection ends completely before bottom and sides.
        val bottomRadius = min(w * 0.40f, dp(30f))
        val mark = canvas.save()
        canvas.translate(cx, h - dp(9f))
        canvas.scale(1f, 0.13f)
        paint.shader = RadialGradient(
            0f, 0f, bottomRadius,
            intArrayOf(
                Color.argb(150, 113, 253, 226),
                Color.argb(47, 53, 200, 209),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, 0.45f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(0f, 0f, bottomRadius, paint)
        canvas.restoreToCount(mark)

        // Small free-floating lower highlight, not an outline/capsule.
        val inset = w * 0.23f
        paint.shader = LinearGradient(
            inset, 0f, w - inset, 0f,
            intArrayOf(Color.TRANSPARENT, Color.argb(160, 113, 253, 226), Color.TRANSPARENT),
            floatArrayOf(0f, 0.5f, 1f),
            Shader.TileMode.CLAMP
        )
        line.set(inset, h - dp(6f), w - inset, h - dp(4.8f))
        canvas.drawRoundRect(line, dp(2f), dp(2f), paint)
        paint.shader = null
    }
}
