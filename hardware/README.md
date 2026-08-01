# Hardware

Electrical design for the two ESP32 devices in this system: the **incubator
controller** and the **coop monitoring node**. Schematics, PCB layouts, BOMs,
mechanical drawings and the design notes behind them.

This is a first-class surface alongside `apps/android/`, `apps/web/`,
`apps/api/` and `apps/firmware/` — it is not part of the firmware. Firmware
consumes the pin assignment; hardware defines it. When the two disagree, see
"Who owns the pin map" below.

## Layout

```
hardware/
├── common/              Shared across both boards
│   ├── datasheets/      Component datasheets
│   └── library/         Schematic symbols + PCB footprints
├── incubator/           Incubator controller board
│   ├── docs/            Block diagram, pin map, power budget
│   ├── schematic/       Source schematic files
│   ├── pcb/             Board layout + fabrication outputs
│   ├── bom/             Bill of materials
│   └── mech/            Enclosure, panel cutouts, mounting
└── coop-monitor/        Coop monitoring node board
    └── (same structure)
```

Each board folder has its own README covering what the board does, its state,
and what still has to be decided.

## The two boards

| | Incubator controller | Coop monitor |
|---|---|---|
| Firmware | [egg_incubator_v2](../apps/firmware/egg_incubator_v2/) | [coop_monitor_v1](../apps/firmware/coop_monitor_v1/) |
| Device ID | `INCUBATOR_01` | `COOP_01` |
| Role | Closed-loop control | Sensor-only telemetry |
| Actuators | 6 (heater, cooler, humidifier, fan, pump, turner) | none |
| Mains-side circuitry | Yes | No |
| Display / buttons | 128×64 OLED, 3 buttons | none |
| Power | Mains-derived | Mains-derived or PoE/battery (undecided) |
| Design docs | [incubator/docs/](incubator/docs/) | [coop-monitor/docs/](coop-monitor/docs/) |

The coop node is deliberately the simpler board. Per
[ADR 0009](../docs/architecture/adr/0009-coop-monitoring-devices.md) it drives
nothing and runs no control loop, so it carries no mains switching and no
safety-critical path. Do not "unify" the two designs onto one board that
happens to depopulate the relay section for coop builds — the whole point of
the split is that a coop node cannot energise anything.

## Altium projects — there are two

| Project | Status |
|---|---|
| [eggubator/](eggubator/) | **Live.** `MCU.SchDoc` (ESP32-WROOM-32E breakout), `eggubator.SchLib`, `eggubator.PcbLib`, `eggubator.PcbDoc` |
| [apps/firmware/Hardware/Eggubator/](../apps/firmware/Hardware/Eggubator/) | **Superseded.** `Block_Diagram.SchDoc`, `ESP32_Wroom_32UE.SchDoc`, `incubator.SchLib`, `incubator.PcbLib`. No board file |

Both describe the same board. The newer project has the PCB file and the module
sheet; the older one has the block-diagram sheet, whose content is now captured
in [incubator/docs/block-diagram.md](incubator/docs/block-diagram.md).

**Work in `hardware/eggubator/`.** The old project is kept for reference only —
do not edit both. Neither has been deleted or merged: consolidating a live
Altium project means relinking library paths and rewriting `.PrjPcbStructure`,
which is worth doing deliberately rather than as a side effect. Two loose ends
to tidy when that happens:

- `hardware/eggubator/` sits outside the `incubator/` ⁄ `coop-monitor/` layout
  used by everything else here.
- [incubator/schematic/PCB_Project/](incubator/schematic/PCB_Project/) holds a
  stray `PCB_Project.PrjPcb` that belongs to neither.

Datasheets: see [common/datasheets/README.md](common/datasheets/README.md).
Migration policy: [CONVENTIONS.md](CONVENTIONS.md#eda-tool).

### Current design state

`MCU.SchDoc` is a **1:1 module breakout**, not the controller schematic, and
`eggubator.PcbDoc` contains **no components, nets or copper** — the schematic
has never been imported into it. Details and the blocking items are in
[incubator/README.md](incubator/README.md#state).

## Who owns the pin map

The pin map lives in firmware (`config.h`) and in the per-board
`docs/pin-map.md` here. Those two must agree, and today the firmware is what
physically runs — so **firmware is authoritative until a board is fabricated**,
at which point the fabricated board becomes authoritative and firmware adapts.

The pin-map documents in this folder are transcribed from firmware and cite the
defining file and line. If you change a pin here, change it in `config.h` in
the same commit, or you have created a board nobody can flash correctly.

There are two known ESP32 strapping-pin conflicts on the incubator
(`RELAY_PUMP` on GPIO12, `RELAY_TURNER` on GPIO15). These must be resolved
**in hardware** before layout — see
[incubator/docs/pin-map.md](incubator/docs/pin-map.md#strapping-pin-conflicts).

## Before fabricating anything

Nothing in this folder has been built or electrically verified. Every current
figure in the power budgets is an estimate from datasheets and typical parts,
not a measurement. Read the "Unverified" section of each board's power budget
before ordering.
