package com.jarvis.assistant

import android.app.Activity
import android.app.AlertDialog
import android.app.NotificationManager
import android.content.Intent
import android.content.pm.PackageManager
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.net.Uri
import android.os.BatteryManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.text.format.DateFormat
import android.view.View
import android.widget.TextView
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** The pop-up Jarvis screen: listens for a command, answers, and has Off / Uninstall. */
class JarvisActivity : Activity(), TextToSpeech.OnInitListener {

    companion object {
        @Volatile
        var isOpen = false
    }

    private lateinit var reactor: ArcReactorView
    private lateinit var status: TextView
    private lateinit var transcript: TextView

    private val main = Handler(Looper.getMainLooper())
    private var tts: TextToSpeech? = null
    private var recognizer: SpeechRecognizer? = null
    private var resumeOnClose = true
    private var torchOn = false

    private val autoClose = Runnable { finish() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_jarvis)
        isOpen = true

        reactor = findViewById(R.id.reactor)
        status = findViewById(R.id.status)
        transcript = findViewById(R.id.transcript)

        findViewById<View>(R.id.btnMic).setOnClickListener { startListening() }
        reactor.setOnClickListener { startListening() }
        findViewById<View>(R.id.btnOff).setOnClickListener { turnOff() }
        findViewById<View>(R.id.btnUninstall).setOnClickListener { confirmUninstall() }
        findViewById<View>(R.id.btnClose).setOnClickListener { finish() }

        getSystemService(NotificationManager::class.java).cancel(JarvisService.WAKE_NOTIF_ID)
        if (JarvisService.isRunning) JarvisService.send(this, JarvisService.ACTION_PAUSE)

