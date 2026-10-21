package com.jarvis.assistant

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.BlurMaskFilter
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import android.view.animation.LinearInterpolator
import kotlin.math.min

/** Animated glowing "arc reactor" that pulses with your voice. */
class ArcReactorView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {

    private val cyan = Color.rgb(41, 216, 255)
    private val dp = resources.displayMetrics.density

    /** true while Jarvis is listening */
    var listening = false
    /** true while Jarvis is talking */
    var speaking = false

    private var target = 0f
    private var smooth = 0f
    private var phase = 0f

    /** Mic level from SpeechRecognizer (roughly -2..10 dB) */
    fun setLevel(rmsDb: Float) {
        target = (rmsDb.coerceIn(0f, 10f)) / 10f
    }

    private val glow = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; color = cyan; strokeWidth = 6 * dp
        maskFilter = BlurMaskFilter(14 * dp, BlurMaskFilter.Blur.NORMAL)
    }
    private val ring = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; color = cyan; strokeWidth = 2 * dp
    }
    private val seg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; color = cyan; strokeWidth = 6 * dp; strokeCap = Paint.Cap.ROUND
    }
    private val thin = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; color = cyan; strokeWidth = 3 * dp; alpha = 170
    }
    private val core = Paint(Paint.ANTI_ALIAS_FLAG)
    private val oval = RectF()

    private val anim = ValueAnimator.ofFloat(0f, 360f).apply {
        duration = 8000
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener {
            phase = it.animatedValue as Float
            invalidate()
        }
    }

    init {
        setLayerType(LAYER_TYPE_SOFTWARE, null) // needed for the blur glow
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow(); anim.start()
    }

    override fun onDetachedFromWindow() {
        anim.cancel(); super.onDetachedFromWindow()
    }

    override fun onDraw(canvas: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val r = min(width, height) / 2f * 0.86f

        val wanted = when {
            listening -> target
            speaking -> 0.35f + 0.25f * kotlin.math.sin(Math.toRadians(phase * 12.0)).toFloat()
            else -> 0f
        }
        smooth += (wanted - smooth) * 0.2f
        val pulse = 1f + smooth * 0.10f

        // outer glow + ring
        glow.alpha = (90 + 140 * smooth).toInt().coerceAtMost(255)
        canvas.drawCircle(cx, cy, r * pulse, glow)
        canvas.drawCircle(cx, cy, r * pulse, ring)

        // rotating segmented ring
        canvas.save()
        canvas.rotate(phase, cx, cy)
        val r1 = r * 0.82f
        oval.set(cx - r1, cy - r1, cx + r1, cy + r1)
        for (i in 0 until 12) canvas.drawArc(oval, i * 30f, 18f, false, seg)
        canvas.restore()

        // counter-rotating thin arcs
        canvas.save()
        canvas.rotate(-phase * 1.6f, cx, cy)
        val r2 = r * 0.64f
        oval.set(cx - r2, cy - r2, cx + r2, cy + r2)
        for (i in 0 until 3) canvas.drawArc(oval, i * 120f, 75f, false, thin)
        canvas.restore()

        // inner ring
        canvas.drawCircle(cx, cy, r * 0.48f, ring)

        // glowing core
        val cr = r * 0.36f * (1f + smooth * 0.35f)
        core.shader = RadialGradient(
            cx, cy, cr,
            intArrayOf(Color.WHITE, cyan, Color.argb(0, 41, 216, 255)),
            floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP
        )
        canvas.drawCircle(cx, cy, cr, core)
    }
}
