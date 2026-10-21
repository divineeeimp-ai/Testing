package com.jarvis.assistant

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast

/** Setup screen: permissions for HyperOS, the On/Off switch, a test button and Uninstall. */
class MainActivity : Activity() {

    private lateinit var power: Button
    private lateinit var state: TextView
    private lateinit var checklist: LinearLayout
    private val cyan = Color.rgb(41, 216, 255)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        power = findViewById(R.id.btnPower)
        state = findViewById(R.id.state)
        checklist = findViewById(R.id.checklist)

        power.setOnClickListener { toggle() }
        findViewById<View>(R.id.btnTest).setOnClickListener {
            if (!hasMic()) { askMic(); return@setOnClickListener }
            startActivity(Intent(this, JarvisActivity::class.java))
        }
        findViewById<View>(R.id.btnUninstall).setOnClickListener { confirmUninstall() }
    }

    override fun onResume() {
        super.onResume()
        refresh()
        // service may still be starting up
        Handler(Looper.getMainLooper()).postDelayed({ if (!isFinishing) refresh() }, 800)
    }

    private fun hasMic() =
        checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED

    private fun hasNotif() = Build.VERSION.SDK_INT < 33 ||
        checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    private fun askMic() {
        val perms = mutableListOf(Manifest.permission.RECORD_AUDIO)
        if (Build.VERSION.SDK_INT >= 33) perms += Manifest.permission.POST_NOTIFICATIONS
        requestPermissions(perms.toTypedArray(), 1)
    }

    override fun onRequestPermissionsResult(code: Int, p: Array<out String>, r: IntArray) {
        super.onRequestPermissionsResult(code, p, r)
        refresh()
    }

    private fun toggle() {
        if (JarvisService.isRunning) {
            Prefs.setEnabled(this, false)
            JarvisService.send(this, JarvisService.ACTION_STOP)
        } else {
            if (!hasMic()) { askMic(); return }
            Prefs.setEnabled(this, true)
            JarvisService.send(this, JarvisService.ACTION_START)
            Toast.makeText(this, "Jarvis is online. Say \"Hey Jarvis\".", Toast.LENGTH_LONG).show()
        }
        Handler(Looper.getMainLooper()).postDelayed({ refresh() }, 600)
    }

    private fun refresh() {
        val on = JarvisService.isRunning
        power.text = if (on) "TURN JARVIS OFF" else "TURN JARVIS ON"
        state.text = if (on) "● Online — say “Hey Jarvis”" else "○ Offline"
        state.setTextColor(if (on) cyan else Color.GRAY)

        val pm = getSystemService(PowerManager::class.java)
        checklist.removeAllViews()
        row("Microphone & notifications", hasMic() && hasNotif()) { askMic() }
        row("Display over other apps", Settings.canDrawOverlays(this)) {
            open(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))
        }
        row("Xiaomi: “Display pop-up windows while running in background” + “Show on Lock screen”", null) {
            openXiaomiPermissions()
        }
        row("Xiaomi: Autostart", null) {
            open(Intent().setComponent(ComponentName(
                "com.miui.securitycenter",
                "com.miui.permcenter.autostart.AutoStartManagementActivity"
            )))
        }
        row("Battery saver: No restrictions", pm.isIgnoringBatteryOptimizations(packageName)) {
            open(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName")))
        }
        row("Lock Jarvis in Recents (open Recents, long-press Jarvis, tap the lock)", null, null)
    }

    /** ok = true/false shows ✓/✗, null = can't be checked automatically */
    private fun row(label: String, ok: Boolean?, action: (() -> Unit)?) {
        val tv = TextView(this).apply {
            text = when (ok) {
                true -> "✓  $label"
                false -> "✗  $label  — tap to fix"
                null -> "•  $label" + if (action != null) "  — tap to open" else ""
            }
            setTextColor(when (ok) { true -> cyan; false -> Color.rgb(255, 120, 120); null -> Color.LTGRAY })
            textSize = 15f
            gravity = Gravity.START
            val pad = (12 * resources.displayMetrics.density).toInt()
            setPadding(pad, pad, pad, pad)
            if (action != null) {
                setBackgroundResource(R.drawable.bg_row)
                setOnClickListener { action() }
            }
        }
        val lp = LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = (8 * resources.displayMetrics.density).toInt() }
        checklist.addView(tv, lp)
    }

    private fun openXiaomiPermissions() {
        val miui = Intent("miui.intent.action.APP_PERM_EDITOR")
            .setClassName("com.miui.securitycenter", "com.miui.permcenter.permissions.PermissionsEditorActivity")
            .putExtra("extra_pkgname", packageName)
        open(miui)
    }

    /** Opens a settings screen; falls back to the app's info page if HyperOS moved it. */
    private fun open(intent: Intent) {
        try {
            startActivity(intent)
        } catch (_: Exception) {
            startActivity(
                Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.fromParts("package", packageName, null))
            )
            Toast.makeText(this, "Look under \"Other permissions\" / \"Autostart\" here", Toast.LENGTH_LONG).show()
        }
    }

    private fun confirmUninstall() {
        AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle("Uninstall Jarvis?")
            .setMessage("This removes Jarvis from your phone.")
            .setPositiveButton("Uninstall") { _, _ ->
                Prefs.setEnabled(this, false)
                JarvisService.send(this, JarvisService.ACTION_STOP)
                startActivity(Intent(Intent.ACTION_DELETE, Uri.fromParts("package", packageName, null)))
            }
            .setNegativeButton("Cancel", null)
            .show()
    }
}
