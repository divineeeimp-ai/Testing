package com.jarvis.assistant

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.drawable.Icon
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.SystemClock
import android.provider.Settings
import android.util.Log
import org.vosk.Model
import org.vosk.Recognizer
import org.vosk.android.RecognitionListener
import org.vosk.android.SpeechService
import org.vosk.android.StorageService

/**
 * Runs in the background, keeps the mic open with a tiny offline model
 * that only understands "hey jarvis", and opens the Jarvis screen when it hears it.
 */
class JarvisService : Service(), RecognitionListener {

    companion object {
        private const val TAG = "JarvisService"
        const val ACTION_START = "com.jarvis.assistant.START"
        const val ACTION_PAUSE = "com.jarvis.assistant.PAUSE"
        const val ACTION_RESUME = "com.jarvis.assistant.RESUME"
        const val ACTION_STOP = "com.jarvis.assistant.STOP"
        const val NOTIF_ID = 1
        const val WAKE_NOTIF_ID = 2
        private const val CH_LISTEN = "jarvis_listening"
        private const val CH_WAKE = "jarvis_wake"
        private const val WAKE_PHRASE = "hey jarvis"

        @Volatile
        var isRunning = false
            private set

        fun send(ctx: Context, action: String) {
            val i = Intent(ctx, JarvisService::class.java).setAction(action)
            try {
                if (action == ACTION_START) ctx.startForegroundService(i) else ctx.startService(i)
            } catch (e: Exception) {
                Log.e(TAG, "Could not send $action", e)
            }
        }
    }

    private val handler = Handler(Looper.getMainLooper())
    private var model: Model? = null
    private var speech: SpeechService? = null
    private var paused = false
    private var lastWake = 0L

    private val watchdog = Runnable {
        // If the Jarvis screen never opened (e.g. HyperOS blocked it), go back to listening.
        if (paused && !JarvisActivity.isOpen) {
            paused = false
            startListening()
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                Prefs.setEnabled(this, false)
                shutdown()
                return START_NOT_STICKY
            }
            ACTION_PAUSE -> {
                if (!isRunning) { stopSelf(); return START_NOT_STICKY }
                paused = true
                handler.removeCallbacks(watchdog)
                stopListening()
            }
            ACTION_RESUME -> {
                if (!isRunning) { stopSelf(); return START_NOT_STICKY }
                paused = false
                startListening()
            }
            else -> {
                if (!goForeground()) {
                    stopSelf()
                    return START_NOT_STICKY
                }
                isRunning = true
                paused = false
                if (model == null) loadModel() else startListening()
            }
        }
        return START_STICKY
    }

    private fun goForeground(): Boolean {
        createChannels()
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE
        )
        val stop = PendingIntent.getService(
            this, 1, Intent(this, JarvisService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_IMMUTABLE
        )
        val n = Notification.Builder(this, CH_LISTEN)
            .setSmallIcon(R.drawable.ic_stat_jarvis)
            .setContentTitle("Jarvis is online")
            .setContentText("Say \"Hey Jarvis\"")
            .setOngoing(true)
            .setContentIntent(open)
            .addAction(Notification.Action.Builder(null as Icon?, "Turn off", stop).build())
            .build()
        return try {
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE)
            } else {
                startForeground(NOTIF_ID, n)
            }
            true
        } catch (e: Exception) {
            // Android won't let a mic service start from the background (e.g. after the system killed it).
            Log.e(TAG, "startForeground failed", e)
            false
        }
    }

    private fun createChannels() {
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(
            NotificationChannel(CH_LISTEN, "Jarvis listening", NotificationManager.IMPORTANCE_LOW)
        )
        nm.createNotificationChannel(
            NotificationChannel(CH_WAKE, "Jarvis wake-up", NotificationManager.IMPORTANCE_HIGH)
        )
    }

    private fun loadModel() {
        StorageService.unpack(this, "model-en-us", "model",
            { m ->
                model = m
                if (!paused && isRunning) startListening()
            },
            { e ->
                Log.e(TAG, "Failed to load model", e)
                shutdown()
            })
    }

    private fun startListening() {
        val m = model ?: return
        if (speech != null) return
        try {
            val rec = Recognizer(m, 16000.0f, "[\"$WAKE_PHRASE\", \"[unk]\"]")
            speech = SpeechService(rec, 16000.0f).also { it.startListening(this) }
        } catch (e: Exception) {
            Log.e(TAG, "Could not start listening", e)
            speech = null
            handler.postDelayed({ if (!paused && isRunning) startListening() }, 3000)
        }
    }

    private fun stopListening() {
        speech?.let {
            try {
                it.stop()
                it.shutdown()
            } catch (_: Exception) {
            }
        }
        speech = null
    }

    // ---- Vosk callbacks (main thread) ----
    override fun onPartialResult(hypothesis: String?) = check(hypothesis)
    override fun onResult(hypothesis: String?) = check(hypothesis)
    override fun onFinalResult(hypothesis: String?) {}
    override fun onTimeout() {}
    override fun onError(exception: Exception?) {
        Log.e(TAG, "Recognizer error", exception)
        stopListening()
        handler.postDelayed({ if (!paused && isRunning) startListening() }, 1000)
    }

    private fun check(hypothesis: String?) {
        if (hypothesis == null || paused) return
        if (!hypothesis.contains(WAKE_PHRASE)) return
        val now = SystemClock.elapsedRealtime()
        if (now - lastWake < 3000) return
        lastWake = now
        handler.post { wake() }
    }

    private fun wake() {
        if (paused) return
        paused = true
        stopListening() // free the mic for the command recognizer

        val i = Intent(this, JarvisActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)

        var launched = false
        if (Settings.canDrawOverlays(this)) {
            try {
                startActivity(i)
                launched = true
            } catch (e: Exception) {
                Log.e(TAG, "Direct launch blocked", e)
            }
        }
        if (!launched) {
            // Fallback: a heads-up / full-screen notification that opens Jarvis.
            val pi = PendingIntent.getActivity(
                this, 2, i, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
            )
            val n = Notification.Builder(this, CH_WAKE)
                .setSmallIcon(R.drawable.ic_stat_jarvis)
                .setContentTitle("Jarvis")
                .setContentText("Yes, sir? Tap to talk.")
                .setCategory(Notification.CATEGORY_CALL)
                .setFullScreenIntent(pi, true)
                .setContentIntent(pi)
                .setAutoCancel(true)
                .setTimeoutAfter(10_000)
                .build()
            getSystemService(NotificationManager::class.java).notify(WAKE_NOTIF_ID, n)
        }
        handler.removeCallbacks(watchdog)
        handler.postDelayed(watchdog, 10_000)
    }

    private fun shutdown() {
        isRunning = false
        handler.removeCallbacksAndMessages(null)
        stopListening()
        model?.close()
        model = null
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        isRunning = false
        handler.removeCallbacksAndMessages(null)
        stopListening()
        model?.close()
        model = null
        super.onDestroy()
    }
}
