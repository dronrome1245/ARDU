package com.ardu.app.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.util.AttributeSet
import android.view.View
import com.ardu.app.LightQuickScenes
import com.ardu.app.R
import kotlin.math.max

class SceneTileView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    enum class Scene { EVENING, WARM, DAY, COOL }

    private val imagePaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)
    private val overlayPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val moodPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1f)
        color = Color.argb(75, 255, 255, 255)
    }
    private val titlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-medium", Typeface.BOLD)
    }
    private val subtitlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(210, 216, 224)
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif", Typeface.NORMAL)
    }

    private var scene = Scene.EVENING
    private var bitmap: Bitmap = decode(Scene.EVENING)

    init {
        isClickable = true
        contentDescription = "Сцена Вечер"
    }

    fun setScene(value: Scene) {
        if (scene == value) return
        scene = value
        bitmap = decode(scene)
        contentDescription = when (scene) {
            Scene.EVENING -> "Сцена Вечер"
            Scene.WARM -> "Сцена Кино"
            Scene.DAY -> "Сцена Гости"
            Scene.COOL -> "Сцена Чтение"
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
        val bounds = RectF(0f, 0f, w, h)

        val save = canvas.save()
        val clip = Path().apply { addRoundRect(bounds, radius, radius, Path.Direction.CW) }
        canvas.clipPath(clip)

        val src = centerCropSource(bitmap, width, height)
        canvas.drawBitmap(bitmap, src, RectF(0f, 0f, w, h), imagePaint)

        // Evening = inviting golden lamp light; Cinema = dark blue theater.
        // Keep the existing real furniture photos, grade them independently.
        moodPaint.shader = when (scene) {
            Scene.EVENING -> LinearGradient(
                0f, 0f, w, h,
                Color.argb(50, 255, 188, 100), Color.argb(22, 255, 120, 44),
                Shader.TileMode.CLAMP
            )
            Scene.WARM -> LinearGradient(
                0f, 0f, w, h,
                Color.argb(138, 7, 23, 60), Color.argb(104, 9, 15, 45),
                Shader.TileMode.CLAMP
            )
            else -> null
        }
        if (moodPaint.shader != null) {
            canvas.drawRect(0f, 0f, w, h, moodPaint)
            moodPaint.shader = null
        }

        overlayPaint.shader = LinearGradient(
            0f, h * .30f, 0f, h,
            intArrayOf(
                Color.TRANSPARENT,
                Color.argb(35, 5, 8, 12),
                Color.argb(225, 7, 10, 14)
            ),
            floatArrayOf(0f, .48f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null
        canvas.restoreToCount(save)

        canvas.drawRoundRect(bounds, radius, radius, borderPaint)

        titlePaint.textSize = dp(12f)
        subtitlePaint.textSize = dp(9.5f)
        val title = when (scene) {
            Scene.EVENING -> "Вечер"
            Scene.WARM -> "Кино"
            Scene.DAY -> "Гости"
            Scene.COOL -> "Чтение"
        }
        val subtitle = when (scene) {
            Scene.EVENING -> "${LightQuickScenes.EVENING.percent}%"
            Scene.WARM -> "${LightQuickScenes.CINEMA.percent}%"
            Scene.DAY -> "${LightQuickScenes.GUESTS.percent}%"
            Scene.COOL -> "${LightQuickScenes.READING.percent}%"
        }
        canvas.drawText(title, w / 2f, h - dp(24f), titlePaint)
        canvas.drawText(subtitle, w / 2f, h - dp(9f), subtitlePaint)
    }

    private fun decode(scene: Scene): Bitmap =
        BitmapFactory.decodeResource(
            resources,
            when (scene) {
                Scene.EVENING -> R.drawable.ardu_scene_evening
                Scene.WARM -> R.drawable.ardu_scene_warm
                Scene.DAY -> R.drawable.ardu_scene_day
                Scene.COOL -> R.drawable.ardu_scene_cool
            }
        )

    private fun centerCropSource(bitmap: Bitmap, targetW: Int, targetH: Int): Rect {
        val scale = max(
            targetW.toFloat() / bitmap.width.toFloat(),
            targetH.toFloat() / bitmap.height.toFloat()
        )
        val visibleW = (targetW / scale).toInt().coerceAtMost(bitmap.width)
        val visibleH = (targetH / scale).toInt().coerceAtMost(bitmap.height)
        val left = (bitmap.width - visibleW) / 2
        val top = (bitmap.height - visibleH) / 2
        return Rect(left, top, left + visibleW, top + visibleH)
    }

    private fun dp(v: Float): Float = v * resources.displayMetrics.density
}
