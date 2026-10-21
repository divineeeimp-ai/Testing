import java.net.URI

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.jarvis.assistant"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.jarvis.assistant"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
        ndk {
            // Xiaomi phones are ARM; keeps the APK smaller
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    // Offline speech engine used for the "Hey Jarvis" wake word
    implementation("com.alphacephei:vosk-android:0.3.47@aar")
    implementation("net.java.dev.jna:jna:5.13.0@aar")
}

// Downloads the small offline English model (~40 MB) into assets the first time you build.
val voskModelDir = file("src/main/assets/model-en-us")
val downloadVoskModel by tasks.registering {
    description = "Downloads the offline Vosk model used for the Hey Jarvis wake word"
    onlyIf { !File(voskModelDir, "uuid").exists() }
    doLast {
        val zip = File(layout.buildDirectory.get().asFile, "vosk-model.zip")
        zip.parentFile.mkdirs()
        println("Downloading Vosk model…")
        URI("https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip")
            .toURL().openStream().use { input -> zip.outputStream().use { input.copyTo(it) } }
        project.copy {
            from(zipTree(zip))
            eachFile {
                relativePath = RelativePath(true, *relativePath.segments.drop(1).toTypedArray())
            }
            includeEmptyDirs = false
            into(voskModelDir)
        }
        File(voskModelDir, "uuid").writeText("vosk-model-small-en-us-0.15\n")
        zip.delete()
    }
}
tasks.named("preBuild") { dependsOn(downloadVoskModel) }
