package com.jarvis.assistant

import android.content.Context

object Prefs {
    private const val FILE = "jarvis"
    private const val KEY_ENABLED = "enabled"

    fun isEnabled(ctx: Context) =
        ctx.getSharedPreferences(FILE, Context.MODE_PRIVATE).getBoolean(KEY_ENABLED, false)

    fun setEnabled(ctx: Context, value: Boolean) {
        ctx.getSharedPreferences(FILE, Context.MODE_PRIVATE).edit()
            .putBoolean(KEY_ENABLED, value).apply()
    }
}
