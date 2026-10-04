package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import kotlin.math.min

class MusicModeTileView @JvmOverloads constructor(
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
        typeface = android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL)
        textSize = sp(11.2f)
    }

    private var modeId: String = "M01"
    private var title: String = "Градиент"

    init {
        isClickable = true
        isFocusable = true
        minimumHeight = dp(78f).toInt()
        setLayerType(LAYER_TYPE_SOFTWARE, null)
    }

    fun configure(id: String, label: String) {
        modeId = id
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
            backgroundPaint.setShadowLayer(dp(8f), 0f, 0f, Color.argb(95, 93, 226, 197))
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

        val iconTop = dp(8f)
        val iconBottom = h * 0.58f
        val iconBounds = RectF(
            dp(9f),
            iconTop,
            w - dp(9f),
            iconBottom
        )
        drawModeIcon(canvas, iconBounds)

        val textY = h - dp(10f)
        titlePaint.color = if (isSelected) Color.rgb(244, 255, 252) else Color.rgb(244, 247, 250)
        canvas.drawText(title, w * 0.5f, textY, titlePaint)
    }

    private fun drawModeIcon(canvas: Canvas, b: RectF) {
        when (modeId) {
            "M01" -> drawGradientWave(canvas, b)
            "M02" -> drawRainbow(canvas, b)
            "M03" -> drawBars(canvas, b, intArrayOf(15, 27, 39, 31, 21))
            "M04" -> drawBars(canvas, b, intArrayOf(18, 38, 25))
            "M05" -> drawFrequencyBeacon(canvas, b)
            "M08" -> drawRunningDots(canvas, b)
            "M09" -> drawSpectrum(canvas, b)
            else -> drawGradientWave(canvas, b)
        }
    }

    private fun drawGradientWave(canvas: Canvas, b: RectF) {
        val p = Path()
        val y = b.centerY()
        p.moveTo(b.left, y)
        p.cubicTo(
            b.left + b.width() * .18f, b.top,
            b.left + b.width() * .30f, b.bottom,
            b.left + b.width() * .48f, y
        )
        p.cubicTo(
            b.left + b.width() * .66f, b.top,
            b.left + b.width() * .80f, b.bottom,
            b.right, y
        )
        iconPaint.style = Paint.Style.STROKE
        iconPaint.strokeCap = Paint.Cap.ROUND
        iconPaint.strokeWidth = dp(4.8f)
        iconPaint.shader = LinearGradient(
            b.left, 0f, b.right, 0f,
            intArrayOf(
                Color.rgb(232, 74, 234),
                Color.rgb(85, 129, 255),
                Color.rgb(26, 224, 247),
                Color.rgb(95, 236, 137)
            ),
            null,
            Shader.TileMode.CLAMP
        )
        canvas.drawPath(p, iconPaint)
        iconPaint.shader = null
    }

    private fun drawRainbow(canvas: Canvas, b: RectF) {
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

        val maxRadius = min(b.width() * .48f, b.height() * .90f)
        val cx = b.centerX()
        val baseY = b.bottom - dp(2f)
        colors.forEachIndexed { i, color ->
            val radius = maxRadius - i * dp(4.3f)
            if (radius <= dp(4f)) return@forEachIndexed
            iconPaint.color = color
            val r = RectF(cx - radius, baseY - radius, cx + radius, baseY + radius)
            canvas.drawArc(r, 180f, 180f, false, iconPaint)
        }
    }

    private fun drawBars(canvas: Canvas, b: RectF, heights: IntArray) {
        iconPaint.style = Paint.Style.FILL
        val colors = intArrayOf(
            Color.rgb(112, 76, 255),
            Color.rgb(42, 170, 255),
            Color.rgb(28, 225, 222),
            Color.rgb(72, 228, 128),
            Color.rgb(255, 174, 57)
        )
        val gap = dp(4f)
        val totalGap = gap * (heights.size - 1)
        val barW = (b.width() - totalGap) / heights.size
        val maxH = heights.maxOrNull()?.toFloat() ?: 1f

        heights.forEachIndexed { i, raw ->
            val h = b.height() * .80f * raw / maxH
            val left = b.left + i * (barW + gap)
            val top = b.bottom - h
            iconPaint.color = colors[i * (colors.size - 1) / (heights.size - 1).coerceAtLeast(1)]
            canvas.drawRoundRect(
                RectF(left, top, left + barW, b.bottom),
                barW * .45f,
                barW * .45f,
                iconPaint
            )
        }
    }

    private fun drawFrequencyBeacon(canvas: Canvas, b: RectF) {
        val cx = b.centerX()
        val cy = b.centerY()
        val maxRadius = min(b.width() * .34f, b.height() * .62f)

        iconPaint.style = Paint.Style.STROKE
        iconPaint.strokeCap = Paint.Cap.ROUND
        iconPaint.strokeWidth = dp(3.4f)
        iconPaint.shader = LinearGradient(
            b.left, 0f, b.right, 0f,
            Color.rgb(31, 225, 242),
            Color.rgb(122, 82, 255),
            Shader.TileMode.CLAMP
        )

        floatArrayOf(.42f, .70f, 1f).forEach { fraction ->
            val radius = maxRadius * fraction
            val oval = RectF(cx - radius, cy - radius, cx + radius, cy + radius)
            canvas.drawArc(oval, -52f, 104f, false, iconPaint)
            canvas.drawArc(oval, 128f, 104f, false, iconPaint)
        }

        iconPaint.shader = null
        iconPaint.style = Paint.Style.FILL
        iconPaint.color = Color.rgb(255, 190, 62)
        canvas.drawCircle(cx, cy, dp(4.6f), iconPaint)

        iconPaint.color = Color.rgb(31, 225, 242)
        canvas.drawRoundRect(
            RectF(cx - dp(1.4f), cy - dp(13f), cx + dp(1.4f), cy - dp(6.5f)),
            dp(1.4f),
            dp(1.4f),
            iconPaint
        )
    }

    private fun drawRunningDots(canvas: Canvas, b: RectF) {
        iconPaint.style = Paint.Style.FILL
        val colors = intArrayOf(
            Color.rgb(20, 220, 237),
            Color.rgb(48, 168, 255),
            Color.rgb(95, 104, 255),
            Color.rgb(173, 72, 250),
            Color.rgb(242, 74, 196),
            Color.rgb(255, 98, 137)
        )
        val n = colors.size
        val radius = min(dp(5.2f), b.width() / (n * 3f))
        for (i in 0 until n) {
            val x = b.left + (i + 0.5f) * b.width() / n
            iconPaint.color = colors[i]
            canvas.drawCircle(x, b.centerY(), radius, iconPaint)
        }
    }

    private fun drawSpectrum(canvas: Canvas, b: RectF) {
        iconPaint.style = Paint.Style.FILL
        val heights = floatArrayOf(.24f, .38f, .58f, .82f, 1f, .72f, .48f, .30f)
        val colors = intArrayOf(
            Color.rgb(20, 204, 255),
            Color.rgb(41, 152, 255),
            Color.rgb(92, 97, 255),
            Color.rgb(155, 76, 255),
            Color.rgb(225, 72, 232),
            Color.rgb(255, 91, 167),
            Color.rgb(255, 141, 72),
            Color.rgb(126, 226, 77)
        )
        val gap = dp(2.5f)
        val barW = (b.width() - gap * (heights.size - 1)) / heights.size
        heights.forEachIndexed { i, fraction ->
            val h = b.height() * .82f * fraction
            val left = b.left + i * (barW + gap)
            iconPaint.color = colors[i]
            canvas.drawRoundRect(
                RectF(left, b.bottom - h, left + barW, b.bottom),
                barW * .35f,
                barW * .35f,
                iconPaint
            )
        }
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    private fun dp(v: Float): Float = v * resources.displayMetrics.density
    private fun sp(v: Float): Float = v * resources.displayMetrics.scaledDensity
}
