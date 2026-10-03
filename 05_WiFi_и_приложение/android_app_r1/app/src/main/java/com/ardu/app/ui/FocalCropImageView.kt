package com.ardu.app.ui

import android.content.Context
import android.graphics.Matrix
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatImageView
import kotlin.math.max

class FocalCropImageView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    private var focusX = 0.5f
    private var focusY = 0.5f

    init {
        scaleType = ScaleType.MATRIX
    }

    fun setFocus(x: Float, y: Float = focusY) {
        focusX = x.coerceIn(0f, 1f)
        focusY = y.coerceIn(0f, 1f)
        updateImageMatrix(drawable)
    }

    override fun setImageDrawable(drawable: Drawable?) {
        super.setImageDrawable(drawable)
        updateImageMatrix(drawable)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        updateImageMatrix(drawable)
    }

    private fun updateImageMatrix(source: Drawable?) {
        if (source == null || width <= 0 || height <= 0) return

        val drawableWidth = source.intrinsicWidth.toFloat()
        val drawableHeight = source.intrinsicHeight.toFloat()
        if (drawableWidth <= 0f || drawableHeight <= 0f) return

        val scale = max(width / drawableWidth, height / drawableHeight)
        val scaledWidth = drawableWidth * scale
        val scaledHeight = drawableHeight * scale

        var dx = width * 0.5f - drawableWidth * focusX * scale
        var dy = height * 0.5f - drawableHeight * focusY * scale

        // Clamp so the image always fully covers the view.
        dx = dx.coerceIn(width - scaledWidth, 0f)
        dy = dy.coerceIn(height - scaledHeight, 0f)

        imageMatrix = Matrix().apply {
            setScale(scale, scale)
            postTranslate(dx, dy)
        }
    }
}
