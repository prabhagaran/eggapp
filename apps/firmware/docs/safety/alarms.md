# Alarms & Faults

!!! info "As-built — describes FW 2.0.0"
    The design-phase alarm FSM (INFO/WARNING/CRITICAL levels, water-level
    alarms) is in the *Design Archive*. This page documents the implemented
    fault and alarm behavior.

## The over-temperature latch (the critical fault)

The one condition that takes over the whole system:

| | Incubator | Climate chamber |
|---|---|---|
| Trip threshold | **39.5 °C** | **80 °C** |

When tripped:

1. `overTempFault` is set under a spinlock and **persisted to NVS**
   (`otFault`) — a power blip cannot silently clear the latch (BUG-010).
2. All relays are forced OFF every control cycle while latched.
3. The UI unconditionally shows the **fault screen** (banner, live temperature,
   "Hold OK 3s to Reset") and discards normal input.
4. Recovery is **manual only**: hold OK for 3 s. The button task clears the
   latch and the NVS flag, forces relays off again, and logs `FAULT_RESET`.

## Sensor-validity gates

- **DS18B20 invalid** (disconnected, power-on-reset 85 °C signature, out of
  −10…100 °C) → heater is forced OFF; control acts only on valid data.
- **DHT22 invalid** → humidifier and pump are forced OFF (BUG-003). Stale
  values are never fabricated at boot; a held value is only served after at
  least one genuine reading.
- During an RTC failure (`rtcEpochValid == false`) all epoch-based logic
  (turner, cyclic phase, ramp, milestones) is suspended rather than allowed to
  underflow (BUG-005); temperature control continues normally.

## Plausibility & runtime alarms (warnings, non-latching)

| Alarm | Condition | Action |
|-------|-----------|--------|
| `SENSOR_WARN` | \|T_DS18B20 − T_DHT22\| > 5 °C for 5 min | throttled error pushed to cloud |
| `HEATER_WARN` | heater ON > 30 min without ≥ 0.5 °C rise | throttled error (stuck relay / failed heater / open door) |
| `NVS_CORRUPT` | out-of-range value found at settings load | value clamped, error pushed (BUG-018) |
| `CLOCK_UNSET` | RTC fell back to compile time | error pushed once (BUG-028) |

## Watchdog

The ESP32 task watchdog (10 s) supervises both control tasks and both sensor
tasks (BUG-001). A hung control task panics and reboots the device — and because
the over-temp latch is NVS-persisted, a latched fault survives that reboot.

## Error reporting path

`pushError(source, message)` → `errorQueue` (depth 20, per-source throttling)
→ drained by the cloud task to the HTTPS endpoint. If the queue overflows, a
dropped-error counter is reported once connectivity returns (BUG-024). Errors
are also always mirrored to the serial log.

## Known gaps (tracked in `FEATURE_ROADMAP.md`)

- No local audible alarm yet (buzzer — roadmap 1.1)
- Non-fault errors are not shown on the OLED, only serial/cloud
- Single temperature sensor remains a SPOF; the DHT22 cross-check warns but
  cannot take over (roadmap 1.2)
