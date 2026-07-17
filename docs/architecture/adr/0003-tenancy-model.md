# ADR 0003: Tenancy — farm-scoped schema, single-tenant personal deployment

- **Date:** 2026-07-17
- **Author agent:** chief-product-architect
- **Status:** Accepted

## Context
The system is being built for personal use by a single owner-operator. When
offered a simplified architecture (drop multi-tenancy/RBAC), the owner chose
to keep the current design so the door stays open for broader use later.
The charter requires this decision as an ADR before system-architect begins
bounded-context design.

## Decision
- **Farm is the tenancy boundary.** Every domain table is farm-scoped (per
  database-architect's existing Definition of Done). No Organization layer
  above Farm exists yet.
- **Deployment is single-tenant**: one operator account owning 1..n farms.
  No billing module, no self-service signup — users are created by the Owner.
- **RBAC is retained** with three roles: **Owner** (everything), **Manager**
  (configuration + records, no user/farm administration), **Worker** (record
  field data, view only otherwise).
- Multi-user features (invites, role management) are deprioritized to P3 in
  the PRD, but the schema and auth model support them from day one.

## Consequences
- system-architect models Farm as the top-level scope in the domain model.
- backend-architect enforces farm-membership scoping on every query and RBAC
  at the application-service layer (as its spec already requires).
- If this ever becomes multi-operator SaaS, an Organization entity above Farm
  is introduced via a new ADR superseding this one.
