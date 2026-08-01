# Incubator controller board

The control board for the egg incubator. Runs
[egg_incubator_v2](../../apps/firmware/egg_incubator_v2/) on an
ESP32-WROOM-32E-N4: reads temperature and humidity, drives six actuators in a
closed loop, shows status on a local OLED, and publishes telemetry over MQTT.

Device ID `INCUBATOR_01`. Firmware 2.0.0.

## State

**A module breakout sheet exists. The controller schematic does not. The PCB
file is empty. Nothing fabricated.**

The live Altium project is [../eggubator/](../eggubator/) —
`eggubator.PrjPcb`, with `MCU.SchDoc`, `eggubator.SchLib`, `eggubator.PcbLib`
and `eggubator.PcbDoc`.

### What MCU.SchDoc actually contains

Three components: one **ESP32-WROOM-32E-N4** and two 19-pin headers. Every one
of the module's 38 pins is net-labelled with its raw name (`IO34`, `IO35`,
`SD0`, `CMD`, `TXD0`…) and wired straight out to a header pin. Three power
ports: `VCC_3.3V`, `VCC_5V`, `GND`.

It is a **1:1 module breakout** — a carrier you wire the incubator up to. That
is a reasonable first board, but it is not the controller: no sensors, no
relays or drivers, no regulator, no passives. The
[block diagram](docs/block-diagram.md) describes the controller; this sheet
implements the ESP32 block of it and nothing else.

### Four things blocking progress on it

1. **Designators are unannotated** — `IC?`, `P?`, `P?`, with both headers
   sharing `P?`. Altium will not push an unannotated schematic to a board.
   *Tools → Annotate Schematics* is the first step, and it is why item 2 is
   true.
2. **`eggubator.PcbDoc` is empty.** `Components6`, `Nets6`, `Pads6`, `Vias6`,
   `Tracks6`, `Arcs6`, `Texts6`, `Fills6`, `Regions6`, `Polygons6` and
   `Connections6` are all zero-length; only the layer stack, default design
   rules and board region hold data, and `BOARDOUTLINE=FALSE`. No *Update PCB
   Document* has run.
3. **No EN RC circuit and no 3V3 decoupling on the sheet.** Both are required
   by the datasheet's *Peripheral Schematics* section — v2.0's revision history
   specifically records an update to the RC note. Without the EN RC the module
   boots unreliably on a slow-rising supply, which presents as an intermittent
   firmware fault. These belong on the breakout, not on whatever plugs into it.
4. **`VCC_5V` is placed but unsourced.** Nothing on the sheet generates or
   consumes it.

One thing to check rather than fix: `SD0`–`SD3`, `CMD` and `CLK` (GPIO6–11) are
brought out to the headers. That is normal for a breakout, but they are the
internal flash lines and are unusable. Mark them on the silkscreen or the
header invites someone to wire to them.

The working system today is still a devkit with wired modules.

## Design docs

- [docs/block-diagram.md](docs/block-diagram.md) — blocks and interconnect
- [docs/pin-map.md](docs/pin-map.md) — every GPIO, with the firmware line that
  defines it, **and two strapping-pin conflicts that must be fixed before
  layout**
- [docs/power-budget.md](docs/power-budget.md) — rails and loads

## Blocking decisions

These must be settled before layout starts. None of them are layout details —
each changes the schematic.

1. **The GPIO12 / GPIO15 strapping conflict.** `RELAY_PUMP` is on GPIO12
   (MTDI) and `RELAY_TURNER` on GPIO15 (MTDO). An active-LOW relay board's
   pull-up on GPIO12 at reset selects the wrong flash voltage and the module
   does not boot. This is logged as BUG-004 in
   [FIRMWARE_BUG_REVIEW.md](../../apps/firmware/FIRMWARE_BUG_REVIEW.md).
   Designing the board around free pins costs nothing now and cannot be fixed
   later without a spin. See
   [docs/pin-map.md](docs/pin-map.md#strapping-pin-conflicts).
2. **Switching element per channel.** SSR-40DA is on file for the heater. What
   switches the cooler, humidifier, pump and turner — mechanical relays, more
   SSRs, or a mix — is undecided, and it sets the mains section's size and
   creepage.
3. **Mains vs. low-voltage split.** Which loads are mains and which are 12/24 V
   DC. The fan is PWM-driven through a logic-level MOSFET, so it is DC. The
   turner is a slow AC gear motor in most builds. Unconfirmed for the rest.
   This overlaps with the next item — the DC rail voltage is now load-bearing.
4. **What voltage enters the board.** The old block-diagram sheet shows a 12 V
   supply. The 3.3 V regulator is an **LM1117I LDO**, which cannot be fed from
   12 V in any workable layout — at 12 V in it dissipates 2.4 W in a SOT-223 and
   goes into thermal shutdown. If the input is 12 V, a **buck stage to 5 V is
   required** ahead of it. Settle this before schematic work continues:
   [common/3v3-rail-lm1117.md](../common/3v3-rail-lm1117.md#rule-1-feed-it-from-5-v-never-12-v).
5. **Enclosure and panel layout.** Drives connector placement, OLED and button
   positions, and where the SSR heatsink lives.

## Safety

This board carries mains. [../CONVENTIONS.md](../CONVENTIONS.md#mains-safety)
lists the non-negotiable rules — isolation barrier with a routed slot, line
fuse, chassis-bonded earth, no mains near the ESP32 or user-touchable parts.

Two firmware-side safety behaviours the hardware must not undermine:

- **Fail-safe on reset.** Relays are active-LOW (`RELAY_ON == LOW`,
  [config.h:42](../../apps/firmware/egg_incubator_v2/config.h#L42)). During
  reset the ESP32's GPIOs float, so every actuator channel needs a pull-up to
  3.3 V that holds it OFF until firmware drives it. Without that, a watchdog
  reset briefly energises the heater.
- **The heater watchdog assumes the heater can actually be cut.** Firmware
  alarms if the heater is on for 30 minutes without a 0.5 °C rise
  ([config.h:181-182](../../apps/firmware/egg_incubator_v2/config.h#L181-L182)).
  An SSR fails *short* far more often than open. A thermal cutout in series
  with the heater, independent of the ESP32, is the only thing that makes the
  over-temp limit real. Budget for one.
