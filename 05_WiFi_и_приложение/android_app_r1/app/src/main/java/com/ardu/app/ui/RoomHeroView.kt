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
        drawSceneContext(canvas, w, h)
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

    private fun drawSceneContext(canvas: Canvas, w: Float, h: Float) {
        when (scene) {
            Scene.AMBIENT -> drawAmbientContext(canvas, w, h)
            Scene.NIGHT -> drawNightContext(canvas, w, h)
            Scene.DAWN -> drawDawnContext(canvas, w, h)
            else -> Unit
        }
    }

    private fun drawAmbientContext(canvas: Canvas, w: Float, h: Float) {
        // Turn the shared room photo into a recognisable media-wall scene.
        val tv = RectF(w * .33f, h * .20f, w * .72f, h * .57f)

        overlayPaint.shader = RadialGradient(
            tv.centerX(), tv.centerY(), w * .34f,
            intArrayOf(
                Color.argb(115, 80, 98, 255),
                Color.argb(70, 35, 225, 211),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, .48f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null

        overlayPaint.color = Color.argb(230, 5, 11, 22)
        canvas.drawRoundRect(tv, dp(5f), dp(5f), overlayPaint)

        overlayPaint.shader = LinearGradient(
            tv.left, tv.top, tv.right, tv.bottom,
            intArrayOf(
                Color.rgb(17, 35, 78),
                Color.rgb(16, 58, 76),
                Color.rgb(8, 17, 34)
            ),
            null,
            Shader.TileMode.CLAMP
        )
        val inner = RectF(tv.left + dp(3f), tv.top + dp(3f), tv.right - dp(3f), tv.bottom - dp(3f))
        canvas.drawRoundRect(inner, dp(4f), dp(4f), overlayPaint)
        overlayPaint.shader = null

        overlayPaint.color = Color.argb(210, 20, 25, 31)
        canvas.drawRoundRect(
            RectF(w * .29f, h * .61f, w * .77f, h * .68f),
            dp(4f), dp(4f), overlayPaint
        )
        overlayPaint.color = Color.argb(130, 93, 226, 197)
        canvas.drawRoundRect(
            RectF(w * .31f, h * .69f, w * .75f, h * .715f),
            dp(3f), dp(3f), overlayPaint
        )
    }

    private fun drawNightContext(canvas: Canvas, w: Float, h: Float) {
        overlayPaint.color = Color.argb(120, 1, 7, 20)
        canvas.drawRect(0f, 0f, w, h, overlayPaint)

        // Cool night window with a few deterministic stars.
        val window = RectF(w * .43f, h * .10f, w * .78f, h * .46f)
        overlayPaint.shader = LinearGradient(
            window.left, window.top, window.right, window.bottom,
            Color.rgb(7, 22, 49),
            Color.rgb(17, 48, 72),
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(window, dp(5f), dp(5f), overlayPaint)
        overlayPaint.shader = null

        overlayPaint.color = Color.argb(205, 220, 238, 255)
        val stars = arrayOf(
            .49f to .18f, .58f to .28f, .69f to .16f, .73f to .34f,
            .53f to .37f, .64f to .24f
        )
        stars.forEach { (sx, sy) ->
            canvas.drawCircle(w * sx, h * sy, dp(1.1f), overlayPaint)
        }

        // Bed silhouette makes the room read as a bedroom instead of a tinted lounge.
        overlayPaint.color = Color.argb(225, 10, 22, 38)
        canvas.drawRoundRect(
            RectF(w * .18f, h * .46f, w * .77f, h * .74f),
            dp(13f), dp(13f), overlayPaint
        )
        overlayPaint.color = Color.argb(230, 23, 42, 60)
        canvas.drawRoundRect(
            RectF(w * .10f, h * .63f, w * .83f, h * .86f),
            dp(18f), dp(18f), overlayPaint
        )
        overlayPaint.color = Color.argb(220, 38, 57, 74)
        canvas.drawOval(RectF(w * .22f, h * .55f, w * .46f, h * .70f), overlayPaint)
        canvas.drawOval(RectF(w * .47f, h * .54f, w * .69f, h * .69f), overlayPaint)

        // Warm bedside lamp.
        overlayPaint.shader = RadialGradient(
            w * .86f, h * .58f, w * .20f,
            Color.argb(170, 255, 171, 79),
            Color.TRANSPARENT,
            Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null
        overlayPaint.color = Color.rgb(255, 190, 102)
        canvas.drawCircle(w * .86f, h * .58f, dp(5f), overlayPaint)
    }

    private fun drawDawnContext(canvas: Canvas, w: Float, h: Float) {
        // Warm window / sunrise light.
        overlayPaint.shader = RadialGradient(
            w * .73f, h * .30f, w * .48f,
            intArrayOf(
                Color.argb(190, 255, 214, 139),
                Color.argb(100, 255, 147, 75),
                Color.TRANSPARENT
            ),
            floatArrayOf(0f, .42f, 1f),
            Shader.TileMode.CLAMP
        )
        canvas.drawRect(0f, 0f, w, h, overlayPaint)
        overlayPaint.shader = null

        val window = RectF(w * .48f, h * .08f, w * .82f, h * .50f)
        overlayPaint.shader = LinearGradient(
            0f, window.top, 0f, window.bottom,
            intArrayOf(
                Color.rgb(255, 211, 142),
                Color.rgb(251, 151, 87),
                Color.rgb(69, 70, 91)
            ),
            null,
            Shader.TileMode.CLAMP
        )
        canvas.drawRoundRect(window, dp(5f), dp(5f), overlayPaint)
        overlayPaint.shader = null

        overlayPaint.color = Color.argb(205, 33, 37, 46)
        canvas.drawRoundRect(
            RectF(w * .13f, h * .48f, w * .76f, h * .72f),
            dp(14f), dp(14f), overlayPaint
        )
        overlayPaint.color = Color.argb(210, 52, 55, 63)
        canvas.drawRoundRect(
            RectF(w * .08f, h * .64f, w * .82f, h * .86f),
            dp(18f), dp(18f), overlayPaint
        )
        overlayPaint.color = Color.argb(225, 88, 75, 70)
        canvas.drawOval(RectF(w * .22f, h * .55f, w * .46f, h * .70f), overlayPaint)
        canvas.drawOval(RectF(w * .47f, h * .54f, w * .69f, h * .69f), overlayPaint)
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
