# Logging & Telemetry

!!! info "As-built — describes FW 2.0.0"
    The design phase planned SD-card logging; the shipped firmware logs to
    **serial** and to a **cloud endpoint**. Local persistent logging is on the
    roadmap (item 6.1).

## Serial logging

Structured, prefixed debug output (`[UI]`, `[BTN]`, `[CLOUD]`, `[RTC]`,
`[FAULT]`, `[BATCH]`, …) for development and field diagnosis over USB.
Sensitive data is redacted — the cloud token never appears in logs (BUG-021).

## Cloud telemetry (`task_cloud`, Core 0)

| Stream | Interval | Content |
|--------|----------|---------|
| Telemetry | **60 s** | temperature, humidity, setpoints, relay states, profile, mode, incubation day |
| Heartbeat | **5 min** | device ID, FW version, uptime, RSSI |
| Errors | event-driven | drained from `errorQueue` (20 deep, per-source throttled) |

Transport: HTTPS GET to a **Google Apps Script** endpoint, token-authenticated,
root CA pinned via `secrets.h` (gitignored; `secrets.h.example` is the
template — cloud features disable cleanly when absent).

### Reliability behavior

- **Retry queue:** failed sends are buffered in `telemetryQueue` (16 messages)
  and retried with exponential backoff (base 2 s, max 3 retries). Only HTTP
  2xx/3xx counts as success (BUG-022).
- **Offline:** errors queue up while disconnected; if the queue overflows, the
  overflow count is reported as a single `ERR_DROPPED` summary on reconnect
  (BUG-024).
- **No heap churn:** URLs are built in stack buffers (`snprintf`), not `String`
  concatenation — protects a 24/7 device from heap fragmentation (BUG-021).
- All radio work lives on Core 0 and can never block the control tasks.

## What is *not* logged locally

There is no SD-card or flash-filesystem history yet: after a power cycle, only
NVS state (settings, start epoch, fault latch) survives locally — trend data
lives in the cloud sheet only. Local CSV logging is roadmap item 6.1 and
becomes practical after the planned flash repartition.
