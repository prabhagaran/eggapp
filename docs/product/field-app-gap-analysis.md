# Field App FRS — gap analysis and derived requirements

Delta between the Field App Functional Requirements Specification and what
exists in the codebase as of **2026-08-23**. Every gap below was checked
against the code, not inferred from the phase plan; file references are given
so each can be re-verified.

No code was changed in producing this document.

Companion documents: [feature-matrix.md](feature-matrix.md) (what exists
today), [ADR 0012](../architecture/adr/0012-flutter-replaces-kotlin-android-app.md)
(why the field app is Flutter, and its phase plan).

---

## 1. Summary

| FRS section | Status |
|---|---|
| §5 Phase 1 read-only | **Substantially met** — 2 items unverified |
| §6 Authentication | Met, one path unverified |
| §7–14 Phase 2 field capture | **Not started** |
| §8–10 Offline architecture | **Not started** — no local persistence exists |
| §11 Idempotency | Met for 6 of 7 write endpoints; **1 gap blocks §12** |
| §15 Phase 3 push | Not started (client); backend ready |
| §16 Phase 4 control | Not started; **backend audit gap** |
| §19 Non-functional | Partially met; see §6 below |

The single most consequential finding: **§12 (offline discard) cannot be
implemented safely against the current API** — see gap **B1**.

---

## 2. Phase 1 — what is already met

Verified present in `apps/android/lib/`:

- Secure authentication, Keystore/Keychain token storage, farm switching,
  field-oriented navigation (§4.1, §6).
- Incubator name, temperature, humidity, device connection status, relay
  states, ~15s refresh while the screen is active (§5.1).
- Batch list, incubation day, details, metrics, schedule, egg sources,
  device-reported day, and the calculated-vs-device discrepancy display
  (§5.2).
- Collection list, quantity, age, and BR-011 green/amber/red classification
  (§5.3).
- Flock list, details, vaccination compliance, mortality history, recent feed
  and water (§5.4).
- Alert list with severity and state; acknowledgement correctly absent (§5.5).
- Profile: identity, farm selection and switching, sign-out (§5.6).
- No Phase 2/3/4 write operation is exposed (§20).

### Gap A1 — Token refresh is unverified (§6, §20)

The refresh-once-on-401 flow is implemented in
[api_client.dart](../../apps/android/lib/data/api_client.dart), including a
guard against refresh loops and a single retry of the original request. It has
**never been exercised against a genuinely expired token**. §20 lists "Token
refresh works after an expired token" as an acceptance criterion, so Phase 1
cannot be formally signed off until this is tested.

**Requirement:** verify refresh against a real expired access token before
declaring Phase 1 complete.

### Gap A2 — iOS is unverified (§1)

§1 requires Android **and** iOS. The iOS target is configured but has never
been built, signed or run — it needs a Mac, which the current development
machine is not.

**Requirement:** build and verify the iOS target, including Keychain token
storage (§6) and, later, APNs for §15. Until then, iOS support is a claim
rather than a verified capability.

---

## 3. Backend gaps (these block Field App phases)

These are **API and schema** gaps. They are listed first because Phase 2
cannot be built correctly until they are resolved.

### Gap B1 — Collection discard has no idempotency key (§11 + §12) — BLOCKING

§12 requires discard to be available offline. §11 requires **all** Field App
write APIs to be idempotent. The discard endpoint satisfies neither:

```ts
// apps/api/src/routes/v1/collections.ts:18-21
const discardSchema = z.object({
  count:  z.number().int().positive(),
  reason: z.string().min(1).max(300),
});
```

No `clientId`, unlike the six other field-write endpoints. The underlying
reason is structural: discards mutate `EggCollection.discardedCount` **in
place** and create no per-discard audit row, so there is nothing to key an
idempotency check against. A queued discard replayed after a network timeout
would deduct the eggs twice.

This is a known issue, not a new discovery — the previous Kotlin app made
discard deliberately online-only for exactly this reason. The new FRS reverses
that decision, so the backend must change to support it.

**Requirement:** introduce a per-discard ledger row carrying a unique
`clientId`, and derive `discardedCount` from it. Then accept `clientId` on the
discard endpoint. Owner: database-architect + backend-architect.

### Gap B2 — Device control operations have no actor (§16, §19.7)

§16 requires every control operation to be auditable; §19.7 requires knowing
*who* performed an operation. `DeviceConfig`
(`packages/db/prisma/schema.prisma:249`) records `version`, `payload`,
`state`, `sentAt`, `receivedAt`, `appliedAt` — and **no user reference**.
There is currently no way to answer "who changed this setpoint".

**Requirement:** add an actor field to `DeviceConfig` and populate it on every
setpoint/actuator write. Blocking for Phase 4.

### Gap B3 — No record of originating client/device (§19.7)

