# Feature matrix — web dashboard vs. field app

What each client surface can actually do today, taken from the code rather
than from the roadmap. Last verified 2026-08-23.

- **Web dashboard** — `apps/web`, Next.js. The full admin surface.
- **Field app** — `apps/android`, Flutter (Android + iOS). Phase 1, **read-only**.

The two are not meant to converge. Coop admin, device binding, inventory,
reports and team management are manager workflows and stay web-only per the
surface split in `CLAUDE.md` — they were never in the Kotlin app the Flutter
one replaced either. The field app's target is the five field workflows plus
write-back, not the web app's eleven tabs.

## Web dashboard

| Area | What it does |
|---|---|
| **Dashboard** | Active batches with day count, live incubator tiles, 24h environment history charts, coop tiles, open alerts |
| **Coops** | Create / edit / delete; sensor tiles for temperature, humidity, CO₂, ammonia, light, feed level, water level |
| **Flocks** | List + create (from a hatch or from acquisition); detail records mortality/cull/sale, vaccination, feed logs, water logs, stage override, delete |
| **Batches** | List + create from egg collections; detail runs the whole lifecycle — candling, hatch, set date, lockdown, close, abort, edit, delete |
| **Incubators** | List, create, edit; detail has telemetry history charts, setpoint control with actuator overrides, and config ack tracking (`sent → received → applied`) |
| **Collections** | Record, list, discard with reason, assign to flock |
| **Alerts** | List + acknowledge |
| **Devices** | Register, bind/unbind to an incubator or coop, decommission |
| **Vaccination templates** | Seed defaults, create, edit, delete |
| **Inventory** | Create, edit, adjust stock (ledger-tracked), delete |
| **Reports** | Four report views — hatch performance, environmental, vaccination compliance, mortality trends — each with CSV export |
| **Team** | Members list, invite, remove |

Plus login and a first-run setup wizard (`/setup`, refuses once any user
exists).

## Field app (Flutter, Phase 1)

| Area | What it does |
|---|---|
| **Incubators** | Live temperature/humidity, device status pill, relay states, 15s poll |
| **Batches** | List with incubation day; detail shows metrics, schedule, egg sources, and the device day cross-check |
| **Collections** | List with BR-011 storage-age bands (green / amber / red) |
| **Flocks** | List; detail shows vaccination compliance, mortality history, recent feed & water |
| **Alerts** | List with severity and state |
| **Profile** | Identity, farm switcher, sign out |

Plus login with Keystore/Keychain token storage and refresh-once-on-401.

## The gap

The field app is **read-only right now** — everything it shows, it cannot yet
change. These worked in the Kotlin app and do not exist yet in Flutter:

- Candling and hatch recording
- Egg collection recording and discard
- Mortality, vaccination, feed and water logging
- Setpoint control
- Push notifications
- The offline queue that made all of the above safe to use in a shed with no
  signal

They are staged as Phases 2–4 in
[ADR 0012](../architecture/adr/0012-flutter-replaces-kotlin-android-app.md).
Until they land, the field app is a viewer.

## Side by side

| Capability | Web | Field app |
|---|:---:|:---:|
| View incubator telemetry | ✅ | ✅ |
| Telemetry history charts | ✅ | ❌ |
| Setpoint / actuator control | ✅ | ⏳ Phase 4 |
| View batches + schedule | ✅ | ✅ |
| Batch lifecycle (set, lockdown, close, abort) | ✅ | ❌ web-only |
| Record candling / hatch | ✅ | ⏳ Phase 2 |
| View egg collections | ✅ | ✅ |
| Record / discard collections | ✅ | ⏳ Phase 2 |
| Assign collections to a batch | ✅ | ❌ web-only |
| View flocks + compliance | ✅ | ✅ |
| Record mortality / vaccination / feed / water | ✅ | ⏳ Phase 2 |
| View alerts | ✅ | ✅ |
| Acknowledge alerts | ✅ | ❌ |
| Push notifications | ❌ | ⏳ Phase 3 |
| Offline capture | ❌ | ⏳ Phase 2 |
| Coops, devices, inventory, reports, team | ✅ | ❌ web-only by design |

`❌ web-only` means a deliberate surface-split decision, not a missing
feature. `⏳` means planned for the phase named.
