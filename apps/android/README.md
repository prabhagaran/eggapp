# eggAPP Android (field app) — placeholder

Owner: android-architect. The Gradle project is **not** hand-scaffolded here:
this machine has no JDK/Android SDK to verify a build, and unverified
boilerplate is worse than none. In Phase 1, generate it with the Android
Studio wizard:

- New Project → "Empty Activity" (Compose) → project location **this folder**
- Package: `com.eggapp.field` · Language: Kotlin · Min SDK: 26
- Then add per `agents/android-architect.md`: Room (offline queue), Retrofit
  (API), WorkManager (sync), FCM (push), BLE (provisioning per
  `docs/iot/ble-pairing-protocol.md`)

Gradle builds independently of the pnpm workspace (sibling by convention —
see `docs/architecture/system-architecture.md`). CI gains an Android job
when this project exists.
