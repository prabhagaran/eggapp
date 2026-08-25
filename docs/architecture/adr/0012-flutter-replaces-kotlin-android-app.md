# ADR 0012 — Flutter replaces the Kotlin/Compose Android app

- **Date**: 2026-08-23
- **Status**: Accepted
- **Supersedes**: the implicit "Android is Kotlin + Jetpack Compose" decision
  recorded in `apps/android/README.md` (first increment, 2026-07-18)
- **Owner**: android-architect

## Context

The field app existed as a Kotlin + Jetpack Compose application: 42 files,
~7,450 lines, built and hardware-verified across six increments (login,
incubator telemetry, offline candling/hatch, FCM push, egg collections,
remote setpoints, flock operations). It worked, and its offline-first
behaviour had been verified against a real emulator with the radios genuinely
disabled — not simulated.

Two things drove the change:

1. **iOS is now in scope.** The Kotlin app could only ever serve Android.
   Reaching iOS meant either a second native codebase or a cross-platform
   rewrite. A single Flutter codebase serving both is the reason to accept the
   cost of a rewrite at all — if the product were staying Android-only, this
   ADR would not be worth writing.
2. **One UI codebase for two mobile targets** keeps the surface split in
   CLAUDE.md honest. The field surface is one product decision; it should not
   fork into two implementations that drift.

## Decision

Replace `apps/android` with a Flutter application targeting **Android and
iOS**. The Kotlin app is deleted rather than kept alongside — permanent
coexistence would mean building every field feature twice for one audience.

Identity is preserved deliberately: `applicationId` and `namespace` stay
`com.eggapp.field`, because the existing Firebase Android app registration
(`google-services.json`) is bound to that package name. Changing it would
have meant re-registering the app and re-issuing push credentials for no
benefit.

### What does not change

- **The API contract.** Flutter consumes exactly the same endpoints, with the
  same `clientId` idempotency rule (BR-010) and the same wire field names.
  `lib/data/models.dart` mirrors `docs/api/openapi.yaml` rather than adapting
  it. No backend change was required.
- **The MQTT/device contract** is untouched. Per CLAUDE.md's escalation rule
  it stays authoritative, and this rewrite adapts to it.
- **The surface split.** Coop admin, device management, reporting and team
  settings remain web-only; the phone shows their state, not their
  administration.
- **The security posture.** Tokens move from `EncryptedSharedPreferences` to
  `flutter_secure_storage`, which is Keystore-backed on Android and Keychain-
  backed on iOS — the same guarantee, now on both platforms.

## Consequences

### Accepted costs

- **There is no working field app until parity is reached.** This was chosen
  knowingly over keeping the Kotlin app alive during the transition. The
  Kotlin source remains in git history (last commit `6f94dda`) and is
  recoverable if that judgement turns out wrong.
- **Offline-first must be rebuilt, not ported.** Room + WorkManager have no
  direct Flutter equivalent; Phase 2 rebuilds the 7-entity queue on Drift with
  a background sync worker. The Kotlin README records that this layer needed
  real on-device debugging to get right — that verification has to be redone,
  not assumed.
- **iOS builds need a Mac.** The Windows machine this repo is developed on can
  compile and run the Android target only. iOS is configured but unverified
  until it is built on Apple hardware.

### Phased delivery

| Phase | Scope | Status |
|---|---|---|
| 1 | Auth, secure token storage, API client with refresh-on-401, live read screens (incubators, batches, collections, flocks, alerts, profile) | **Done** |
| 2 | Drift offline queue (candling, hatch, collections, mortality, vaccination, feed, water) + background sync | Not started |
| 3 | FCM push, notification channel, `POST /v1/me/push-token` registration | Not started |
| 4 | Setpoint control, remaining write parity | Not started |

Phase 1 deliberately ships **read-only**. Every write in the Kotlin app went
through a local record first, and shipping a write path that skips the queue
would teach field workers a behaviour Phase 2 then takes away.

## Incidental fix carried over

The Kotlin app never displayed an incubation day counter. The Flutter app
does, and it uses the corrected calendar-day, 1-based calculation
(`lib/core/incubation.dart`) that matches the firmware's
`calcIncubationDay()`, rather than the elapsed-milliseconds formula that the
web dashboard used until 2026-08-23. `apps/web/lib/useAuthedFarm.ts` was
fixed in the same change so all three surfaces count the same way.

Note that a separate, real defect remains open in the firmware: the RTC is
written in IST (`NTP_UTC_OFFSET_SEC = 19800`) but read back as a UTC epoch, so
the device's clock runs 5.5 h fast and its milestone triggers (lockdown,
candling) fire early. That is a firmware change and is explicitly **not**
addressed here.
