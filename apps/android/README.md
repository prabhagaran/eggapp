# eggAPP Android (field app)

Owner: android-architect. Kotlin + Jetpack Compose, min SDK 26.

## Status (first increment, 2026-07-18)

Built and verified for real — Gradle build, install on an emulator, and a
live network call against the deployed API were all actually exercised,
not just written:

- **Login** (US-USR-001): email/password → JWT, stored via
  `EncryptedSharedPreferences` (Keystore-backed, per the security-devops
  review item in `agents/android-architect.md`).
- **Incubators list**: live temp/humidity per incubator, polling every
  15s (matches `apps/web`'s cadence — telemetry itself lands every ~60s,
  see `docs/iot/telemetry-contract.md`).
- Retrofit client with an `Authenticator` that refreshes the access
  token once on a 401 and retries, mirroring `apps/web/lib/api.ts`.

**Not built yet** (this was the first slice, not full P1 Android scope):
offline-first field entry (candling/hatch/collection — needs Room +
WorkManager sync per BR-010), BLE device provisioning (blocked on the
firmware — see `docs/iot/device-lifecycle.md`, BLE isn't implemented
there yet either), FCM push notifications (needs a Firebase project —
external setup, not started).

## Toolchain used to build/verify this (none of it required a separate install)

- **JDK**: Android Studio's bundled JBR (`Android Studio/jbr`, OpenJDK 21)
- **Android SDK**: already present at `%LOCALAPPDATA%\Android\Sdk`
- **Gradle**: portable 8.9 distribution (no admin rights needed — see
  below if setting up a new machine)

```
export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
./gradlew assembleDebug
```

`local.properties` (gitignored) needs `sdk.dir=` pointing at the SDK —
**use forward slashes**, not `\\`-escaped backslashes; a single backslash
in that file is a Java properties escape character and silently mangles
the path (`Invalid file path` from AGP's `SdkLocator`, caught the hard
way while first setting this up).

If Gradle itself isn't installed and `choco`/admin rights aren't
available: download the binary distribution directly —
`https://services.gradle.org/distributions/gradle-8.9-bin.zip` — extract
anywhere, then `gradle wrapper --gradle-version 8.9` from this directory
generates `gradlew` properly (do this once; `gradlew` is what's tracked
in the repo and used afterward).

## Configuration

`API_BASE_URL` is set in `app/build.gradle.kts` (`buildConfigField`) —
currently hardcoded to the Radxa's LAN IP (`http://192.168.1.44:3001/`),
matching how the firmware points at the broker. Revisit if this ever
needs to be configurable per-build (e.g. a release variant hitting a
different host).

## Package structure

```
com.eggapp.field/
  MainActivity.kt        NavHost: login -> incubators
  data/                  Retrofit API client, DTOs, TokenStore
  ui/login/               Login screen + ViewModel
  ui/incubators/          Incubator list screen + ViewModel
  ui/theme/               Compose theme (brand color matches apps/web)
```
