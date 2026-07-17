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

## Status (second increment, 2026-07-18): offline-first field entry

- **Candling & hatch recording** (US-CAN-002, US-HAT-002): Room-backed
  offline queue (`data/local/`) — saves always succeed locally first,
  regardless of connectivity. A WorkManager `SyncWorker`
  (`NetworkType.CONNECTED` constraint) pushes queued records via the
  same `clientId`-idempotent endpoints the web client uses (BR-010).
  Rejections (e.g. missing discrepancy note) are marked `conflict`
  rather than retried forever; network failures retry with WorkManager's
  backoff.
- **Batch list & detail** (`ui/batch/`): active batches for the farm,
  with the candling form (while `incubating`) or hatch form (while
  `lockdown`/`hatching`) shown inline. The next unrecorded candling day
  is pre-filled from the batch's schedule.
- **Found and fixed during verification, not before**: the batch detail
  screen originally only showed the recording forms once the batch had
  been fetched from the server — meaning the entire offline-recording UI
  vanished when actually offline, exactly backwards for a field app.
  Fixed with `BatchCache` (`data/BatchCache.kt`): the last successfully
  fetched batch is cached locally and shown immediately, with a live
  refresh attempted on top when reachable.

**Verified for real — genuine offline, not simulated**: disabled the
emulator's wifi/data radios (`svc wifi disable` / `svc data disable`;
the `airplane_mode` *setting* alone doesn't cut connectivity in an
emulator), confirmed via `ping` that the network was actually
unreachable, then recorded a candling session through the real UI. It
saved locally and displayed `queued`. Re-enabled connectivity;
WorkManager fired the sync automatically within ~15s with **no manual
action or app restart** — confirmed via OkHttp logs (`201 Created`) and
the UI flipping to `synced`. Then confirmed server-side, independently,
that the exact data landed correctly: `viableCount` 50→45,
`fertilityPct` computed as 90% (BR-015), matching what was entered
offline exactly.

**Not built yet**: BLE device provisioning (blocked on the firmware —
see `docs/iot/device-lifecycle.md`, BLE isn't implemented there yet
either), egg collection recording (same offline pattern as
candling/hatch, just not built this increment).

## Status (third increment, 2026-07-18): push notifications (US-NOT-002)

Alerts now reach the phone as a real push notification through eggAPP
itself — explicitly not a third-party relay (e.g. Telegram); the
requirement was notifications inside the app only.

- **`push/EggAppMessagingService.kt`**: `FirebaseMessagingService` that
  displays incoming pushes as a high-priority notification (channel
  `eggapp_alerts`) deep-linking back into the app, and registers the
  device's FCM token with `POST /v1/me/push-token` — once right after
  login (`LoginViewModel`, since `onNewToken` only fires on rotation,
  not every app start) and again whenever the token rotates.
- **Runtime permission** (`MainActivity`): Android 13+ requires
  `POST_NOTIFICATIONS` consent or pushes arrive but never display;
  requested once on first launch.
- `google-services.json` (gitignored, real Firebase Android app config)
  is required in `app/` for the Google Services Gradle plugin to
  generate the FCM config baked into the APK.

**Verified for real, full chain, on the emulator** (`Pixel_9a`,
`google_apis_playstore` image — needed for real Play Services/FCM):
isolated test user logged in through the actual login UI (screenshots
at each step, not assumed), confirmed server-side via a Prisma query
that the real FCM token registered on login landed in `User.fcmToken`,
then published a genuinely out-of-range MQTT telemetry reading
(`temp: 60`) for a fake test device to the real broker on the Radxa.
Confirmed server-side that this created a real `critical` Alert row,
then confirmed on-device — via the notification shade, not just
logcat — that eggAPP delivered: **"eggAPP • now — 🔴 Critical alert —
Temperature out of range: 60°C"**. Delivery took ~1m45s end-to-end
(MQTT publish → Alert → FCM → device), consistent with normal FCM
latency on an emulator, not an error.

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
  MainActivity.kt        NavHost: login -> incubators -> batches -> batch detail
  data/                  Retrofit API client, DTOs, TokenStore, BatchCache,
                         FieldRecordRepository (offline-first saves)
  data/local/            Room: CandlingEntity/HatchEntity, DAO, Database
  sync/                  SyncWorker (WorkManager)
  ui/login/               Login screen + ViewModel
  ui/incubators/          Incubator list screen + ViewModel
  ui/batch/               Batches list, batch detail + candling/hatch forms
  ui/theme/               Compose theme (brand color matches apps/web)
```

Kapt (Room's annotation processor) is used instead of KSP — avoids a
second Kotlin-version-matched plugin to keep in sync; falls back to
Kotlin 1.9 language mode for the annotation-processing step only (a
harmless warning, not an error), since kapt doesn't yet support Kotlin
2.0's language version.
