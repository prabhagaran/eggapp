# Datasheets

Datasheets for parts used on either board.

## On file here

| File | Part | Version | Use |
|---|---|---|---|
| `esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf` | ESP32-WROOM-32E / -32UE | **v2.0** (© 2025) | **Authoritative.** All ESP32 figures in the design docs cite this |
| `DS-17830-ESP32_WROOM_MCU_Module_-_16MB__Chip_Antenna_.pdf` | Same module | **v1.1** (2020-11-02) | Superseded — see below |
| `lm1117.pdf` | **LM1117 / LM1117I** LDO (TI) | **SNOS412Q** (Jan 2023) | 3.3 V rail. Analysis in [../3v3-rail-lm1117.md](../3v3-rail-lm1117.md) |

Note on the LM1117: this is the **TI LM1117**, *not* the AMS1117 those parts
are commonly confused with. They share the SOT-223 pinout but are different
silicon with different dropout and quiescent-current specs. The BOMs specify
`LM1117IMPX3.3/NOPB`.

### The DS-17830 file is a superseded duplicate

Despite the distributor filename, its content is the **same** Espressif
ESP32-WROOM-32E/32UE datasheet, at revision **v1.1 from November 2020** — two
major revisions behind the v2.0 already on file. The filename is a distributor
document ID describing a stock line ("16 MB, chip antenna"), not a different
part with its own datasheet; open it and the title page reads
"ESP32-WROOM-32E & ESP32-WROOM-32UE Datasheet V1.1".

It should be deleted. Keeping two revisions of one datasheet in one folder is
precisely the failure mode
[../../CONVENTIONS.md](../../CONVENTIONS.md) warns about: someone checks a
footprint or a current figure against whichever copy they opened first.

Two things the older revision genuinely lacks:

- v1.1 describes the module as **4 MB flash, with 8/16 MB by custom order**;
  v2.0 lists 4/8/16 MB as standard. If a 16 MB part is actually being sourced,
  v2.0 is the document that covers it.
- v2.0's revision history records an update to *Table 16 Current Consumption
  Depending on RF Modes*. The values happen to be unchanged between the two,
  but that is something to confirm rather than assume.

### Two facts worth carrying forward

Both design docs now cite these; they are recorded here so the source is
traceable if the file is ever replaced.

- **Table 14 — `I_VDD`, current delivered by external power supply: min
  0.5 A.** A requirement for the module alone, before any peripheral.
- **Table 16 — 802.11b TX @19.5 dBm: 239 mA average, 379 mA peak.** The
  commonly quoted "240 mA" is the average. Lowest RF-active figure in the whole
  table is 112 mA (RX), which sets the floor for any always-associated node.

## Also collected (not yet moved here)

These are in
[apps/firmware/Hardware/Datasheet/](../../../apps/firmware/Hardware/Datasheet/)
and move here when the Altium project migrates
([../../CONVENTIONS.md](../../CONVENTIONS.md#eda-tool)):

| File | Part | Used by |
|---|---|---|
| `SSR40DA.pdf` | SSR-40DA solid-state relay | Incubator (heater) |
| `infineon-irl540n-datasheet-en.pdf` | IRL540N logic-level N-MOSFET | Incubator (fan PWM) |
| `Industrial_Grade_Electronics_Protection_Circuits.pdf` | Protection reference | Both |
| `Industrial_Protection_References.pdf` | Protection reference | Both |

That folder also holds a copy of the v2.0 ESP32 datasheet, byte-identical to
the one here. Delete one of the two when the migration happens — but not
before, since the Altium project's part descriptions reference that path.

## Still needed

Parts referenced by firmware or by the design docs that have no datasheet on
file yet. Fetch before finalising the relevant schematic section:

- **DS18B20** — 1-Wire temperature, incubator primary sensor
- **DHT22 / AM2302** — temperature + humidity, both boards
- **DS1307** — RTC, incubator
- **SSD1306** 128×64 OLED module — incubator
- **MH-Z19B** — NDIR CO₂, coop (UART)
- **MQ-137** — ammonia, coop (analog; needs the Rs/R0 curve, not just the
  pinout — the firmware conversion is an uncalibrated linear placeholder)
- **BH1750** — ambient light, coop (I2C)
- **HC-SR04** — ultrasonic level, coop ×2 (5 V part on a 3.3 V MCU — the echo
  level-shift is a design decision, not an oversight to discover at bring-up)
- Relay module / relay part for the non-SSR incubator channels
- AC-DC supply module for both boards
- **12 V → 5 V buck converter**, if the board input is 12 V. Needed before the
  LM1117 can be used at all — see
  [../3v3-rail-lm1117.md](../3v3-rail-lm1117.md#rule-1-feed-it-from-5-v-never-12-v)

## Naming

`<manufacturer>-<part>-<revision-or-date>.pdf`. The revision matters: a
footprint checked against rev C and a part shipped to rev E is a board spin.