        if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            status.text = "I need microphone access. Open the Jarvis app to set me up."
            return
        }
        status.text = "Online"
        tts = TextToSpeech(this, this)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        getSystemService(NotificationManager::class.java).cancel(JarvisService.WAKE_NOTIF_ID)
        say("Yes, sir?", listenAfter = true)
    }

    // ---------- Voice output ----------
    override fun onInit(result: Int) {
        val t = tts ?: return
        if (result != TextToSpeech.SUCCESS) {
            startListening(); return
        }
        val lang = t.setLanguage(Locale.UK)
        if (lang == TextToSpeech.LANG_MISSING_DATA || lang == TextToSpeech.LANG_NOT_SUPPORTED) {
            t.setLanguage(Locale.US)
        }
        t.setPitch(0.85f)
        t.setSpeechRate(1.0f)
        t.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(id: String?) {
                main.post { reactor.speaking = true }
            }

            override fun onDone(id: String?) {
                main.post {
                    reactor.speaking = false
                    if (id == "listen") startListening() else scheduleAutoClose()
                }
            }

            @Deprecated("Deprecated in Java")
            override fun onError(id: String?) = onDone(id)
        })
        say("Yes, sir?", listenAfter = true)
    }

    private fun say(text: String, listenAfter: Boolean = false) {
        main.removeCallbacks(autoClose)
        status.text = text
        val t = tts
        if (t == null) {
            if (listenAfter) startListening() else scheduleAutoClose()
            return
        }
        t.speak(text, TextToSpeech.QUEUE_FLUSH, null, if (listenAfter) "listen" else "done")
    }

    private fun scheduleAutoClose() {
        main.removeCallbacks(autoClose)
        main.postDelayed(autoClose, 8000)
    }

    // ---------- Voice input ----------
    private fun startListening() {
        main.removeCallbacks(autoClose)
        tts?.stop()
        reactor.speaking = false
        if (!SpeechRecognizer.isRecognitionAvailable(this)) {
            status.text = "Speech recognition isn't available. Install/update the Google app."
            return
        }
        recognizer?.destroy()
        recognizer = SpeechRecognizer.createSpeechRecognizer(this).apply {
            setRecognitionListener(listener)
        }
        val i = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
            .putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            .putExtra(RecognizerIntent.EXTRA_LANGUAGE, "en-US")
            .putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
            .putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1)
        recognizer?.startListening(i)
        reactor.listening = true
        status.text = "Listening…"
        transcript.text = ""
    }

    private val listener = object : RecognitionListener {
        override fun onReadyForSpeech(params: Bundle?) {
            status.text = "Listening…"
        }

        override fun onRmsChanged(rmsdB: Float) = reactor.setLevel(rmsdB)
        override fun onBeginningOfSpeech() {}
        override fun onBufferReceived(buffer: ByteArray?) {}
        override fun onEndOfSpeech() {
            reactor.listening = false
            status.text = "Thinking…"
        }

        override fun onError(error: Int) {
            reactor.listening = false
            status.text = when (error) {
                SpeechRecognizer.ERROR_NO_MATCH, SpeechRecognizer.ERROR_SPEECH_TIMEOUT ->
                    "I didn't catch that. Tap the mic."
                SpeechRecognizer.ERROR_NETWORK, SpeechRecognizer.ERROR_NETWORK_TIMEOUT ->
                    "No connection. Tap the mic to try again."
                else -> "Tap the mic to talk."
            }
            scheduleAutoClose()
        }

        override fun onPartialResults(partialResults: Bundle?) {
            partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                ?.firstOrNull()?.let { transcript.text = "“$it”" }
        }

        override fun onResults(results: Bundle?) {
            reactor.listening = false
            val text = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                ?.firstOrNull().orEmpty()
            if (text.isBlank()) {
                status.text = "I didn't catch that. Tap the mic."
                scheduleAutoClose()
                return
            }
            transcript.text = "“$text”"
            handle(text)
        }

        override fun onEvent(eventType: Int, params: Bundle?) {}
    }

    // ---------- Commands ----------
    private fun handle(raw: String) {
        val c = raw.lowercase(Locale.ROOT).trim()
        fun has(vararg w: String) = w.any { c.contains(it) }

        when {
            !c.startsWith("open ") && Regex("\\b(flashlight|torch|lights?)\\b").containsMatchIn(c) -> {
                val on = when {
                    has(" on") -> true
                    has(" off") -> false
                    else -> !torchOn
                }
                if (setTorch(on)) say(if (on) "Flashlight on." else "Flashlight off.")
                else say("I couldn't reach the flashlight.")
            }
            has("turn off", "go to sleep", "shut down", "stop listening", "power off") -> {
                say("Going offline. Goodbye, sir.")
                main.postDelayed({ turnOff() }, 2200)
            }
            has("uninstall", "delete yourself") -> confirmUninstall()
            c.startsWith("open ") || c.startsWith("launch ") || c.startsWith("start ") ->
                openApp(c.substringAfter(' ').trim())
            has("what time", "the time") ->
                say("It's ${DateFormat.getTimeFormat(this).format(Date())}, sir.")
            has("date", "what day") ->
                say("Today is ${SimpleDateFormat("EEEE, MMMM d", Locale.US).format(Date())}.")
            has("battery") -> {
                val bm = getSystemService(BatteryManager::class.java)
                val lvl = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
                say("Battery is at $lvl percent.")
            }
            c.startsWith("play ") || has("youtube") -> {
                val q = c.removePrefix("play ").replace("on youtube", "").replace("youtube", "").trim()
                launchAndClose(
                    Intent(Intent.ACTION_VIEW, Uri.parse("https://www.youtube.com/results?search_query=" + Uri.encode(q))),
                    "Playing $q on YouTube."
                )
            }
            c.startsWith("search ") || c.startsWith("google ") -> {
                val q = c.substringAfter(' ').removePrefix("for ").trim()
                webSearch(q)
            }
            has("thank") -> say("Always a pleasure, sir.")
            has("who are you", "your name") ->
                say("I am Jarvis. Just A Rather Very Intelligent System.")
            has("hello", "hi jarvis", "hey jarvis", "you there") -> say("At your service, sir.", listenAfter = true)
            has("how are you") -> say("All systems operational, sir.")
            else -> webSearch(raw)
        }
    }

    private fun webSearch(q: String) {
        launchAndClose(
            Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com/search?q=" + Uri.encode(q))),
            "Searching for $q."
        )
    }

    private fun openApp(name: String) {
        val pm = packageManager
        val apps = pm.queryIntentActivities(
            Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER), 0
        )
        val match = apps.firstOrNull { it.loadLabel(pm).toString().equals(name, true) }
            ?: apps.firstOrNull { it.loadLabel(pm).toString().contains(name, true) }
            ?: apps.firstOrNull { name.contains(it.loadLabel(pm).toString(), true) }
        val launch = match?.let { pm.getLaunchIntentForPackage(it.activityInfo.packageName) }
        if (launch == null) {
            say("I couldn't find an app called $name.")
            return
        }
        launchAndClose(launch, "Opening ${match.loadLabel(pm)}.")
    }

    private fun launchAndClose(intent: Intent, speech: String) {
        say(speech)
        main.postDelayed({
            try {
                startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            } catch (_: Exception) {
            }
            finish()
        }, 1300)
    }

    private fun setTorch(on: Boolean): Boolean = try {
        val cm = getSystemService(CameraManager::class.java)
        val id = cm.cameraIdList.first {
            cm.getCameraCharacteristics(it).get(CameraCharacteristics.FLASH_INFO_AVAILABLE) == true
        }
        cm.setTorchMode(id, on)
        torchOn = on
        true
    } catch (_: Exception) {
        false
    }

    // ---------- Off / Uninstall ----------
    private fun turnOff() {
        resumeOnClose = false
        Prefs.setEnabled(this, false)
        JarvisService.send(this, JarvisService.ACTION_STOP)
        finish()
    }

    private fun confirmUninstall() {
        recognizer?.cancel()
        reactor.listening = false
        main.removeCallbacks(autoClose)
        AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle("Uninstall Jarvis?")
            .setMessage("This removes Jarvis from your phone.")
            .setPositiveButton("Uninstall") { _, _ -> uninstall() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun uninstall() {
        resumeOnClose = false
        Prefs.setEnabled(this, false)
        JarvisService.send(this, JarvisService.ACTION_STOP)
        startActivity(
            Intent(Intent.ACTION_DELETE, Uri.fromParts("package", packageName, null))
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        )
        finish()
    }

    // ---------- Lifecycle ----------
    override fun onStop() {
        super.onStop()
        // If you leave the screen (home button etc.), close Jarvis so the wake word resumes.
        if (!isChangingConfigurations && !isFinishing) finish()
    }

    override fun onDestroy() {
        isOpen = false
        main.removeCallbacksAndMessages(null)
        recognizer?.destroy()
        recognizer = null
        tts?.stop()
        tts?.shutdown()
        tts = null
        if (resumeOnClose && JarvisService.isRunning) {
            JarvisService.send(this, JarvisService.ACTION_RESUME)
        }
        super.onDestroy()
    }
}
