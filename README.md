# Jarvis for Xiaomi / HyperOS

Say **"Hey Jarvis"** from anywhere → the Jarvis screen pops up, says *"Yes, sir?"* and listens.
The screen has a big **mic** button, an **Off** button (stops listening completely) and an **Uninstall** button (removes the app).

## 1. Get the APK (no PC needed — uses GitHub's free build servers)

1. Make a free account at github.com and create a **new repository** (e.g. `jarvis`).
2. Unzip this folder and upload **everything inside it** (including the hidden `.github` folder) — "Add file → Upload files". Easiest from a computer browser.
3. Open the **Actions** tab → "Build Jarvis APK" runs automatically (≈5 min). If it doesn't, press **Run workflow**.
4. When it's green, open the run → download **Jarvis-apk** → unzip → `app-debug.apk`.
5. Copy it to your phone and tap it. HyperOS will warn about an unknown app — allow it.

*(With Android Studio instead: open the folder, press Run. The voice model downloads automatically on the first build.)*

## 2. Set up on the phone (once)

Open **Jarvis** and tap every row in the checklist:

| Step | Why |
|---|---|
| Microphone & notifications | to hear "Hey Jarvis" |
| Display over other apps | lets Jarvis pop up over other apps |
| Xiaomi other permissions → **Display pop-up windows while running in background** and **Show on Lock screen** | HyperOS blocks pop-ups without this |
| Autostart → on | stops HyperOS killing Jarvis |
| Battery saver → **No restrictions** | same |
| Recents → long-press Jarvis → 🔒 lock | same |

Then tap **TURN JARVIS ON**. A "Jarvis is online" notification stays while it listens.

## Things you can say after "Hey Jarvis"

- "What time is it" / "What's the date" / "Battery"
- "Open WhatsApp" (any app name)
- "Flashlight on" / "Torch off"
- "Play Back in Black on YouTube"
- "Search for pizza near me" — anything else also becomes a Google search
- "Turn off" / "Go to sleep" → switches Jarvis off
- "Uninstall" → asks to remove Jarvis

## Good to know

- The wake word runs **fully offline** on the phone. The command after it uses Google's speech recognition (needs the Google app, which Global HyperOS has).
- Commands are in English.
- The mic is open all the time while Jarvis is on, so you'll see the orange mic dot and some extra battery use. Tap **Off** whenever you don't need it.
- After a reboot, open the app and tap **Turn Jarvis on** again (Android doesn't let mic apps start themselves at boot).
- Android never lets an app delete itself silently — **Uninstall** opens the system "Uninstall Jarvis?" box and you press OK.
