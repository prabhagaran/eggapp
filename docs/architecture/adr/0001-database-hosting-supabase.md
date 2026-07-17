# ADR 0001: Use Supabase (managed Postgres) for database hosting

- **Date:** 2026-07-17
- **Author agent:** database-architect
- **Status:** Accepted

## Context
The system needs a PostgreSQL database. Rather than self-hosting Postgres or
provisioning a raw cloud instance, a managed Postgres provider removes
operational overhead (backups, patching, connection pooling) for a small team.

## Decision
Use **Supabase** strictly as a managed PostgreSQL host. Only the database is
in scope:

- Schema design, migrations, and access continue to go through **Prisma**,
  exactly as defined in `database-architect.md`.
- Supabase Auth, Storage, and Realtime are **not** adopted. Authentication
  stays JWT/RBAC implemented by **backend-architect**, as already specified.
  File/attachment storage (if needed) is a separate future decision, not
  assumed to be Supabase Storage.
- The application connects via a standard Postgres connection string;
  application code has no dependency on Supabase-specific SDKs or client
  libraries.

## Consequences
- **security-devops-engineer** adds the Supabase connection string / service
  role key to its secret management strategy (`docs/security/threat-model.md`)
  — same treatment as any other production credential.
- No changes to backend-architect's auth ownership, API contract, or any
  other agent's scope.
- If Supabase Auth/Storage/Realtime are adopted later, this ADR is superseded
  by a new one, since that would change backend-architect's and
  security-devops-engineer's ownership boundaries.
