package com.ardu.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View

class RoomHeroView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    enum class Scene { LIVING, MUSIC, AMBIENT, NIGHT, DAWN }

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val glow = Paint(Paint.ANTI_ALIAS_FLAG)
    private var scene = Scene.LIVING

    fun setScene(value: Scene) {
        if (scene == value) return
        scene = value
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
        val w = width.toFloat()
        val h = height.toFloat()
        val r = dp(22f)

        val colors = when (scene) {
            Scene.LIVING -> intArrayOf(Color.rgb(18, 47, 42), Color.rgb(22, 28, 34), Color.rgb(47, 32, 19))
            Scene.MUSIC -> intArrayOf(Color.rgb(19, 34, 44), Color.rgb(25, 21, 43), Color.rgb(15, 48, 43))
            Scene.AMBIENT -> intArrayOf(Color.rgb(17, 31, 42), Color.rgb(30, 20, 48), Color.rgb(10, 48, 49))
            Scene.NIGHT -> intArrayOf(Color.rgb(10, 17, 31), Color.rgb(14, 25, 43), Color.rgb(38, 25, 25))
            Scene.DAWN -> intArrayOf(Color.rgb(41, 26, 27), Color.rgb(68, 43, 27), Color.rgb(23, 35, 46))
        }
        paint.shader = LinearGradient(0f, 0f, w, h, colors, null, Shader.TileMode.CLAMP)
        canvas.drawRoundRect(RectF(0f, 0f, w, h), r, r, paint)
        paint.shader = null

        paint.color = Color.argb(90, 255, 255, 255)
        canvas.drawRoundRect(RectF(w * .08f, h * .14f, w * .42f, h * .62f), dp(10f), dp(10f), paint)

        if (scene == Scene.NIGHT) {
            paint.color = Color.rgb(10, 18, 36)
            canvas.drawRoundRect(RectF(w * .09f, h * .15f, w * .41f, h * .61f), dp(9f), dp(9f), paint)
            paint.color = Color.rgb(215, 228, 255)
            canvas.drawCircle(w * .31f, h * .28f, dp(12f), paint)
        } else if (scene == Scene.DAWN) {
            paint.shader = LinearGradient(
                0f, h * .15f, 0f, h * .62f,
                Color.rgb(255, 156, 79), Color.rgb(255, 223, 172),
                Shader.TileMode.CLAMP
            )
            canvas.drawRoundRect(RectF(w * .09f, h * .15f, w * .41f, h * .61f), dp(9f), dp(9f), paint)
            paint.shader = null
        } else {
            paint.color = Color.rgb(19, 32, 35)
            canvas.drawRoundRect(RectF(w * .09f, h * .15f, w * .41f, h * .61f), dp(9f), dp(9f), paint)
        }

        paint.color = when (scene) {
            Scene.NIGHT, Scene.DAWN -> Color.rgb(30, 43, 56)
            else -> Color.rgb(39, 52, 54)
        }
        canvas.drawRoundRect(RectF(w * .25f, h * .58f, w * .78f, h * .86f), dp(16f), dp(16f), paint)
        paint.color = Color.argb(110, 255, 255, 255)
        canvas.drawRoundRect(RectF(w * .29f, h * .52f, w * .50f, h * .69f), dp(12f), dp(12f), paint)
        canvas.drawRoundRect(RectF(w * .53f, h * .52f, w * .73f, h * .69f), dp(12f), dp(12f), paint)

        val lampX = w * .84f
        val lampY = h * .48f
        val lampColor = when (scene) {
            Scene.AMBIENT -> Color.rgb(138, 112, 255)
            Scene.MUSIC -> Color.rgb(93, 226, 197)
            Scene.NIGHT -> Color.rgb(255, 170, 95)
            Scene.DAWN -> Color.rgb(255, 190, 104)
            Scene.LIVING -> Color.rgb(255, 197, 119)
        }
        glow.color = Color.argb(55, Color.red(lampColor), Color.green(lampColor), Color.blue(lampColor))
        canvas.drawCircle(lampX, lampY, dp(46f), glow)
        paint.color = lampColor
        canvas.drawRoundRect(RectF(lampX - dp(19f), lampY - dp(25f), lampX + dp(19f), lampY + dp(2f)), dp(5f), dp(5f), paint)
        paint.strokeWidth = dp(4f)
        canvas.drawLine(lampX, lampY + dp(2f), lampX, h * .82f, paint)

        if (scene == Scene.MUSIC) {
            paint.color = Color.rgb(10, 13, 18)
            canvas.drawRoundRect(RectF(w * .06f, h * .54f, w * .16f, h * .88f), dp(7f), dp(7f), paint)
            paint.color = Color.rgb(93, 226, 197)
            canvas.drawCircle(w * .11f, h * .64f, dp(9f), paint)
            canvas.drawCircle(w * .11f, h * .78f, dp(13f), paint)
        }

        if (scene == Scene.AMBIENT) {
            val barY = h * .80f
            val barH = dp(8f)
            paint.shader = LinearGradient(
                w * .07f, barY, w * .92f, barY,
                intArrayOf(Color.MAGENTA, Color.BLUE, Color.CYAN, Color.GREEN, Color.YELLOW, Color.RED),
                null, Shader.TileMode.CLAMP
            )
            canvas.drawRoundRect(RectF(w * .07f, barY, w * .92f, barY + barH), barH, barH, paint)
            paint.shader = null
        }
    }

    private fun dp(v: Float): Float = v * resources.displayMetrics.density
}
