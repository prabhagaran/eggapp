# MQTT ingest adapter

Inbound adapter subscribing per `docs/iot/mqtt-topics.md` and
`telemetry-contract.md` (captured live from the real firmware — see those
docs). Validates against the contract, then writes via the same Prisma
models the REST layer uses. No business rules live here.

Started only from `src/index.ts` (the real server entrypoint) — never
from `buildApp()` in `src/app.ts`, so the test suite (which calls
`buildApp()` directly, many times) never tries to connect to a real
broker. Opt-in via `MQTT_URL`; unset disables it cleanly (see
`config.ts`), same pattern as the firmware's own opt-in-via-secrets.h.
