# @eggapp/db — Prisma schema (Supabase Postgres)

Owner: database-architect. Hosting per ADR 0001 (Supabase, managed Postgres only).

## One-time Supabase setup

1. Create a project at supabase.com (free tier is fine — see NFR volumes).
2. Dashboard → Settings → Database → **Connection string**.
3. Copy the repo root `.env.example` to `packages/db/.env` and fill:
   - `DATABASE_URL` — **Transaction pooler**, port 6543, `?pgbouncer=true` — runtime
   - `DIRECT_URL` — **Session pooler**, port 5432 — migrations. Use the
     pooler host here, *not* the "Direct connection" string the dashboard
     also offers (`db.<project-ref>.supabase.co`) — that host is IPv6-only
     and `P1001: Can't reach database server` on networks without IPv6.
   - Percent-encode special characters in your password (`@`→`%40`, etc.)
     or the URL won't parse.
4. From repo root:
   ```
   pnpm --filter @eggapp/db db:deploy   # applies all committed migrations
   pnpm db:seed                         # seeds Species reference data
   ```
   (Day-to-day schema work uses `pnpm db:migrate` / prisma migrate dev.)

## Contents (Phase 1 schema)

- **Farm & Access:** User, Farm, FarmMembership (+ FarmRole) — ADR 0003.
- **Species reference:** incubation parameters from domain-knowledge §1,
  seeded idempotently by `prisma/seed.ts`; drives BR-008 scheduling.
- **Device & Telemetry:** Device, TelemetryReading (append-only),
  DeviceEvent, DeviceConfig (ack states, US-INC-003) — BR-007 binding via
  unique FK.
- **Incubation:** EggCollection, EggBatch (+ BatchStatus lifecycle BR-001),
  BatchEggSource, CandlingSession, HatchEvent — offline-sync `clientId`
  idempotency per BR-010.
- **Alerting:** AlertRule (overrides; species defaults are stage-aware),
  Alert, Notification.

Phase 2 adds the Flock Operations context (Flock, Vaccination, Feed/Water,
Mortality) — see `docs/architecture/domain-model.md`.
