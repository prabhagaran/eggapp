# Mosquitto credentials

The `passwd` file is **gitignored** — create it locally before first start.
Per-device credentials (revocable individually, US-DEV-004) plus one account
for the API ingest:

```
docker run --rm -v "$PWD:/work" eclipse-mosquitto:2 \
  mosquitto_passwd -c -b /work/passwd api-ingest '<strong-password>'
docker run --rm -v "$PWD:/work" eclipse-mosquitto:2 \
  mosquitto_passwd -b /work/passwd device-<hardware-id> '<device-password>'
```

(`-c` only on the first command — it creates the file.) Reload after changes:
`docker compose restart mosquitto`. Topic-level ACLs per device are added
when `docs/iot/mqtt-topics.md` exists (needs the real topic scheme).
