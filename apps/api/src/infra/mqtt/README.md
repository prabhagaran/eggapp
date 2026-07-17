# MQTT ingest adapter (Phase 1)

Inbound adapter subscribing per `docs/iot/mqtt-topics.md` (blocked on
firmware contract discovery — see `docs/iot/firmware-discovery-guide.md`).
Validates against the telemetry contract, then calls the same application
services REST uses. No business rules in this adapter.
