# User Stories — Farms

**Priority:** P1 (001, 003) / P3 (002) · **Surfaces:** Web

## US-FRM-001: First-run farm setup [Web, P1]
As the Owner on a fresh install, I want a setup wizard that creates my account and first farm (name, location, timezone) in one flow, so the system is usable immediately.
- Given an empty system, when setup completes, then I am Owner of the farm and land on its dashboard.

## US-FRM-002: Additional farms [Web, P3]
As the Owner, I want to add further farms and switch between them; all data views are scoped to the selected farm. Rules: BR-013

## US-FRM-003: Farm scoping enforced everywhere [System, P1]
As the system, I scope every query to the requesting user's farm memberships — there is no unscoped data path. Rules: BR-013 · ADR 0003
