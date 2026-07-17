# User Stories — Users & Auth

**Priority:** P1 (001) / P3 (002–003) · **Surfaces:** Both (login), Web (admin)

## US-USR-001: Login and session [Both, P1]
As the Owner, I want email+password login issuing JWT access/refresh tokens, working identically on Android and web.
- Given valid credentials, when I log in, then I receive a session that survives app restarts (secure storage on Android per security-devops review).
- Given an expired access token with a valid refresh token, then renewal is silent; a revoked/expired refresh token forces re-login.

## US-USR-002: Invite a user to a farm [Web, P3]
As the Owner, I want to invite a member with a role (Manager/Worker) per ADR 0003.
- Given an invite, when accepted, then the user has exactly that role on exactly that farm. Rules: BR-013

## US-USR-003: Role enforcement [System, P1 groundwork]
As the system, I enforce RBAC at the service layer regardless of surface.
- Given a Worker, when they attempt threshold configuration or user admin, then the API refuses (403) even if a client somehow shows the control. Rules: BR-013
