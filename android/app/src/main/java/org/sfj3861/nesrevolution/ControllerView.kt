package org.sfj3861.nesrevolution

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot
import kotlin.math.min

internal class ControllerView(context: Context) : View(context) {
    var onButtonsChanged: ((Set<Int>) -> Unit)? = null

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pressed = mutableSetOf<Int>()
    private val labels = arrayOf("A", "B", "SEL", "START", "↑", "↓", "←", "→")

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(Color.rgb(22, 24, 29))

        val unit = min(width, height).toFloat()
        val radius = unit * 0.16f
        val dpadX = width * 0.22f
        val dpadY = height * 0.52f
        drawKey(canvas, dpadX, dpadY - radius * 0.76f, radius * 0.62f, 4)
        drawKey(canvas, dpadX, dpadY + radius * 0.76f, radius * 0.62f, 5)
        drawKey(canvas, dpadX - radius * 0.76f, dpadY, radius * 0.62f, 6)
        drawKey(canvas, dpadX + radius * 0.76f, dpadY, radius * 0.62f, 7)

        val actionRadius = radius * 0.82f
        drawKey(canvas, width * 0.77f, height * 0.52f, actionRadius, 1)
        drawKey(canvas, width * 0.90f, height * 0.43f, actionRadius, 0)
        drawKey(canvas, width * 0.43f, height * 0.78f, radius * 0.42f, 2)
        drawKey(canvas, width * 0.56f, height * 0.78f, radius * 0.42f, 3)
    }

    private fun drawKey(canvas: Canvas, x: Float, y: Float, radius: Float, index: Int) {
        paint.color = if (pressed.contains(index)) Color.rgb(90, 145, 220) else Color.rgb(65, 72, 84)
        canvas.drawCircle(x, y, radius, paint)
        paint.color = Color.WHITE
        paint.textAlign = Paint.Align.CENTER
        paint.textSize = radius * 0.78f
        val baseline = y - (paint.ascent() + paint.descent()) / 2f
        canvas.drawText(labels[index], x, baseline, paint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN,
            MotionEvent.ACTION_MOVE,
            MotionEvent.ACTION_POINTER_UP -> updatePressed(event)
            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_CANCEL -> updateButtons(emptySet())
            else -> return true
        }
        return true
    }

    private fun updatePressed(event: MotionEvent) {
        val next = mutableSetOf<Int>()
        for (pointer in 0 until event.pointerCount) {
            if (event.actionMasked == MotionEvent.ACTION_POINTER_UP &&
                pointer == event.actionIndex) continue
            next.addAll(buttonsAt(event.getX(pointer), event.getY(pointer)))
        }
        updateButtons(next)
    }

    private fun buttonsAt(x: Float, y: Float): Set<Int> {
        val unit = min(width, height).toFloat()
        val radius = unit * 0.16f
        val dpadX = width * 0.22f
        val dpadY = height * 0.52f
        val actionRadius = radius * 0.82f
        val hits = mutableSetOf<Int>()

        fun hit(index: Int, cx: Float, cy: Float, r: Float) {
            if (hypot(x - cx, y - cy) <= r * 1.3f) hits.add(index)
        }

        hit(4, dpadX, dpadY - radius * 0.76f, radius * 0.62f)
        hit(5, dpadX, dpadY + radius * 0.76f, radius * 0.62f)
        hit(6, dpadX - radius * 0.76f, dpadY, radius * 0.62f)
        hit(7, dpadX + radius * 0.76f, dpadY, radius * 0.62f)
        hit(1, width * 0.77f, height * 0.52f, actionRadius)
        hit(0, width * 0.90f, height * 0.43f, actionRadius)
        hit(2, width * 0.43f, height * 0.78f, radius * 0.42f)
        hit(3, width * 0.56f, height * 0.78f, radius * 0.42f)
        return hits
    }

    private fun updateButtons(next: Set<Int>) {
        if (next == pressed) return
        pressed.clear()
        pressed.addAll(next)
        onButtonsChanged?.invoke(pressed.toSet())
        invalidate()
    }
}
