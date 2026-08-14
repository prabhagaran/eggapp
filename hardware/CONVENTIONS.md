# Hardware conventions

Rules that apply to every board in this folder. Board-specific decisions live
in the board's own README.

## EDA tool

The existing incubator project is **Altium**
([apps/firmware/Hardware/Eggubator/](../apps/firmware/Hardware/Eggubator/)).
New work stays in Altium unless a deliberate decision is made otherwise —
mixing two EDA tools across two boards that share a symbol library costs more
than either tool saves.

If a switch to KiCad is wanted (licence cost, reviewability of text-based
files in git, CI-generated fabrication outputs), that is an architectural
decision and belongs in an ADR under
[docs/architecture/adr/](../docs/architecture/adr/), not in a commit message.
Two open items either way:

- The Altium project at `apps/firmware/Hardware/` has not been moved into
  `hardware/`. Doing so requires relinking library paths and the
  `.PrjPcbStructure`.
- Altium binary files do not diff. Every schematic change therefore needs a
  human-readable summary in the commit message and, for anything affecting
  pins or power, an update to the board's `docs/`.

## What goes in git

Committed:

- Schematic and PCB source files
- Symbol and footprint libraries under `common/library/`
- BOMs (CSV — text, so it diffs)
- Design docs under each board's `docs/`
- Released fabrication outputs (Gerbers, drill, pick-and-place), **zipped and
  version-tagged**, under `<board>/pcb/releases/`

Not committed (see [.gitignore](../.gitignore)):

- Altium `History/` and `__Previews/` directories, `.PrjPcbStructure` caches
- KiCad backups (`*-backups/`, `*.kicad_prl`, `fp-info-cache`)
- Ad-hoc Gerber exports that were never released

## Naming

- Board folders: lowercase kebab-case matching the firmware sketch they pair
  with (`incubator`, `coop-monitor`).
- Reference designators: standard (R, C, U, Q, D, J, SW, K for relays, FB for
  ferrites). Number by sheet: sheet 2 uses 200-series.
- Net names: match the firmware macro where a net lands on a GPIO —
  `RELAY_HEATER`, `DHT_PIN`, `I2C_SDA`. A net called `NET_U3_12` in a design
  whose pin map is documented by macro name is a review finding.
- Fabrication releases: `<board>-r<rev>-<yyyy-mm-dd>.zip`, e.g.
  `incubator-rA-2026-08-01.zip`. Revisions are letters (rA, rB) for board
  spins; firmware versions are numbers and are unrelated.

## Design rules

Applied to both boards unless a board README overrides:

| Rule | Value | Why |
|---|---|---|
| Min trace / space (signal) | 0.2 mm / 0.2 mm | Comfortable at any fab house; no premium tier |
| Min via | 0.3 mm drill / 0.6 mm pad | Same |
| Mains clearance (L–N, L–PE) | ≥ 3.0 mm, slot beneath | 230 V working; see below |
| Mains-to-logic isolation gap | ≥ 6.0 mm | Reinforced-ish; the ESP32 side is touchable |
| Copper for ≥ 1 A | ≥ 1.0 mm on 1 oz | ~10 °C rise |
| Decoupling | 100 nF per IC power pin, at the pin | |
| ESP32 antenna keepout | **No copper on any layer under the antenna**; module overhangs the board edge | Mandatory — see below |

## Antenna keepout

Both boards use the **ESP32-WROOM-32E** — the PCB-antenna variant. That makes
the keepout a hard rule rather than a precaution:

- **No copper, on any layer**, under the antenna area. Not a ground pour, not a
  trace, not a plane.
- The module is placed so the **antenna end overhangs the board edge**, or at
  minimum sits over a routed cutout. Copper under a PCB antenna detunes it and
  costs range that no amount of firmware retry logic recovers.
- Keep the keepout clear of the enclosure's metal too — a plastic box with a
  metallised coating is the same problem one layer out.

Datasheet v2.0 marks the zone on the pin layout (Figure 3, "Keepout Zone") and
refers to *ESP32 Hardware Design Guidelines → Positioning a Module on a Base
Board* for the base-board keepout dimensions. Read that section before placing
the module; the keepout extends onto the carrier board, not just the module.

The -32UE (external antenna, U.FL) has no keepout zone. If a board is ever
respun onto -32UE this rule stops applying to it — but the two are not
interchangeable without that being a deliberate decision, because the range
tradeoff is the whole reason to pick one.

## Mains safety

Only the incubator board carries mains. Non-negotiable for it:

- Mains and logic on opposite sides of a clearly marked isolation barrier,
  with a routed slot through the board under the barrier.
- No mains under or near the ESP32 module, the OLED, or any user-touchable
  connector.
- Earth (PE) bonded to the chassis with a dedicated ring terminal, not a PCB
  trace.
- Fuse on the line side, before anything else.
- Silkscreen hazard marking on the mains section.
- The switching element (SSR-40DA) is a **panel-mounted module**, not a PCB
  part — the board carries only its low-current control pair. Keep it that way;
  it moves the 40 A path off the PCB entirely.

## Documentation duty

A board is not "designed" until its `docs/` folder answers three questions:

1. **block-diagram.md** — what blocks exist and what connects to what.
2. **pin-map.md** — every ESP32 pin: net, function, and the firmware line that
   defines it.
3. **power-budget.md** — every rail, every load, worst-case draw, and which
   numbers are measured versus estimated.

Anything still undecided is written down as an open question in those files
rather than left blank. A blank field reads as "not applicable"; an open
question reads as "someone must decide this".
