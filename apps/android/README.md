# eggAPP field app (Flutter)

Owner: android-architect. Flutter 3.44 / Dart 3.12, targeting **Android and
iOS**. Replaces the Kotlin + Jetpack Compose app that lived here until
2026-08-23 — see [ADR 0012](../../docs/architecture/adr/0012-flutter-replaces-kotlin-android-app.md)
for why, and what was given up.

The old Kotlin source is not gone, just not here: it is in git history at
commit `6f94dda` under `apps/android/app/src/main/kotlin/`.

## Status: Phase 1 (2026-08-23) — auth and live read screens

Phase 1 is deliberately **read-only**. Every write in the Kotlin app went
through a local record before the network, and shipping a write path that
skips that queue would teach field workers a behaviour Phase 2 then takes
away.

- **Login** — email/password → JWT, stored via `flutter_secure_storage`
  (Android Keystore / iOS Keychain). Same posture as the Kotlin app's
  `EncryptedSharedPreferences`, now on both platforms.
- **API client** (`lib/data/api_client.dart`) — Dio with a bearer-token
  interceptor and a refresh-once-on-401 retry, mirroring `apps/web/lib/api.ts`
  and the Kotlin `RefreshAuthenticator`. Concurrent 401s collapse onto a
  single refresh instead of firing one each.
- **Screens** — incubators (live telemetry, 15s poll), batches + detail,
  collections, flocks + detail with vaccination compliance, alerts, profile
  with farm switcher.
- **Incubation day counter** — 1-based and calendar-day based
  (`lib/core/incubation.dart`), matching the firmware's `calcIncubationDay()`.
  Covered by `test/incubation_test.dart`.

### Verified for real (Pixel_9a emulator, Android 16 / API 36)

`flutter analyze` clean, 11/11 unit tests pass, debug APK builds, installs and
runs as `com.eggapp.field`. Every Phase 1 screen was driven through the real
UI against the **deployed** API over Tailscale — not fixtures, not mocks:

- **Login** with the `android-create@test.local` QA account (see below) →
  landed on Incubators with the farm name resolved into the app bar.
- **Incubators**: the test farm's incubator with no device bound renders
  `no device` and em dashes for temperature/humidity/last-reading — confirming
  null telemetry stays null rather than displaying as `0.0`.
- **Batches**: a `planned` batch with no `setAt` shows day `—`; an
  `incubating` one set 7/12/2026 shows **day 43/21**, which is the correct
  1-based calendar count (42 elapsed days + 1) on a batch left open past its
  hatch date.
- **Batch detail**: metrics, schedule, and egg sources with the source
  collection's BR-011 age chip. The `device day` stat correctly does not
  render when the batch has no `deviceDay`.
- **Collections**: a 35-day-old collection renders its age chip red (BR-011
  `>14`), with counts (20 / 5 / 15 / 0) matching the API exactly.
- **Flocks + detail**: the parallel flock + vaccination-compliance fetch
  resolves into one screen; empty sections state what is missing rather than
  rendering blank.
- **Alerts**: empty state.
- **Profile**: identity, farm radio with role/timezone, API base URL.
- **Token persistence**: `am force-stop` then relaunch went straight back to
  Incubators without a login prompt — and that relaunch was the installed APK
  standalone, not under `flutter run`. Confirms `flutter_secure_storage` is
  really persisting to the Keystore and `Session.restore()` works.

**Still unverified**: the refresh-on-401 path (needs a token to actually
expire in situ), and everything iOS.

## Status: Phase 2, first slice (2026-08-23) — offline candling and hatch

A deliberately narrow vertical slice: the local store, the sync queue, and
**candling and hatch only**. Scoped this way so the offline architecture is
proven against a real batch before six more forms are built on top of it —
and because day-7 candling on the live batch falls on 2026-08-27.

- **Local store** (`lib/data/local/database.dart`) — Drift. A field write is
  committed here before any network attempt (FRS §9), so a save cannot fail
  for lack of connectivity.
- **Sync queue** carrying all ten §10 metadata fields, including the ones the
  retired Kotlin app never stored: device identity, retry count, last attempt,
  and error detail.
- **Four-state machine** — `pending → syncing → synced → failed` (§10).
  `syncing` is a real state, so a second pass cannot pick up an in-flight
  operation.
- **Terminal vs retryable failures** — a transport error retries with backoff
  (30s doubling to 15m); a business-rule rejection (wrong batch status,
  missing discrepancy note) is marked terminal and stops retrying, because no
  amount of retrying will fix it. The user is shown why and can discard it.
- **Sync visibility** (§19.6) — a banner above every tab, a queue screen with
  per-record state and error, and locally-captured records shown on the batch
  they belong to.
- **Field-entry UI** (§19.4) — 48dp stepper targets rather than keyboard
  entry, live BR-003 reconciliation, and day pre-fill that accounts for both
  server-held and locally queued sessions.
