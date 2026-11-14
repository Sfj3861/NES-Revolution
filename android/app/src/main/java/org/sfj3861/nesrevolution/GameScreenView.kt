package org.sfj3861.nesrevolution

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.View

internal class GameScreenView(context: Context) : View(context) {
    private val bitmap = Bitmap.createBitmap(256, 240, Bitmap.Config.ARGB_8888)
    private val paint = Paint().apply {
        isFilterBitmap = false
        isDither = false
    }
    private val destination = RectF()
    private var hasFrame = false

    fun setFrame(pixels: IntArray) {
        if (pixels.size != 256 * 240) return
        bitmap.setPixels(pixels, 0, 256, 0, 0, 256, 240)
        hasFrame = true
        postInvalidateOnAnimation()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.BLACK)
        if (!hasFrame || width <= 0 || height <= 0) return

        val scale = minOf(width / 256f, height / 240f)
        val drawWidth = 256f * scale
        val drawHeight = 240f * scale
        val left = (width - drawWidth) / 2f
        val top = (height - drawHeight) / 2f
        destination.set(left, top, left + drawWidth, top + drawHeight)
        canvas.drawBitmap(bitmap, null, destination, paint)
    }
}
