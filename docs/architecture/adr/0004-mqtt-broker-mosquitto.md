# ADR 0004: MQTT broker — Eclipse Mosquitto

- **Date:** 2026-07-17
- **Author agent:** iot-integration-architect
- **Status:** Accepted

## Context
The platform needs an MQTT broker between the ESP32 incubators and the API
ingest module. Scale per NFR: ≤20 devices at design ceiling, ~9k messages/day
expected. Candidates: Mosquitto (single-node, minimal), EMQX/HiveMQ
(clustered, dashboards), managed cloud brokers.

## Decision
**Eclipse Mosquitto 2.x**, run via Docker on the deployment host
(`infra/docker/`). Rationale: personal scale needs no clustering; tiny
footprint; per-device username/password auth with individually revocable
credentials (supports US-DEV-004) and topic ACLs; ubiquitous, boring, easy
to replace behind the standard MQTT protocol if scale ever demands it.

## Consequences
- Per-device credentials + one API-ingest account in a passwd file
  (never committed); topic-level ACLs added once the real topic scheme is
  documented from firmware discovery.
- LAN-only deployment initially; **TLS/exposure review is a
  security-devops-engineer threat-model item** before any port forwarding.
- Broker uptime is on the availability-critical alerting path (NFR) —
  observability (security-devops) monitors it, and firmware-side buffering
  during broker outages must be confirmed during contract discovery.