- **Eligibility checked offline** (§13, `lib/core/batch_eligibility.dart`) —
  mirrors the server's rules so a user learns at the incubator that a record
  will not be accepted, rather than days later on sync. The server remains
  authoritative and re-validates.

### Verified for real — genuine offline, not simulated

26/26 unit tests pass and `flutter analyze` is clean, but the meaningful proof
was on the emulator against the **deployed** API:

1. Signed in, opened an incubating batch.
2. Disabled the radios with `svc wifi disable` / `svc data disable` and
   confirmed via `ping` that `100.76.190.23` was genuinely **unreachable** —
   the `airplane_mode` setting alone does not cut connectivity in an emulator.
3. Recorded a day-7 candling through the real UI with zero connectivity. It
   saved, said *"Candling saved. It will sync when online"*, and appeared
   under "Captured on this device".
4. Re-enabled connectivity. It synced automatically, with no manual action or
   app restart.
5. Confirmed **server-side, independently** via a direct database query:
   `dayNo 7, fertile 8, clear 2`, exactly as entered offline, carrying the
   operation id as its `clientId`, with the batch's `viableCount` correctly
   updated 10 → 8.

**Found and fixed during that run, not before**: after a successful save the
form zeroed the loss counts but left `fertile` at the submitted value, so it
immediately displayed a false "counts don't balance" warning about a record
that had saved correctly. The form now resets to the same balanced default it
opened with.

### Not built yet

Remaining Phase 2 workflows (egg collection recording and discard, mortality,
vaccination, feed, water), Phase 3 (push), Phase 4 (setpoint control) — see
[the gap analysis](../../docs/product/field-app-gap-analysis.md) for the full
list and [ADR 0012](../../docs/architecture/adr/0012-flutter-replaces-kotlin-android-app.md)
for the phase plan.

Two constraints worth restating: **offline discard is blocked on the backend**
— the discard endpoint has no idempotency key, so a replayed discard would
double-deduct eggs (gap B1). And the sync worker runs **in-process**, not as
an OS background job: a queued record syncs the next time the app is open and
connected, which covers the field case but is not a true background worker.

BLE device provisioning remains blocked on the firmware, as it was for the
Kotlin app.

## Configuration

`API_BASE_URL` defaults to nila's **Tailscale** address
(`http://100.76.190.23:3001/`), not its LAN IP — reachable from home WiFi or
anywhere else, as long as Tailscale is connected on both ends. Override per
run without editing the file:

```
flutter run --dart-define=API_BASE_URL=http://10.0.2.2:3001/
```

`10.0.2.2` is what an Android emulator needs to reach an API running on the
development machine itself.

## Build

```
export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
flutter pub get
flutter analyze
flutter test
flutter run -d emulator-5554
```

Two files are gitignored and must exist locally:

- `android/app/google-services.json` — the real Firebase Android config.
  Bound to package `com.eggapp.field`, which is why the Flutter app kept that
  exact `applicationId` rather than taking Flutter's default
  `com.eggapp.eggapp_field`. Needed from Phase 3 on.
- `android/local.properties` — `sdk.dir=` pointing at the Android SDK. **Use
  forward slashes**; a single backslash is a Java properties escape character
  and silently mangles the path.

### iOS

The iOS target is configured but **unverified** — it needs a Mac to build and
sign, which the Windows machine this repo is developed on cannot do.

## Test account

A QA account left over from the Kotlin increments still exists on the deployed
instance and is what Phase 1 was verified against:

| | |
|---|---|
| Email | `android-create@test.local` |
| Password | `android-create-pw` |
| Farm | "Android Create QA (test — safe to delete)" |

It is created by [`apps/api/android-create-verify-setup.mts`](../api/android-create-verify-setup.mts),
which is where the password comes from — nothing here is a secret worth
protecting, but note the farm is **live data on the deployed API**, not a
local fixture, so anything written through it is real.

The API refuses `POST /v1/setup` once any user exists, so a fresh test account
cannot be self-provisioned; re-run that script against the database to make
another, and delete it afterward by exact id.

## Package structure

```
lib/
  main.dart              wiring + logged-in/logged-out root
  core/                  config, theme, incubation-day maths, formatting
  data/                  api_client (auth + refresh), api_service, models, token_store
  state/                 Session (identity, active farm)
  ui/components/         StatusPill, AppCard, Stat, AsyncView (load/error/retry/poll)
  ui/login/              login screen
  ui/shell/              bottom-nav shell
  ui/incubators/         live telemetry list
  ui/batches/            batch list + detail (incubation day, device cross-check)
  ui/collections/        egg collections with BR-011 storage-age bands
  ui/flocks/             flock list + detail (mortality, vaccination, feed/water)
  ui/alerts/             alert list
  ui/profile/            identity, farm switcher, sign out
test/
  incubation_test.dart   day-count and device-mismatch rules
```

State management is `provider` with `ChangeNotifier` — the app's shared state
is one `Session` object, and anything heavier would be scaffolding without a
load to carry.
