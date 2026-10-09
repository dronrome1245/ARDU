package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.roundToInt

class SmartSliderView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    enum class VisualMode {
        ACCENT,
        BRIGHTNESS,
        KELVIN,
        HUE
    }

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val thumbPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val thumbBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(2f)
        color = Color.argb(120, 255, 255, 255)
    }

    private var minimumValue = 0
    private var maximumValue = 100
    private var currentValue = 0
    private var visualMode = VisualMode.ACCENT

    private var onPreview: ((Int) -> Unit)? = null
    private var onCommit: ((Int) -> Unit)? = null

    private val trackHeight get() = dp(24f)
    private val thumbRadius get() = dp(17f)
    private val thumbShadowReserve get() = dp(8f)
    private val edgeInset get() = thumbRadius + thumbShadowReserve

    init {
        isClickable = true
        minimumHeight = dp(54f).roundToInt()
    }

    fun configure(
        min: Int,
        max: Int,
        mode: VisualMode
    ) {
        require(max > min)
        minimumValue = min
        maximumValue = max
        visualMode = mode
        currentValue = currentValue.coerceIn(min, max)
        invalidate()
    }

    fun setValue(value: Int, notify: Boolean = false) {
        val clamped = value.coerceIn(minimumValue, maximumValue)
        if (clamped == currentValue) return
        currentValue = clamped
        invalidate()
        if (notify) onPreview?.invoke(currentValue)
    }

    fun value(): Int = currentValue

    fun setListener(
        preview: (Int) -> Unit,
        commit: (Int) -> Unit
    ) {
        onPreview = preview
        onCommit = commit
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desiredHeight = dp(58f).roundToInt()
        val width = resolveSize(suggestedMinimumWidth.coerceAtLeast(dp(220f).roundToInt()), widthMeasureSpec)
        val height = resolveSize(desiredHeight, heightMeasureSpec)
        setMeasuredDimension(width, height)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val left = edgeInset
        val right = width - edgeInset
        val centerY = height * 0.5f
        val top = centerY - trackHeight * 0.5f
        val bottom = centerY + trackHeight * 0.5f
        val radius = trackHeight * 0.5f
        val track = RectF(left, top, right, bottom)

        when (visualMode) {
            VisualMode.ACCENT -> drawAccentTrack(canvas, track, radius)
            VisualMode.BRIGHTNESS -> drawBrightnessTrack(canvas, track, radius)
            VisualMode.KELVIN -> drawKelvinTrack(canvas, track, radius)
            VisualMode.HUE -> drawHueTrack(canvas, track, radius)
        }

        val thumbX = xForValue(currentValue, left, right)
        thumbPaint.color = when (visualMode) {
            VisualMode.KELVIN -> kelvinColor(currentValue)
            VisualMode.HUE -> hueColor(currentValue)
            VisualMode.BRIGHTNESS -> Color.rgb(93, 226, 197)
            VisualMode.ACCENT -> Color.rgb(93, 226, 197)
        }
        thumbPaint.setShadowLayer(dp(7f), 0f, dp(2f), Color.argb(100, 0, 0, 0))
        setLayerType(LAYER_TYPE_SOFTWARE, thumbPaint)
        canvas.drawCircle(thumbX, centerY, thumbRadius, thumbPaint)
        thumbPaint.clearShadowLayer()
        canvas.drawCircle(thumbX, centerY, thumbRadius, thumbBorderPaint)
    }

    private fun drawAccentTrack(canvas: Canvas, track: RectF, radius: Float) {
        trackPaint.shader = null
        trackPaint.color = Color.rgb(42, 50, 61)
        canvas.drawRoundRect(track, radius, radius, trackPaint)

        val progress = normalized()
        if (progress > 0f) {
            fillPaint.shader = LinearGradient(
                track.left, 0f, track.right, 0f,
                intArrayOf(
                    Color.rgb(44, 145, 125),
                    Color.rgb(93, 226, 197)
                ),
                null,
                Shader.TileMode.CLAMP
            )
            canvas.drawRoundRect(
                RectF(track.left, track.top, track.left + track.width() * progress, track.bottom),
                radius, radius, fillPaint
            )
            fillPaint.shader = null
        }
    }

    private fun drawBrightnessTrack(canvas: Canvas, track: RectF, radius: Float) {
        trackPaint.shader = LinearGradient(
            track.left, 0f, track.right, 0f,
            intArrayOf(
                Color.rgb(31, 38, 46),
                Color.rgb(41, 81, 73),
                Color.rgb(93, 226, 197)
            ),
            floatArrayOf(0f, 0.55f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(track, radius, radius, trackPaint)
        trackPaint.shader = null

        val progress = normalized()
        if (progress < 1f) {
            val shade = RectF(
                track.left + track.width() * progress,
                track.top,
                track.right,
                track.bottom
            )
            fillPaint.color = Color.argb(125, 11, 14, 18)
            canvas.drawRoundRect(shade, radius, radius, fillPaint)
        }
    }

    private fun drawKelvinTrack(canvas: Canvas, track: RectF, radius: Float) {
        trackPaint.shader = LinearGradient(
            track.left, 0f, track.right, 0f,
            intArrayOf(
                Color.rgb(255, 119, 50),
                Color.rgb(255, 194, 118),
                Color.rgb(255, 244, 219),
                Color.rgb(219, 239, 255),
                Color.rgb(117, 186, 255)
            ),
            floatArrayOf(0f, 0.25f, 0.50f, 0.72f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(track, radius, radius, trackPaint)
        trackPaint.shader = null
    }

    private fun drawHueTrack(canvas: Canvas, track: RectF, radius: Float) {
        trackPaint.shader = LinearGradient(
            track.left, 0f, track.right, 0f,
            intArrayOf(
                Color.RED,
                Color.YELLOW,
                Color.GREEN,
                Color.CYAN,
                Color.BLUE,
                Color.MAGENTA,
                Color.RED
            ),
            null,
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(track, radius, radius, trackPaint)
        trackPaint.shader = null
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEnabled) return false
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                updateFromX(event.x)
                onPreview?.invoke(currentValue)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                updateFromX(event.x)
                onPreview?.invoke(currentValue)
                return true
            }
            MotionEvent.ACTION_UP -> {
                updateFromX(event.x)
                onPreview?.invoke(currentValue)
                onCommit?.invoke(currentValue)
                parent?.requestDisallowInterceptTouchEvent(false)
                performClick()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                parent?.requestDisallowInterceptTouchEvent(false)
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    private fun updateFromX(x: Float) {
        val left = edgeInset
        val right = width - edgeInset
        val fraction = ((x - left) / (right - left)).coerceIn(0f, 1f)
        val next = minimumValue + ((maximumValue - minimumValue) * fraction).roundToInt()
        if (next != currentValue) {
            currentValue = next
            invalidate()
        }
    }

    private fun normalized(): Float =
        if (maximumValue == minimumValue) 0f
        else (currentValue - minimumValue).toFloat() / (maximumValue - minimumValue).toFloat()

    private fun xForValue(value: Int, left: Float, right: Float): Float =
        left + (right - left) *
            ((value - minimumValue).toFloat() / (maximumValue - minimumValue).toFloat())
                .coerceIn(0f, 1f)

    private fun hueColor(value: Int): Int {
        val fraction = ((value - minimumValue).toFloat() /
            (maximumValue - minimumValue).toFloat()).coerceIn(0f, 1f)
        return Color.HSVToColor(floatArrayOf(fraction * 360f, 1f, 1f))
    }

    private fun kelvinColor(kelvin: Int): Int {
        val fraction = ((kelvin - minimumValue).toFloat() /
            (maximumValue - minimumValue).toFloat()).coerceIn(0f, 1f)
        return when {
            fraction < 0.5f -> blend(
                Color.rgb(255, 146, 70),
                Color.rgb(255, 244, 219),
                fraction * 2f
            )
            else -> blend(
                Color.rgb(255, 244, 219),
                Color.rgb(117, 186, 255),
                (fraction - 0.5f) * 2f
            )
        }
    }

    private fun blend(a: Int, b: Int, t: Float): Int {
        val clamped = t.coerceIn(0f, 1f)
        return Color.rgb(
            (Color.red(a) + (Color.red(b) - Color.red(a)) * clamped).roundToInt(),
            (Color.green(a) + (Color.green(b) - Color.green(a)) * clamped).roundToInt(),
            (Color.blue(a) + (Color.blue(b) - Color.blue(a)) * clamped).roundToInt()
        )
    }

    private fun dp(value: Float): Float =
        value * resources.displayMetrics.density
}
