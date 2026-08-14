# Coop monitor board

Sensor-only ESP32 node for the bird house. Runs
[coop_monitor_v1](../../apps/firmware/coop_monitor_v1/): reads whatever sensors
are fitted, publishes `profile:"COOP"` telemetry every 60 s, and does nothing
else.

Device ID `COOP_01`. Firmware 1.0.0. Defined by
[ADR 0009](../../docs/architecture/adr/0009-coop-monitoring-devices.md).

## State

**No schematic, no layout, nothing built.** This folder is the design's
starting point.

The firmware currently runs in simulation mode
(`SIMULATE_SENSORS 1`,
[config.h:97](../../apps/firmware/coop_monitor_v1/config.h#L97)), publishing
invented readings so the pipeline could be brought up before hardware existed.
That is the state this board is meant to end.

## What makes this board different

It drives nothing. No relays, no mains switching, no control loop, no display.
Per ADR 0009 the coop node is deliberately incapable of energising anything —
so **do not** design it as the incubator board with the relay section
depopulated. A board that could be repopulated into an actuator is a board that
eventually will be.

That leaves a genuinely simple design: module, power, sensor connectors,
protection.

## Sensors

Every channel is optional and compiled out when absent
([config.h:99-104](../../apps/firmware/coop_monitor_v1/config.h#L99-L104)). The
board should mirror that: **each sensor on its own connector, with the node
fully functional when only the DHT22 is fitted.**

| Sensor | Interface | Firmware default | Note |
|---|---|---|---|
| DHT22 | 1-wire digital, GPIO4 | `HAS_TEMP_HUM 1` | The one sensor assumed present |
| MH-Z19B CO₂ | UART2, GPIO16/17 | off | 5 V supply, 3.3 V logic |
| MQ-137 NH₃ | Analog, GPIO35 | off | 5 V heater, needs long burn-in |
| BH1750 lux | I2C, GPIO21/22 | off | 3.3 V native |
| HC-SR04 feed level | GPIO25/33 | off | 5 V part — echo needs level shifting |
| HC-SR04 water level | GPIO32/34 | off | Same |

Full detail in [docs/pin-map.md](docs/pin-map.md).

## Design docs

- [docs/block-diagram.md](docs/block-diagram.md)
- [docs/pin-map.md](docs/pin-map.md)
- [docs/power-budget.md](docs/power-budget.md)

## Blocking decisions

1. **Power source.** Mains adapter, PoE, or battery + solar. This is the
   largest open question and it changes the whole board. A coop is often far
   from an outlet, but the firmware never sleeps — it holds a 60 s MQTT
   keepalive and publishes every 60 s
   ([config.h:69-71](../../apps/firmware/coop_monitor_v1/config.h#L69-L71)) —
   so a battery build is not viable without a firmware sleep strategy first.
   Decide power before layout; do not assume battery will "work out".
2. **HC-SR04 5 V echo.** The echo pin outputs 5 V into a 3.3 V GPIO. GPIO34 is
   input-only and has no clamp. A divider or level shifter per channel is
   required, not optional.
3. **MQ-137 heater.** It runs a continuous 5 V heater — it is the dominant
   load on this board and it rules out most low-power designs. It also needs a
   burn-in and an Rs/R0 calibration the firmware does not currently implement
   (the conversion is a linear placeholder, per the firmware README).
4. **Enclosure and ingress rating.** A coop is dusty, humid, ammoniac and
   full of birds. The sensors must see the air while the electronics do not —
   which is an enclosure problem, not a PCB problem, and it should be settled
   before connector placement.

## Environment

Worth stating plainly, because it drives part selection more than the circuit
does: high humidity, high ammonia, dust, and temperature swings. Ammonia
attacks copper and silver. Conformal coating on the populated board and a
sealed enclosure with a breathable sensor path are the baseline, not an
upgrade.