§19.7 requires knowing "from which client/device it originated". Six of the
seven field-record models carry `recordedById` (who) and `createdAt` (when),
but **no model records the originating client**. A record captured on a phone
is indistinguishable from one entered on the web dashboard.

**Requirement:** add an origin/source field to field-record models, set by the
client on write.

### Gap B4 — `EggCollection` has no `recordedById` (§19.7)

`CandlingSession`, `HatchEvent`, `MortalityRecord`, `VaccinationRecord`,
`FeedLog` and `WaterLog` all carry `recordedById`. `EggCollection`
(`schema.prisma:276`) does not — it has `clientId` but no user reference. "Who
collected these eggs" is currently unanswerable.

**Requirement:** add `recordedById` to `EggCollection` for consistency with
every other field record.

---

## 4. Phase 2 — offline capture (§7–14) — not started

### Gap C1 — No local persistence of any kind

The app has **no local database**. `flutter_secure_storage` is the only
persistence and it holds solely the auth token and farm id. §8 designates
offline operation a *foundational architectural requirement*; nothing of it
exists yet.

**Requirement:** a local transactional store (Drift is the intended choice per
ADR 0012), written to **before** any network attempt, per the §9 flow.

### Gap C2 — Sync queue metadata is more demanding than the previous implementation

§10 mandates per-transaction metadata that goes beyond what the retired Kotlin
app stored. Newly required fields:

| §10 field | Previously present? |
|---|---|
| Unique operation ID | Yes (`clientId`) |
| Entity ID | Yes |
| Operation type | Partially — implied by table, not stored |
| Creation timestamp | Yes |
| **Local device/user identity** | **No** |
| Payload | Yes |
| Synchronization state | Partially — see C3 |
| **Retry count** | **No** |
| **Last synchronization attempt** | **No** |
| **Error information** | Partially (a `conflict` flag, no detail) |

**Requirement:** the sync queue schema must carry all ten fields.

### Gap C3 — Sync state machine must include `Syncing`

§10 requires **Pending → Syncing → Synced → Failed**. The Kotlin app used
`queued` / `synced` / `conflict` with no in-flight state. An explicit
`Syncing` state is needed both for §19.6 user visibility and to prevent a
second worker picking up an in-flight operation.

**Requirement:** implement the four-state machine exactly as specified.

### Gap C4 — No sync-status visibility to the user (§19.6)

§19.6 requires users to determine whether transactions are pending,
synchronizing, synced or failed. No such UI exists.

**Requirement:** per-record sync status indication, plus a way to see
outstanding and failed items.

### Gap C5 — Automatic retry on connectivity restoration (§10, §19.6)

No background sync worker exists. §10 requires automatic retry when
connectivity returns; §19.6 requires synchronization to be automatic.

**Requirement:** a background sync worker triggered by connectivity change,
with backoff, honouring the retry-count and last-attempt fields from C2.

### Gap C6 — All eight write workflows (§7, §12–14)

None implemented: candling, hatch, egg collection recording, collection
discard, mortality, vaccination, feed, water. Each must satisfy the §12–14
field lists (e.g. mortality requires flock, quantity, reason, timestamp,
notes) and each must work offline.

### Gap C7 — Batch eligibility validation (§13)

§13 requires validating that the selected batch is eligible for candling or
hatch. Offline, the client cannot consult the server, so this must be
decidable from cached batch state — with the backend re-validating on sync
(§21: business rules belong to the backend).

**Requirement:** define eligibility rules evaluable client-side offline, and
define the reconciliation behaviour when the server later rejects a queued
record.

### Gap C8 — Offline read access is unspecified but practically required

§8 covers *write* operations. §19.5 requires the app to handle offline
operation gracefully. Today, on a cold start with no connectivity, **every
read screen shows an error** — nothing is cached. A field worker in a shed
would be unable to see which batch is on which incubator, and so could not
choose what to record against.

This is a known failure mode, not a hypothetical: the Kotlin app hit exactly
this and added `BatchCache` after on-device testing showed the recording UI
vanished when offline.

**Requirement (needs a product decision):** cache the last successfully
fetched read data for offline display, with clear staleness indication. The
FRS should state this explicitly rather than leaving it implied by §19.5.

---

## 5. Phases 3 and 4 — not started

### Gap D1 — Push notifications, client side (§15)

Not implemented in Flutter. The **backend is ready** — it dispatches FCM and
stores `User.fcmToken` — and a valid `google-services.json` bound to
`com.eggapp.field` is in place.

**Requirement:** FCM integration, notification channel, runtime notification
permission (Android 13+), token registration on login and rotation, and APNs
for iOS. §15 also requires that a notification failure never loses the
underlying alert — the server-side alert must remain authoritative.

### Gap D2 — Incubator control (§16)

Not implemented. Requires viewing and modifying setpoints, viewing actuator
states, and authorized overrides.

### Gap D3 — Three-state control distinction (§16)

