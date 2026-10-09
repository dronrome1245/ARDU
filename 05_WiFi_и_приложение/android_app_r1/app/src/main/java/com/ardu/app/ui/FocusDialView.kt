package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.SweepGradient
import android.graphics.RadialGradient
import android.graphics.Shader
import android.graphics.Matrix
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

class FocusDialView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val progressPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        color = Color.rgb(93, 226, 197)
    }
    private val glowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        color = Color.argb(80, 93, 226, 197)
    }
    private val thumbPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
    }
    private val centerGlow = Paint(Paint.ANTI_ALIAS_FLAG)
    private val tickPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        strokeWidth = dp(1.5f)
        color = Color.argb(70, 255, 255, 255)
    }
    private val valuePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(170, 180, 192)
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif", Typeface.NORMAL)
    }
    private val bulbPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(93, 226, 197)
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif", Typeface.BOLD)
    }

    private var currentValue = 0
    private var hasValue = false
    private var previewListener: ((Int) -> Unit)? = null
    private var commitListener: ((Int) -> Unit)? = null

    private val startAngle = 135f
    private val sweepAngle = 270f

    init {
        isClickable = true
        minimumHeight = dp(260f).roundToInt()
    }

    fun setValue(value: Int, notify: Boolean = false) {
        val next = value.coerceIn(0, 255)
        if (next == currentValue && hasValue) return
        currentValue = next
        hasValue = true
        invalidate()
        if (notify) previewListener?.invoke(currentValue)
    }

    fun setKnown(known: Boolean) {
        hasValue = known
        invalidate()
    }

    fun value(): Int = currentValue

    fun setListener(
        preview: (Int) -> Unit,
        commit: (Int) -> Unit
    ) {
        previewListener = preview
        commitListener = commit
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desired = dp(300f).roundToInt()
        val width = resolveSize(desired, widthMeasureSpec)
        val height = resolveSize(desired, heightMeasureSpec)
        val size = min(width, height)
        setMeasuredDimension(size, size)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val size = min(width, height).toFloat()
        val cx = width / 2f
        val cy = height / 2f
        val stroke = dp(18f)
        val radius = size * 0.5f - stroke - dp(10f)
        val rect = RectF(cx - radius, cy - radius, cx + radius, cy + radius)

        bgPaint.strokeWidth = stroke
        bgPaint.color = Color.rgb(31, 39, 48)
        canvas.drawArc(rect, startAngle, sweepAngle, false, bgPaint)

        val ringShader = SweepGradient(
            cx, cy,
            intArrayOf(
                Color.rgb(34, 129, 112),
                Color.rgb(93, 226, 197),
                Color.rgb(96, 218, 238),
                Color.rgb(93, 226, 197),
                Color.rgb(34, 129, 112)
            ),
            null
        )
        val matrix = Matrix()
        matrix.postRotate(startAngle, cx, cy)
        ringShader.setLocalMatrix(matrix)
        progressPaint.shader = ringShader

        glowPaint.strokeWidth = stroke + dp(12f)
        if (hasValue) canvas.drawArc(rect, startAngle, sweepAngle * fraction(), false, glowPaint)

        progressPaint.strokeWidth = stroke
        if (hasValue) canvas.drawArc(rect, startAngle, sweepAngle * fraction(), false, progressPaint)
        progressPaint.shader = null

        centerGlow.shader = RadialGradient(
            cx, cy, radius * .78f,
            intArrayOf(
                Color.argb(56, 93, 226, 197),
                Color.argb(22, 28, 70, 65),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, .5f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(cx, cy, radius * 0.76f, centerGlow)
        centerGlow.shader = null

        for (i in 0..10) {
            val tickAngle = Math.toRadians((startAngle + sweepAngle * (i / 10f)).toDouble())
            val outer = radius + dp(14f)
            val inner = radius + dp(if (i % 5 == 0) 5f else 8f)
            canvas.drawLine(
                cx + cos(tickAngle).toFloat() * inner,
                cy + sin(tickAngle).toFloat() * inner,
                cx + cos(tickAngle).toFloat() * outer,
                cy + sin(tickAngle).toFloat() * outer,
                tickPaint
            )
        }

        val angle = Math.toRadians((startAngle + sweepAngle * fraction()).toDouble())
        val tx = cx + cos(angle).toFloat() * radius
        val ty = cy + sin(angle).toFloat() * radius
        thumbPaint.setShadowLayer(dp(9f), 0f, dp(2f), Color.argb(120, 0, 0, 0))
        setLayerType(LAYER_TYPE_SOFTWARE, thumbPaint)
        if (hasValue) canvas.drawCircle(tx, ty, dp(13f), thumbPaint)
        thumbPaint.clearShadowLayer()

        val bulbY = cy - dp(55f)
        canvas.drawCircle(cx, bulbY, dp(17f), bulbPaint)
        canvas.drawRoundRect(
            RectF(cx - dp(10f), bulbY + dp(13f), cx + dp(10f), bulbY + dp(24f)),
            dp(4f), dp(4f), bulbPaint
        )
        bulbPaint.strokeWidth = dp(3f)
        canvas.drawLine(cx - dp(8f), bulbY + dp(27f), cx + dp(8f), bulbY + dp(27f), bulbPaint)

        valuePaint.textSize = dp(39f)
        canvas.drawText(if (hasValue) "${percent()}%" else "—", cx, cy + dp(8f), valuePaint)

        labelPaint.textSize = dp(14f)
        canvas.drawText("Яркость", cx, cy + dp(36f), labelPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEnabled || !hasValue) return false
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(currentValue)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(currentValue)
                return true
            }
            MotionEvent.ACTION_UP -> {
                updateFromTouch(event.x, event.y)
                previewListener?.invoke(currentValue)
                commitListener?.invoke(currentValue)
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

    private fun updateFromTouch(x: Float, y: Float) {
        val cx = width / 2f
        val cy = height / 2f
        var deg = Math.toDegrees(atan2((y - cy).toDouble(), (x - cx).toDouble())).toFloat()
        if (deg < 0f) deg += 360f

        var relative = deg - startAngle
        if (relative < 0f) relative += 360f

        val clamped = when {
            relative <= sweepAngle -> relative
            relative < 315f -> sweepAngle
            else -> 0f
        }
        currentValue = ((clamped / sweepAngle) * 255f).roundToInt().coerceIn(0, 255)
        invalidate()
    }

    private fun fraction(): Float = currentValue / 255f

    private fun percent(): Int = ((currentValue * 100) + 127) / 255

    private fun dp(v: Float): Float = v * resources.displayMetrics.density
}
