# @eggapp/db — Prisma schema (Supabase Postgres)

Owner: database-architect. Hosting per ADR 0001 (Supabase, managed Postgres only).

## One-time Supabase setup

1. Create a project at supabase.com (free tier is fine — see NFR volumes).
2. Dashboard → Settings → Database → copy both connection strings.
3. Copy the repo root `.env.example` to `packages/db/.env` and fill:
   - `DATABASE_URL` — pooled (port 6543, `?pgbouncer=true`) — runtime
   - `DIRECT_URL` — direct (port 5432) — migrations
4. From repo root:
   ```
   pnpm db:migrate   # creates tables (prisma migrate dev)
   pnpm db:seed      # seeds Species reference data
   ```

## Contents (Phase 0 baseline)

- **Farm & Access context:** User, Farm, FarmMembership (+ FarmRole) — ADR 0003.
- **Species reference:** incubation parameters from domain-knowledge §1,
  seeded idempotently by `prisma/seed.ts`; drives BR-008 scheduling.

Later phases add Incubation, Device & Telemetry, Flock Operations, Alerting,
and Inventory contexts via migrations — see `docs/architecture/domain-model.md`.