§16 requires the UI to clearly distinguish **requested**, **device-reported**
and **successfully applied** state, retaining `Sent → Received → Applied`.
This is a stricter presentation requirement than the web dashboard currently
meets — the web shows the ack state but does not separate "what the user
requested" from "what the device reports".

**Requirement:** design this three-way display once and apply it to both
surfaces.

### Gap D4 — Server-side authorization for control (§16, §19.3)

§16 requires that safety-critical operations not rely on client-side
validation, with final authorization at the backend/device layer.

**Requirement:** confirm and, if necessary, strengthen server-side
authorization on the setpoint/config endpoints; pairs with B2 for audit.

---

## 6. Non-functional gaps

### Gap E1 — Rapid data-entry usability (§19.4)

§19.4 requires field operations to be optimized for rapid entry in
agricultural/hatchery environments. No such design work has been done. In
practice this implies large touch targets, glove and wet-hand operability,
daylight-readable contrast, minimal typing (steppers over keyboards), and
sensible defaults such as pre-filling the next candling day.

**Requirement:** a field-entry interaction spec before Phase 2 UI is built.
Owner: ui-ux-architect.

### Gap E2 — Network-transition handling (§19.5)

§19.5 requires graceful handling of slow connectivity and network
transitions. Current behaviour is limited: fixed 15s/20s timeouts, and a
failed background poll deliberately keeps stale data on screen. There is no
handling of transitions (Wi-Fi → cellular → Tailscale re-establishment), which
matters because the API is reachable only over Tailscale.

**Requirement:** define expected behaviour on network transition, including
Tailscale reconnection.

---

## 7. Consolidated new requirements

Ordered by what blocks what.

| # | Requirement | FRS ref | Owner | Blocks |
|---|---|---|---|---|
| 1 | Discard ledger row + `clientId` on discard | §11, §12 | backend/db | Phase 2 discard |
| 2 | Local transactional store (Drift) | §8, §9 | android | All of Phase 2 |
| 3 | Sync queue with all ten §10 metadata fields | §10 | android | Phase 2 |
| 4 | Four-state sync machine incl. `Syncing` | §10 | android | Phase 2, §19.6 |
| 5 | Background sync worker with retry/backoff | §10, §19.6 | android | Phase 2 |
| 6 | Sync-status UI | §19.6 | android/ui-ux | Phase 2 sign-off |
| 7 | Eight offline write workflows | §12–14 | android | Phase 2 |
| 8 | Client-side batch eligibility + server re-validation | §13 | android/backend | Candling, hatch |
| 9 | Offline read cache (needs product decision) | §19.5 | chief-product-architect | Field usability |
| 10 | Field-entry interaction spec | §19.4 | ui-ux | Phase 2 UI |
| 11 | `recordedById` on `EggCollection` | §19.7 | database | Audit |
| 12 | Client/device origin on field records | §19.7 | database/backend | Audit |
| 13 | Actor field on `DeviceConfig` | §16, §19.7 | database/backend | Phase 4 |
| 14 | FCM/APNs client integration | §15 | android | Phase 3 |
| 15 | Setpoint/actuator control UI | §16 | android | Phase 4 |
| 16 | Requested/reported/applied display | §16 | ui-ux | Phase 4 |
| 17 | Server-side control authorization review | §16, §19.3 | security-devops | Phase 4 |
| 18 | Verify token refresh on real expiry | §6, §20 | android/qa | Phase 1 sign-off |
| 19 | Build and verify iOS | §1 | android | iOS support claim |
| 20 | Network-transition behaviour spec | §19.5 | android | Field reliability |

---

## 8. Points needing a product decision

These are not implementation gaps — they are places where the FRS should be
made explicit.

1. **Alert acknowledgement stays web-only** (§5.5, §18). A field worker who
   receives a critical push (§15) and is standing at the incubator cannot
   acknowledge it from the app. Confirm this is intended.
2. **Offline read caching is unstated** (gap C8). §8 covers writes only; the
   FRS should say whether read data is cached for offline viewing.
3. **Telemetry history is excluded** (§18). Confirm a field worker never needs
   to see, for example, the last 24h of temperature while standing at a
   misbehaving incubator.
4. **Roadmap phase numbering collides.** The product roadmap's Phase 1–4
   (`roadmap.md`) and this FRS's Field App Phase 1–4 are different sequences
   with the same names. Recommend renaming the Field App phases (e.g. FA-1 …
   FA-4) to avoid ambiguity in status reporting.

---

## 9. Out of scope — confirmed

FRS §17 lists capabilities that remain Web-Dashboard-only: coop management,
device registration/binding/decommissioning, inventory, reports and CSV
export, team management, batch lifecycle administration, and
collection-to-batch assignment.

These match the surface split already recorded in `CLAUDE.md` and
[feature-matrix.md](feature-matrix.md). They are **not** counted as gaps
anywhere in this document.
