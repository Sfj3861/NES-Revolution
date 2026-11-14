package org.sfj3861.nesrevolution

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

internal class ControllerView(context: Context) : View(context) {
    var onButtonsChanged: ((Set<Int>) -> Unit)? = null
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pressed = mutableSetOf<Int>()
    private val labels = arrayOf("A", "B", "SELECT", "START", "↑", "↓", "←", "→")
    private val hitRects = Array(8) { RectF() }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.rgb(22, 24, 29))
        val unit = min(width, height).toFloat()
        val r = unit * 0.19f
        val dx = width * 0.19f
        val dy = height * 0.48f
        drawCircleKey(canvas, dx, dy - r * 0.92f, r * 0.58f, 4)
        drawCircleKey(canvas, dx, dy + r * 0.92f, r * 0.58f, 5)
        drawCircleKey(canvas, dx - r * 0.92f, dy, r * 0.58f, 6)
        drawCircleKey(canvas, dx + r * 0.92f, dy, r * 0.58f, 7)
        val actionR = unit * 0.19f
        drawCircleKey(canvas, width * 0.78f, height * 0.47f, actionR, 1)
        drawCircleKey(canvas, width * 0.90f, height * 0.39f, actionR, 0)
        drawPillKey(canvas, width * 0.43f, height * 0.78f, width * 0.12f, height * 0.15f, 2)
        drawPillKey(canvas, width * 0.58f, height * 0.78f, width * 0.12f, height * 0.15f, 3)
    }

    private fun drawCircleKey(canvas: Canvas, x: Float, y: Float, radius: Float, index: Int) {
        paint.color = if (index in pressed) Color.rgb(90, 145, 220) else Color.rgb(65, 72, 84)
        canvas.drawCircle(x, y, radius, paint)
        hitRects[index].set(x - radius * 1.35f, y - radius * 1.35f, x + radius * 1.35f, y + radius * 1.35f)
        drawLabel(canvas, labels[index], x, y, radius * 0.78f)
    }

    private fun drawPillKey(canvas: Canvas, cx: Float, cy: Float, w: Float, h: Float, index: Int) {
        val rect = RectF(cx - w / 2f, cy - h / 2f, cx + w / 2f, cy + h / 2f)
        hitRects[index].set(rect)
        paint.color = if (index in pressed) Color.rgb(90, 145, 220) else Color.rgb(65, 72, 84)
        canvas.drawRoundRect(rect, h / 2f, h / 2f, paint)
        drawLabel(canvas, labels[index], cx, cy, min(h * 0.42f, w * 0.16f))
    }

    private fun drawLabel(canvas: Canvas, label: String, x: Float, y: Float, size: Float) {
        paint.color = Color.WHITE
        paint.textAlign = Paint.Align.CENTER
        paint.textSize = size
        val baseline = y - (paint.ascent() + paint.descent()) / 2f
        canvas.drawText(label, x, baseline, paint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN,
            MotionEvent.ACTION_MOVE, MotionEvent.ACTION_POINTER_UP -> updatePressed(event)
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> updateButtons(emptySet())
            else -> return true
        }
        return true
    }

    private fun updatePressed(event: MotionEvent) {
        val next = mutableSetOf<Int>()
        for (pointer in 0 until event.pointerCount) {
            if (event.actionMasked == MotionEvent.ACTION_POINTER_UP && pointer == event.actionIndex) continue
            val x = event.getX(pointer)
            val y = event.getY(pointer)
            for (index in hitRects.indices) if (hitRects[index].contains(x, y)) next.add(index)
        }
        updateButtons(next)
    }

    private fun updateButtons(next: Set<Int>) {
        if (next == pressed) return
        pressed.clear()
        pressed.addAll(next)
        onButtonsChanged?.invoke(pressed.toSet())
        invalidate()
    }
}
