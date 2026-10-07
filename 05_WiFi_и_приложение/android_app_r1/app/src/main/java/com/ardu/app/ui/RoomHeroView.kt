package com.ardu.app.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import com.ardu.app.R
import kotlin.math.max
import kotlin.math.roundToInt

class RoomHeroView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    enum class Scene { LIVING, MUSIC, AMBIENT, NIGHT, DAWN }

    private val imagePaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)
    private val overlayPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1f)
        color = Color.argb(58, 255, 255, 255)
    }
    private val roomBitmap: Bitmap by lazy(LazyThreadSafetyMode.NONE) {
        BitmapFactory.decodeResource(resources, R.drawable.ardu_light_hero)
    }

    private var scene = Scene.LIVING

    fun setScene(value: Scene) {
        if (scene == value) return
        scene = value
        contentDescription = when (scene) {
            Scene.LIVING -> "Гостиная"
            Scene.MUSIC -> "Музыкальная атмосфера"
            Scene.AMBIENT -> "Фоновое освещение комнаты"
            Scene.NIGHT -> "Комната ночью"
            Scene.DAWN -> "Комната на рассвете"
        }
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desiredHeight = dp(170f).toInt()
        setMeasuredDimension(
            resolveSize(suggestedMinimumWidth.coerceAtLeast(dp(280f).toInt()), widthMeasureSpec),
            resolveSize(desiredHeight, heightMeasureSpec)
        )
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (width <= 0 || height <= 0) return

        val w = width.toFloat()
        val h = height.toFloat()
        val radius = dp(22f)
        val bounds = RectF(0f, 0f, w, h)

        val save = canvas.save()
        val clip = Path().apply {
            addRoundRect(bounds, radius, radius, Path.Direction.CW)
        }
        canvas.clipPath(clip)

        val focus = focalPoint(scene)
        val source = focalCropSource(
            roomBitmap,
            width,
            height,
            focus.first,
            focus.second
        )
        canvas.drawBitmap(roomBitmap, source, bounds, imagePaint)

        drawSceneGrade(canvas, w, h)
        drawReadabilityVignette(canvas, w, h)

        canvas.restoreToCount(save)
        canvas.drawRoundRect(bounds, radius, radius, borderPaint)
    }

    private fun drawSceneGrade(canvas: Canvas, w: Float, h: Float) {
        overlayPaint.shader = when (scene) {
            Scene.LIVING -> LinearGradient(
                0f, 0f, w, h,
                intArrayOf(
                    Color.argb(18, 44, 213, 181),
                    Color.TRANSPARENT,
                    Color.argb(18, 255, 178, 94)
                ),
                floatArrayOf(0f, .52f, 1f),
                Shader.TileMode.CLAMP
            )

            Scene.MUSIC -> LinearGradient(
                0f, 0f, w, h,
                intArrayOf(
                    Color.argb(92, 31, 46, 96),
                    Color.argb(42, 18, 18, 38),
                    Color.argb(84, 21, 139, 126)
                ),
                floatArrayOf(0f, .50f, 1f),
                Shader.TileMode.CLAMP
            )

            Scene.AMBIENT -> LinearGradient(
                0f, h, w, 0f,
                intArrayOf(
                    Color.argb(78, 104, 42, 168),
                    Color.argb(20, 20, 24, 35),
                    Color.argb(74, 18, 158, 162)
                ),
                floatArrayOf(0f, .48f, 1f),
                Shader.TileMode.CLAMP
            )

            Scene.NIGHT -> LinearGradient(
                0f, 0f, w, h,
                intArrayOf(
                    Color.argb(132, 6, 17, 41),
                    Color.argb(82, 8, 16, 30),
                    Color.argb(40, 41, 24, 24)
                ),
                floatArrayOf(0f, .58f, 1f),
                Shader.TileMode.CLAMP
            )

            Scene.DAWN -> LinearGradient(
                0f, h, w, 0f,
                intArrayOf(
                    Color.argb(56, 67, 42, 52),
                    Color.argb(34, 255, 151, 78),
                    Color.argb(92, 255, 210, 139)
                ),
                floatArrayOf(0f, .52f, 1f),
                Shader.TileMode.CLAMP
            )
        }
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null

        when (scene) {
            Scene.MUSIC -> {
                overlayPaint.shader = RadialGradient(
                    w * .25f, h * .52f, w * .52f,
                    Color.argb(54, 80, 116, 255),
                    Color.TRANSPARENT,
                    Shader.TileMode.CLAMP
                )
                canvas.drawRect(0f, 0f, w, h, overlayPaint)
            }

            Scene.AMBIENT -> {
                overlayPaint.shader = RadialGradient(
                    w * .73f, h * .44f, w * .56f,
                    Color.argb(52, 79, 255, 218),
                    Color.TRANSPARENT,
                    Shader.TileMode.CLAMP
                )
                canvas.drawRect(0f, 0f, w, h, overlayPaint)
            }

            Scene.NIGHT -> {
                overlayPaint.shader = RadialGradient(
                    w * .83f, h * .40f, w * .42f,
                    Color.argb(54, 255, 161, 85),
                    Color.TRANSPARENT,
                    Shader.TileMode.CLAMP
                )
                canvas.drawRect(0f, 0f, w, h, overlayPaint)
            }

            Scene.DAWN -> {
                overlayPaint.shader = RadialGradient(
                    w * .78f, h * .34f, w * .60f,
                    Color.argb(72, 255, 220, 164),
                    Color.TRANSPARENT,
                    Shader.TileMode.CLAMP
                )
                canvas.drawRect(0f, 0f, w, h, overlayPaint)
            }

            Scene.LIVING -> Unit
        }
        overlayPaint.shader = null
    }

    private fun drawReadabilityVignette(canvas: Canvas, w: Float, h: Float) {
        overlayPaint.shader = LinearGradient(
            0f, h * .48f, 0f, h,
            intArrayOf(
                Color.TRANSPARENT,
                Color.argb(28, 4, 7, 10),
                Color.argb(112, 4, 7, 10)
            ),
            floatArrayOf(0f, .48f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null
    }

    private fun focalPoint(value: Scene): Pair<Float, Float> = when (value) {
        Scene.LIVING -> .64f to .50f
        Scene.MUSIC -> .28f to .48f
        Scene.AMBIENT -> .52f to .50f
        Scene.NIGHT -> .72f to .50f
        Scene.DAWN -> .60f to .48f
    }

    private fun focalCropSource(
        bitmap: Bitmap,
        targetWidth: Int,
        targetHeight: Int,
        focusX: Float,
        focusY: Float
    ): Rect {
        val scale = max(
            targetWidth.toFloat() / bitmap.width.toFloat(),
            targetHeight.toFloat() / bitmap.height.toFloat()
        )
        val visibleWidth = (targetWidth / scale).roundToInt().coerceIn(1, bitmap.width)
        val visibleHeight = (targetHeight / scale).roundToInt().coerceIn(1, bitmap.height)

        val centerX = (focusX.coerceIn(0f, 1f) * bitmap.width).roundToInt()
        val centerY = (focusY.coerceIn(0f, 1f) * bitmap.height).roundToInt()

        val maxLeft = bitmap.width - visibleWidth
        val maxTop = bitmap.height - visibleHeight
        val left = (centerX - visibleWidth / 2).coerceIn(0, maxLeft)
        val top = (centerY - visibleHeight / 2).coerceIn(0, maxTop)

        return Rect(left, top, left + visibleWidth, top + visibleHeight)
    }

    private fun dp(value: Float): Float =
        value * resources.displayMetrics.density
}
