package com.ardu.app.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

class ColorWheelView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val wheelPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val markerOuter = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(4f)
        color = Color.WHITE
    }
    private val markerInner = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1.5f)
        color = Color.argb(190, 0, 0, 0)
    }

    private var wheelBitmap: Bitmap? = null
    private var hue = 25f
    private var saturation = 0.55f

    private var previewListener: ((Int) -> Unit)? = null
    private var commitListener: ((Int) -> Unit)? = null

    fun setListener(
        onPreview: (Int) -> Unit,
        onCommit: (Int) -> Unit
    ) {
        previewListener = onPreview
        commitListener = onCommit
    }

    fun setColor(color: Int) {
        val hsv = FloatArray(3)
        Color.colorToHSV(color, hsv)
        hue = hsv[0]
        saturation = hsv[1].coerceIn(0f, 1f)
        invalidate()
    }

    fun selectedColor(): Int =
        Color.HSVToColor(floatArrayOf(hue, saturation, 1f))

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desired = dp(280f).toInt()
        val width = resolveSize(desired, widthMeasureSpec)
        val height = resolveSize(desired, heightMeasureSpec)
        val size = min(width, height)
        setMeasuredDimension(size, size)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        if (w <= 0 || h <= 0) return
        wheelBitmap?.recycle()
        wheelBitmap = buildWheel(w, h)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        wheelBitmap?.let { canvas.drawBitmap(it, 0f, 0f, wheelPaint) }

        val radius = min(width, height) * 0.5f - dp(7f)
        val centerX = width * 0.5f
        val centerY = height * 0.5f
        val angle = Math.toRadians(hue.toDouble())
        val markerRadius = radius * saturation
        val x = centerX + cos(angle).toFloat() * markerRadius
        val y = centerY + sin(angle).toFloat() * markerRadius
        canvas.drawCircle(x, y, dp(10f), markerOuter)
        canvas.drawCircle(x, y, dp(12f), markerInner)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (!insideWheel(event.x, event.y)) return false
                parent?.requestDisallowInterceptTouchEvent(true)
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(selectedColor())
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(selectedColor())
                return true
            }
            MotionEvent.ACTION_UP -> {
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(selectedColor())
                commitListener?.invoke(selectedColor())
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

    private fun insideWheel(x: Float, y: Float): Boolean {
        val dx = x - width * 0.5f
        val dy = y - height * 0.5f
        return hypot(dx, dy) <= min(width, height) * 0.5f
    }

    private fun updateFromTouch(x: Float, y: Float) {
        val centerX = width * 0.5f
        val centerY = height * 0.5f
        val radius = min(width, height) * 0.5f - dp(7f)
        val dx = x - centerX
        val dy = y - centerY
        saturation = (hypot(dx, dy) / radius).coerceIn(0f, 1f)
        var angle = Math.toDegrees(atan2(dy, dx).toDouble()).toFloat()
        if (angle < 0f) angle += 360f
        hue = angle
        invalidate()
    }

    private fun buildWheel(w: Int, h: Int): Bitmap {
        val bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val pixels = IntArray(w * h)
        val centerX = w * 0.5f
        val centerY = h * 0.5f
        val radius = min(w, h) * 0.5f - dp(3f)

        var index = 0
        for (y in 0 until h) {
            val dy = y - centerY
            for (x in 0 until w) {
                val dx = x - centerX
                val distance = hypot(dx, dy)
                pixels[index++] = if (distance <= radius) {
                    var angle = Math.toDegrees(atan2(dy, dx).toDouble()).toFloat()
                    if (angle < 0f) angle += 360f
                    val sat = (distance / radius).coerceIn(0f, 1f)
                    Color.HSVToColor(floatArrayOf(angle, sat, 1f))
                } else {
                    Color.TRANSPARENT
                }
            }
        }

        bitmap.setPixels(pixels, 0, w, 0, 0, w, h)
        return bitmap
    }

    private fun dp(value: Float): Float =
        value * resources.displayMetrics.density
}
