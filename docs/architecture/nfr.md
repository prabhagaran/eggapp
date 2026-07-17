# Non-Functional Requirements

- **Owner agent:** system-architect (with database-architect, iot-integration-architect)
- **Status:** v1 (2026-07-17) — required before capacity-sensitive decisions per `CLAUDE.md`

## Scale assumptions (personal deployment — design headroom in parentheses)

| Dimension | Expected | Design ceiling |
|---|---|---|
| Farms | 1 | 5 |
| Incubators / devices per farm | 1–3 | 20 |
| Telemetry frequency | 1 reading / 30 s / device | 1 / 10 s |
| Telemetry volume | ~3k readings/day/device (~9k/day total) | ~170k/day |
| Concurrent users | 1–2 | 10 |
| Flocks active | 1–5 | 50 |
| Android offline duration | hours–2 days | 7 days queued |

## Retention

- Raw telemetry: **90 days**, then hourly min/max/avg rollups kept **2 years**.
- Batch-window telemetry summaries (per-batch env report): kept with the
  batch indefinitely (computed at batch close).
- Domain records (batches, hatch, vaccination, flock ledger): indefinite;
  ≥12 months after flock disposal is the compliance floor (domain-knowledge §9).
- Notifications/alerts history: 12 months.

**Consequence for database-architect:** at ≤170k readings/day, plain
Postgres on Supabase with a BRIN/btree-indexed append-only table +
scheduled rollup + delete jobs is sufficient. **No separate time-series
store; no partitioning required at expected scale** (revisit only if the
design ceiling is approached).

## Availability & operations

- Best-effort personal service; brief downtime acceptable **except** alert
  delivery during active incubation — the alerting path (broker → ingest →
  FCM) is the availability-critical slice. Device-side: firmware buffers
  during broker outages (verify actual buffer depth in contract discovery).
- Backups: Supabase automated daily + weekly `pg_dump` export kept off-site.
- Observability (security-devops): uptime check on api + broker, missed-
  heartbeat infra alert distinct from domain alerts.

## Performance targets

- Breach → push notification: **< 2 min** end-to-end (PRD metric 2).
- Live status staleness: ≤ 60 s (US-INC-002 / US-ENV-001).
- Android cold start → usable offline: < 3 s on a mid-range phone.
- Full offline queue sync (7 days, ~200 records): < 60 s on reconnect.

## Security baseline (input to threat model)

- Single-tenant but farm-scoped everywhere (ADR 0003) — scoping bugs are
  still access-control bugs.
- Device MQTT auth: per-device credentials, revocable (US-DEV-004);
  BLE pairing auth per security-devops threat-model item (ADR 0002).
- Secrets: Supabase connection string, JWT signing key, FCM service key,
  broker credentials — managed per security-devops strategy, never in repo.
