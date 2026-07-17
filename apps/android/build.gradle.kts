// Root build file — plugin versions declared here, applied per-module.
plugins {
    id("com.android.application") version "8.6.0" apply false
    id("org.jetbrains.kotlin.android") version "2.0.20" apply false
    // Kotlin 2.0+ requires this separate plugin when Compose is enabled —
    // it used to be bundled into the Kotlin compiler itself.
    id("org.jetbrains.kotlin.plugin.compose") version "2.0.20" apply false
}
