package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import kotlin.math.min

class AmbientEffectTileView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val backgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1.2f)
    }
    private val iconPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val titlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(244, 247, 250)
        textAlign = Paint.Align.CENTER
        typeface = android.graphics.Typeface.create(
            "sans-serif-medium",
            android.graphics.Typeface.NORMAL
        )
        textSize = sp(11.5f)
    }

    private var effectId: String = "F01"
    private var title: String = "Цвет"

    init {
        isClickable = true
        isFocusable = true
        minimumHeight = dp(88f).toInt()
        setLayerType(LAYER_TYPE_SOFTWARE, null)
    }

    fun configure(id: String, label: String) {
        effectId = id
        title = label
        contentDescription = label
        invalidate()
    }

    override fun drawableStateChanged() {
        super.drawableStateChanged()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return

        val pad = dp(1.5f)
        val radius = dp(18f)
        val bounds = RectF(pad, pad, w - pad, h - pad)

        if (isSelected) {
            backgroundPaint.color = Color.rgb(18, 60, 53)
            backgroundPaint.setShadowLayer(
                dp(8f),
                0f,
                0f,
                Color.argb(95, 93, 226, 197)
            )
            borderPaint.color = Color.rgb(93, 226, 197)
            borderPaint.strokeWidth = dp(1.6f)
        } else {
            backgroundPaint.color = Color.rgb(28, 34, 42)
            backgroundPaint.clearShadowLayer()
            borderPaint.color = Color.rgb(46, 58, 69)
            borderPaint.strokeWidth = dp(1.1f)
        }

        canvas.drawRoundRect(bounds, radius, radius, backgroundPaint)
        backgroundPaint.clearShadowLayer()
        canvas.drawRoundRect(bounds, radius, radius, borderPaint)

        val iconBounds = RectF(
            dp(10f),
            dp(9f),
            w - dp(10f),
            h * .60f
        )
        drawEffectIcon(canvas, iconBounds)

        titlePaint.color =
            if (isSelected) Color.rgb(244, 255, 252) else Color.rgb(244, 247, 250)
        canvas.drawText(title, w * .5f, h - dp(10f), titlePaint)
    }

    private fun drawEffectIcon(canvas: Canvas, bounds: RectF) {
        when (effectId) {
            "F01" -> drawColorOrb(canvas, bounds)
            "F02" -> drawFlow(canvas, bounds)
            "F03" -> drawRainbow(canvas, bounds)
            else -> drawColorOrb(canvas, bounds)
        }
    }

    private fun drawColorOrb(canvas: Canvas, bounds: RectF) {
        val cx = bounds.centerX()
        val cy = bounds.centerY()
        val radius = min(bounds.width(), bounds.height()) * .35f

        iconPaint.style = Paint.Style.FILL
        iconPaint.shader = RadialGradient(
            cx - radius * .25f,
            cy - radius * .25f,
            radius,
            intArrayOf(
                Color.rgb(248, 232, 255),
                Color.rgb(137, 99, 255),
                Color.rgb(42, 211, 218)
            ),
            floatArrayOf(0f, .48f, 1f),
            Shader.TileMode.CLAMP
        )
        iconPaint.setShadowLayer(dp(8f), 0f, 0f, Color.argb(100, 97, 111, 255))
        canvas.drawCircle(cx, cy, radius, iconPaint)
        iconPaint.clearShadowLayer()
        iconPaint.shader = null
    }

    private fun drawFlow(canvas: Canvas, bounds: RectF) {
        iconPaint.style = Paint.Style.STROKE
        iconPaint.strokeCap = Paint.Cap.ROUND
        iconPaint.strokeWidth = dp(5.2f)
        iconPaint.shader = LinearGradient(
            bounds.left,
            0f,
            bounds.right,
            0f,
            intArrayOf(
                Color.rgb(122, 82, 255),
                Color.rgb(237, 75, 218),
                Color.rgb(28, 225, 222)
            ),
            null,
            Shader.TileMode.CLAMP
        )

        val pathA = Path().apply {
            moveTo(bounds.left, bounds.centerY() + dp(7f))
            cubicTo(
                bounds.left + bounds.width() * .25f, bounds.top - dp(2f),
                bounds.left + bounds.width() * .55f, bounds.bottom + dp(2f),
                bounds.right, bounds.centerY() - dp(8f)
            )
        }
        val pathB = Path().apply {
            moveTo(bounds.left + dp(2f), bounds.centerY() - dp(8f))
            cubicTo(
                bounds.left + bounds.width() * .34f, bounds.bottom,
                bounds.left + bounds.width() * .66f, bounds.top,
                bounds.right - dp(2f), bounds.centerY() + dp(7f)
            )
        }

        canvas.drawPath(pathA, iconPaint)
        iconPaint.alpha = 165
        canvas.drawPath(pathB, iconPaint)
        iconPaint.alpha = 255
        iconPaint.shader = null
    }

    private fun drawRainbow(canvas: Canvas, bounds: RectF) {
        iconPaint.style = Paint.Style.STROKE
        iconPaint.strokeCap = Paint.Cap.BUTT
        iconPaint.strokeWidth = dp(5f)

        val colors = intArrayOf(
            Color.rgb(255, 80, 97),
            Color.rgb(255, 151, 53),
            Color.rgb(255, 224, 62),
            Color.rgb(67, 227, 126),
            Color.rgb(54, 167, 255),
            Color.rgb(161, 83, 255)
        )

        val maxRadius = min(bounds.width() * .47f, bounds.height() * .92f)
        val cx = bounds.centerX()
        val baseY = bounds.bottom - dp(1f)

        colors.forEachIndexed { index, color ->
            val radius = maxRadius - index * dp(4.3f)
            if (radius <= dp(4f)) return@forEachIndexed
            iconPaint.color = color
            val arc = RectF(
                cx - radius,
                baseY - radius,
                cx + radius,
                baseY + radius
            )
            canvas.drawArc(arc, 180f, 180f, false, iconPaint)
        }
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    private fun dp(value: Float): Float =
        value * resources.displayMetrics.density

    private fun sp(value: Float): Float =
        value * resources.displayMetrics.scaledDensity
}
