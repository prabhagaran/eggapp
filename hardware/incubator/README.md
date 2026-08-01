# Incubator controller board

The control board for the egg incubator. Runs
[egg_incubator_v2](../../apps/firmware/egg_incubator_v2/) on an
ESP32-WROOM-32E-N4: reads temperature and humidity, drives six actuators in a
closed loop, shows status on a local OLED, and publishes telemetry over MQTT.

Device ID `INCUBATOR_01`. Firmware 2.0.0.

## State

**Schematic substantially drawn — 71 components. PCB file still empty. Nothing
fabricated.**

The live Altium project is [../eggubator/](../eggubator/) —
`eggubator.PrjPcb`, with `MCU.SchDoc`, `eggubator.SchLib`, `eggubator.PcbLib`
and `eggubator.PcbDoc`.

### What MCU.SchDoc contains

| Block | Parts |
|---|---|
| MCU | `ESP32-WROOM-32E-N4`, 2× 19-pin header |
| 3.3 V rail | `LM1117IMPX3.3`, 2× 22 µF 0805 |
| **5 V rail** | `LM2596S-ADJ` buck, 330 µH `CDRH104RNP-331NC`, 2× 865060453007 polarised cap, `SS54` Schottky |
| Actuators, ×6 identical channels | `AWHSH112D00G` relay, `APC-817C1-SL` optocoupler, `MMBT2222A-G` NPN, `1N4007` flyback, 1 k + 680 R, red LED |
| Sensors / IO | 4.7 kΩ DS18B20 pull-up, 8× `691137710003` connectors, headers 4/5/7/10 |
| Buttons | 2× `TL1105AF160Q` tactile |
| Terminals | `282837-2`, `282837-5` screw terminals |

Nets use the firmware macro names — `DHT_PIN`, `DS18B20_PIN`, `I2C_SDA`,
`I2C_SCL`, `BTN_UP`/`BTN_DOWN`/`BTN_OK` — which is what
[CONVENTIONS.md](../CONVENTIONS.md#naming) asks for and keeps the sheet
checkable against `config.h`. Power ports: `VIN`, `VCC_5V`, `VCC_3.3V`, `GND`.

The six actuator channels are **opto-isolated** — optocoupler into an NPN into
the relay coil, with a flyback diode and an indicator LED per channel. That
covers all six firmware actuators and resolves the "switching element per
channel" question for everything except the heater, which stays on the
panel-mounted SSR-40DA.

### Still blocking

1. **`eggubator.PcbDoc` is empty.** `Components6`, `Nets6`, `Pads6`, `Vias6`,
   `Tracks6`, `Arcs6`, `Texts6`, `Fills6`, `Regions6`, `Polygons6` and
   `Connections6` are all zero-length; only the layer stack, default design
   rules and board region hold data, and `BOARDOUTLINE=FALSE`. No *Update PCB
   Document* has run. **Annotation is now done (71/71), so this is unblocked** —
   it is the next action.
2. **No EN RC circuit on the sheet.** The ESP32 datasheet's *Peripheral
   Schematics* section requires it — v2.0's revision history specifically
   records an update to the RC note. Without it the module boots unreliably on
   a slow-rising supply, which presents as an intermittent firmware fault. The
   two 100 nF caps just added cover decoupling; the EN RC is a separate
   resistor-plus-capacitor and is still missing.
3. **Two capacitors are unannotated** (`C?`). Re-run *Tools → Annotate
   Schematics* before the PCB import, or they arrive on the board without
   designators.

Two things to verify rather than change: only **two** tactile switches are
placed while the firmware expects three buttons plus a reset/boot pair, so
confirm whether `BTN_UP`/`BTN_DOWN`/`BTN_OK` are intended to arrive on a header
rather than on-board. And `SD0`–`SD3`, `CMD`, `CLK` (GPIO6–11) are still routed
to the 19-pin headers; they are the internal flash lines and unusable, so mark
them on the silkscreen.

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

1. **Actuator drive polarity.** Firmware is active-LOW
   (`RELAY_ON == LOW`,
   [config.h:47](../../apps/firmware/egg_incubator_v2/config.h#L47)), and the
   channels are opto-isolated. Which way the `APC-817C1-SL` LED faces decides
   whether the board matches that or inverts it — and an inverted channel
   energises the heater when firmware means to switch it off. Verify on all six
   channels; this is a hazard, not a nuisance.
2. **Mains vs. low-voltage split.** Which loads are mains and which are DC. The
   fan is PWM-driven through a logic-level MOSFET, so it is DC. The turner is a
   slow AC gear motor in most builds. The relay contacts on the sheet can carry
   either, so this is about creepage and the mains section's size, not part
   choice.
3. **Enclosure and panel layout.** Drives connector placement, OLED and button
   positions, and where the SSR heatsink lives.

### Settled by the current schematic

- **Switching element per channel** — six identical opto-isolated relay
  channels (`AWHSH112D00G` + `APC-817C1-SL` + `MMBT2222A-G`), with the heater
  still on the panel-mounted SSR-40DA.
- **Board input voltage** — an `LM2596S-ADJ` buck now generates 5 V from `VIN`,
  which is what makes the LM1117 workable. It must **not** be bypassed: feeding
  the LM1117 from 12 V directly dissipates 2.4 W in a SOT-223 and lands it in
  thermal shutdown
  ([common/3v3-rail-lm1117.md](../common/3v3-rail-lm1117.md#rule-1-feed-it-from-5-v-never-12-v)).
  The buck's own feedback divider sets the 5 V output — it is the **ADJ**
  variant, so that divider is not optional and its values decide whether the
  LM1117 has enough headroom.

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
