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
either).

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

## Status (fourth increment, 2026-07-18): egg collection recording (US-EGG-001/004)

Closes the last unbuilt P1 Android gap (besides BLE, which stays
blocked on the firmware) — same offline-first pattern as candling/hatch,
reusing the already-shipped backend (`POST /farms/:farmId/collections`,
clientId-idempotent per BR-010; `POST .../collections/:id/discard`).

- **`ui/collections/`**: a farm-level screen (not tied to a batch, since
  collection→batch assignment is a web-only workflow, US-EGG-003) with
  a record-collection form and a live list. New `CollectionEntity`
  (Room, `pending_collection` table) queues saves offline exactly like
  `CandlingEntity`/`HatchEntity`; `SyncWorker` gained a third push loop.
  Bumped `AppDatabase` to version 2 with `fallbackToDestructiveMigration()`
  — no released version to preserve queued rows across.
- **Discard is online-only, deliberately**: unlike create, the discard
  endpoint has no `clientId` idempotency (no per-discard audit row to
  key off — counts are aggregated in place). Queuing it for
  retry-on-reconnect risks a retried request double-deducting eggs, so
  `CollectionsViewModel.discard()` calls the API directly and surfaces
  failure instead. Discarding normally happens while reviewing a
  freshly-fetched list anyway, so this isn't a real gap in practice.

**Verified for real, full chain, on the emulator**: isolated test user
(`collection-qa@test.local`) logged in through the actual UI. Disabled
the emulator's wifi/data radios, confirmed via `ping` the network was
actually unreachable, recorded a 40-egg collection through the real
form — it queued locally (`"queued"`, visible immediately, zero
connectivity). Re-enabled connectivity; WorkManager synced it
automatically within ~15s, no manual action or restart, confirmed via
OkHttp logs (`201 Created`) and a direct Postgres check
(`count: 40`, correct `clientId`). Re-entering the screen showed the
server-computed `availableCount`/`assignedCount`. Then discarded 3 eggs
with a reason through the real dialog — confirmed via OkHttp (`200 OK`)
and the UI updating to `available 37 · discarded 3`. Test farm/user
cleaned up by exact ID afterward; real account confirmed untouched.

## Status (fifth increment, 2026-07-18): remote setpoint control (US-INC-003, Android)

Extends the web-only setpoint control (see the backend/web increment
notes) to the field app — reuses the same `POST`/`GET
.../incubators/:id/config` endpoints, no backend changes needed.

- **`ui/incubators/SetpointsScreen.kt` + `SetpointsViewModel.kt`**:
  reachable via a "Setpoints" button on each incubator card (only shown
  when a device is bound). Fetches the incubator by id (same idiom as
  `BatchDetailViewModel` — not passed the full object through nav args),
  prefills temp/humidity setpoint + hysteresis from the device's latest
  telemetry snapshot, and polls `GET .../config` every 3s while the ack
  state is `sent` or `received`, stopping once it reaches `applied` or
  `unconfirmed`.

**Verified for real, full chain, on the emulator**: isolated test
incubator/device, logged in through the real UI, confirmed the form
prefilled with the device's actual current values (37.5/0.3/60.0/3.0),
changed the temp setpoint and submitted — confirmed via OkHttp logs the
real `POST .../config` call and the resulting `DeviceConfig` row.
Published fake `received` then `applied` acks via `mosquitto_pub` (same
technique used for the backend/web verification) and watched the
screen's live polling correctly progress through **"v1: sent — waiting
for device"** → **"v1: received by device — applying"** → **"v1:
applied ✓"** with no manual refresh. Test farm/device cleaned up by
exact ID afterward; real account confirmed untouched.

## Status (sixth increment, 2026-07-18): Flock Operations — Phase 2 (US-FLK/US-VAC/US-FED/US-WTR)

Extends the field app to the full Flock Operations domain: flock
tracking, mortality/cull/sale, vaccination, and feed/water checks — all
offline-first, same Room + WorkManager pattern as candling/hatch/
collections.

- **`ui/flocks/FlocksScreen.kt` + `FlocksViewModel.kt`**: farm-level
  flock list (mirrors `BatchesScreen`) showing species/purpose/derived
  stage/current count/age.
- **`ui/flocks/FlockDetailScreen.kt` + `FlockDetailViewModel.kt`**: one
  screen combining four offline-capable forms — mortality/cull/sale
  (BR-009 ledger), vaccination (with a compliance hint for the next
  due/overdue schedule item), and feed/water checks — each backed by
  its own Room entity (`MortalityEntity`/`VaccinationEntity`/
  `FeedLogEntity`/`WaterLogEntity`, `AppDatabase` bumped 2→3,
  `fallbackToDestructiveMigration()`) and `SyncWorker` push loop.
  Server data (flock detail + vaccination compliance) and the four
  local Flow sources are combined with a small hand-rolled `Quad<A,B,C,D>`
  (Kotlin has no built-in 4-tuple) so locally-queued records show up
  immediately regardless of sync state, same as `BatchDetailViewModel`.

**Verified for real, full chain, on the emulator**: logged into the
isolated Phase 2 test farm/flock through the actual login UI, opened
the flock list and detail screens and confirmed the server-computed
metrics (`47 birds · 79 days old · Grower`), the mortality history
table, the vaccination compliance table (5 items correctly `overdue`,
1 `administered`, 1 `upcoming`), and the feed/water table with a
`stage mismatch` badge — all rendered from real data, not fixtures.
Disabled the emulator's wifi/data radios, recorded a feed check through
the real form — it queued locally (`"queued"`, visible immediately in
the "Not yet synced" section, zero connectivity). Re-enabled
connectivity; `SyncWorker` synced it automatically within ~10s, no
manual action or restart, confirmed both by the UI flipping to
`"synced"` and independently via a direct API query against the live
backend showing the exact `feedType`/`quantityKg` submitted offline.
Test farm/user cleaned up by exact ID afterward; real account
confirmed untouched.

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
points at the Radxa's **Tailscale** address (`http://100.92.177.99:3001/`),
not its LAN IP. A Tailscale address is reachable whether the phone is on
home WiFi or anywhere else with internet, as long as Tailscale is
connected on both ends (Radxa + phone) — no separate home/away config.
Revisit if this ever needs to be configurable per-build (e.g. a release
variant hitting a different host).

## Package structure

```
com.eggapp.field/
  MainActivity.kt        NavHost: login -> incubators -> batches/collections/flocks -> detail screens
  push/                  EggAppMessagingService (FCM receive + display)
  data/                  Retrofit API client, DTOs, TokenStore, BatchCache,
                         FieldRecordRepository (offline-first saves)
  data/local/            Room: CandlingEntity/HatchEntity/CollectionEntity/
                         MortalityEntity/VaccinationEntity/FeedLogEntity/WaterLogEntity, DAO, Database
  sync/                  SyncWorker (WorkManager)
  ui/login/               Login screen + ViewModel
  ui/incubators/          Incubator list + setpoint control screens/ViewModels
  ui/batch/               Batches list, batch detail + candling/hatch forms
  ui/collections/         Egg collection recording + discard
  ui/flocks/              Flock list, flock detail + mortality/vaccination/feed/water forms
  ui/theme/               Compose theme (brand color matches apps/web)
```

Kapt (Room's annotation processor) is used instead of KSP — avoids a
second Kotlin-version-matched plugin to keep in sync; falls back to
Kotlin 1.9 language mode for the annotation-processing step only (a
harmless warning, not an error), since kapt doesn't yet support Kotlin
2.0's language version.
