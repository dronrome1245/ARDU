package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.util.AttributeSet
import android.view.View

class SceneTileView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    enum class Scene {
        EVENING,
        WARM,
        DAY,
        COOL
    }

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val titlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
    }
    private val subtitlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(190, 198, 207)
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif", Typeface.NORMAL)
    }
    private var scene = Scene.EVENING

    init {
        isClickable = true
        foreground = null
    }

    fun setScene(value: Scene) {
        if (scene == value) return
        scene = value
        contentDescription = when (scene) {
            Scene.EVENING -> "Сцена Вечер"
            Scene.WARM -> "Сцена Тёплый свет"
            Scene.DAY -> "Сцена Дневной свет"
            Scene.COOL -> "Сцена Холодный свет"
        }
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        setMeasuredDimension(
            resolveSize(dp(86f).toInt(), widthMeasureSpec),
            resolveSize(dp(112f).toInt(), heightMeasureSpec)
        )
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()
        val radius = dp(17f)
        val imageBottom = h * .66f

        val colors = when (scene) {
            Scene.EVENING -> intArrayOf(Color.rgb(23, 27, 38), Color.rgb(73, 47, 32))
            Scene.WARM -> intArrayOf(Color.rgb(37, 29, 20), Color.rgb(112, 71, 35))
            Scene.DAY -> intArrayOf(Color.rgb(23, 43, 45), Color.rgb(56, 96, 88))
            Scene.COOL -> intArrayOf(Color.rgb(20, 31, 47), Color.rgb(47, 82, 122))
        }

        paint.shader = LinearGradient(0f, 0f, w, imageBottom, colors, null, Shader.TileMode.CLAMP)
        canvas.drawRoundRect(RectF(0f, 0f, w, h), radius, radius, paint)
        paint.shader = null

        // Tiny room/window composition.
        paint.color = Color.argb(80, 255, 255, 255)
        canvas.drawRoundRect(
            RectF(w * .12f, h * .11f, w * .52f, imageBottom * .76f),
            dp(7f), dp(7f), paint
        )
        paint.color = when (scene) {
            Scene.EVENING -> Color.rgb(18, 25, 39)
            Scene.WARM -> Color.rgb(48, 37, 27)
            Scene.DAY -> Color.rgb(187, 224, 219)
            Scene.COOL -> Color.rgb(142, 190, 235)
        }
        canvas.drawRoundRect(
            RectF(w * .14f, h * .13f, w * .50f, imageBottom * .74f),
            dp(6f), dp(6f), paint
        )

        // Sofa.
        paint.color = Color.rgb(36, 45, 51)
        canvas.drawRoundRect(
            RectF(w * .18f, imageBottom * .52f, w * .78f, imageBottom * .88f),
            dp(9f), dp(9f), paint
        )

        // Lamp + glow.
        val lamp = when (scene) {
            Scene.EVENING -> Color.rgb(255, 160, 85)
            Scene.WARM -> Color.rgb(255, 191, 102)
            Scene.DAY -> Color.rgb(255, 238, 199)
            Scene.COOL -> Color.rgb(173, 216, 255)
        }
        paint.color = Color.argb(45, Color.red(lamp), Color.green(lamp), Color.blue(lamp))
        canvas.drawCircle(w * .78f, imageBottom * .44f, dp(24f), paint)
        paint.color = lamp
        canvas.drawRoundRect(
            RectF(w * .69f, imageBottom * .22f, w * .87f, imageBottom * .45f),
            dp(4f), dp(4f), paint
        )
        paint.strokeWidth = dp(3f)
        canvas.drawLine(w * .78f, imageBottom * .45f, w * .78f, imageBottom * .77f, paint)

        // Label zone darkening.
        paint.color = Color.argb(85, 0, 0, 0)
        canvas.drawRoundRect(
            RectF(0f, imageBottom * .88f, w, h),
            radius, radius, paint
        )

        titlePaint.textSize = dp(12f)
        subtitlePaint.textSize = dp(9.5f)
        val title = when (scene) {
            Scene.EVENING -> "Вечер"
            Scene.WARM -> "Тёплый"
            Scene.DAY -> "Дневной"
            Scene.COOL -> "Холодный"
        }
        val subtitle = when (scene) {
            Scene.EVENING -> "2200 K"
            Scene.WARM -> "2700 K"
            Scene.DAY -> "4000 K"
            Scene.COOL -> "6000 K"
        }
        canvas.drawText(title, w / 2f, h - dp(25f), titlePaint)
        canvas.drawText(subtitle, w / 2f, h - dp(10f), subtitlePaint)
    }

    private fun dp(v: Float): Float = v * resources.displayMetrics.density
}
