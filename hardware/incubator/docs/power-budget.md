# Incubator — power budget

> ESP32 figures are from the **ESP32-WROOM-32E/32UE datasheet v2.0**
> ([common/datasheets/](../../common/datasheets/)), Tables 14 and 16. Everything
> else is an estimate from datasheets and typical parts, and **nothing has been
> measured on this hardware**. See [Unverified](#unverified) before ordering.

## 3.3 V rail

| Load | Typical | Peak | Basis |
|---|---|---|---|
| ESP32-WROOM-32E, RX / associated | 112 mA | 118 mA | Datasheet Table 16, 802.11b/g/n RX. **Lowest RF-active figure in the datasheet** |
| ESP32 TX, 802.11b @19.5 dBm | 239 mA avg | **379 mA** | Table 16 — the worst case, and what the rail must survive |
| ESP32 TX, 802.11n MCS7 @13 dBm | 165 mA avg | 211 mA | Table 16 — the best case if the link negotiates 11n |
| SSD1306 OLED 128×64 | 20 mA | 30 mA | All-pixels-on is the peak |
| DS18B20 | 1 mA | 1.5 mA | During conversion |
| DHT22 | 1.5 mA | 2.5 mA | During measurement |
| DS1307 | 1.5 mA | — | Running from 3.3 V, not the coin cell |
| I2C + 1-Wire pull-ups | 2 mA | — | 4.7 kΩ ×3, worst case all low |
| Actuator fail-safe pull-ups | 3 mA | — | 6 channels, ~10 kΩ to 3.3 V |
| **Total** | **~140 mA** | **~415 mA** | Peak = 802.11b TX during an OLED refresh |

**Regulator: 800 mA minimum. Do not fit a 500 mA part.**

Two independent reasons, both from the datasheet:

1. **Table 14 sets `I_VDD` — "current delivered by external power supply" — at
   a minimum of 0.5 A for the module alone.** That is a stated requirement, not
   a measured average. The OLED, sensors and pull-ups sit on top of it.
2. The 802.11b TX peak is **379 mA**, not the 240 mA figure often quoted —
   239 mA is the TX *average* at that rate. Sizing to the average is how a
   rail sags mid-transmit.

An ESP32 browning out during a transmit burst produces exactly the
intermittent, unreproducible reset that costs a week to diagnose. 800 mA gives
margin over the 415 mA worst case and clears the datasheet minimum with the
peripherals included.

The firmware makes the TX case routine rather than rare: it publishes MQTT
every 60 s, pushes to Google Sheets, and holds a 30 s keepalive
([config.h:171-203](../../../apps/firmware/egg_incubator_v2/config.h#L171-L203)).
The radio is never off.

Decoupling at the module: 10 µF bulk plus 100 nF at the pin, short loop. This
is the single most common cause of "the ESP32 randomly reboots when WiFi
connects".

## 5 V rail

| Load | Typical | Peak | Basis |
|---|---|---|---|
| 3.3 V regulator input | 140 mA | 415 mA | Pass-through; higher if switching |
| Relay coils (mechanical) | — | 70 mA each | ~5 V / 70 Ω typical coil. **Count undecided** |
| SSR-40DA control input | 15 mA | 20 mA | 3–32 V DC control; low |
| Optocoupler LEDs | 10 mA each | — | If opto-isolating each channel |
| Fan (if 5 V) | — | **TBD** | Depends on the fan chosen; likely 12 V, not this rail |
| **Total** | **TBD** | **TBD** | Blocked on the switching-element decision |

The relay count is the dominant term and is not yet decided
([../README.md](../README.md#blocking-decisions)). Four mechanical relays
energised simultaneously is ~280 mA of coil current alone — a case worth
checking, because the firmware can legitimately run the humidifier and pump
together while the turner cycles.

**Do not order the AC-DC module until the switching elements are chosen.**

## Mains section

The heater is the load that sets everything. Not budgeted here because it does
not pass through the PCB: the SSR-40DA is panel-mounted and switches the
element directly, with only its control pair reaching the board. Size the SSR
heatsink to the element, not to this document.

## Design margins

The regulator is an **LM1117I-3.3 in SOT-223** — full analysis, thermal limits
and layout rules in
[common/3v3-rail-lm1117.md](../../common/3v3-rail-lm1117.md). Two things from
it that constrain this board: it must be fed from **5 V, never 12 V**, and its
tab is **VOUT**, so the heatsink pour goes on the 3.3 V net.

| Rail | Design for | Reason |
|---|---|---|
| 3.3 V | ≥ 800 mA | 379 mA TX peak + peripherals, over a 0.5 A datasheet minimum |
| 5 V | ≥ 2 A | Placeholder until relay count is fixed; revise |
| Bulk capacitance at ESP32 | ≥ 10 µF + 100 nF | TX burst |
| VDD33 at the module | 3.0–3.6 V | Datasheet Table 14. 3.0 V is the floor **under load**, not at idle |
| AC-DC module | ≥ 1.5× computed peak | Derating for a warm cabinet |

## Thermal

The board sits in or near a cabinet held at 37.5 °C, and the climate-chamber
profile allows up to 80 °C
([config.h:57](../../../apps/firmware/egg_incubator_v2/config.h#L57)).
Component ratings must be checked against the **cabinet** ambient, not room
temperature. Electrolytics in particular: an 85 °C part next to an 80 °C
chamber has no life left. Use 105 °C parts, or keep the board outside the
heated volume.

**The ESP32 module itself is affected.** Datasheet Table 14 gives an operating
ambient of −40 to +85 °C for the standard part, with a separate **105 °C
version** available. The climate-chamber profile's 80 °C ceiling leaves 5 °C of
margin on the standard part — before self-heating, and before any enclosure
rise. Two options, and one of them has to be chosen:

- Mount the controller **outside** the heated volume, with sensors on leads.
  Preferred: it also solves the electrolytic problem and the mains clearance
  problem at once.
- Order the **105 °C module variant** and check every other part against 80 °C.

Note that the incubation profile (37.5 °C) is comfortable for the standard
part. It is the climate-chamber profile that breaks it — so this is only a
problem if that profile is actually used on hardware that lives inside the
chamber.

## Unverified

Before a fabrication release, these need measurement rather than estimation:

- Actual ESP32 average draw with this firmware's duty cycle — it publishes
  every 60 s and holds an MQTT keepalive of 30 s, so it is never in deep sleep
  and the TX duty cycle is higher than a nominal sensor node.
- Real relay coil current for the parts actually chosen.
- Fan stall current, which is where an undersized MOSFET dies.
- Inrush at power-on, which sets the AC-DC module and the fuse rating.
- Rail sag during a TX burst, measured at the module pin with a scope. This is
  the measurement most worth taking and the one most often skipped.
