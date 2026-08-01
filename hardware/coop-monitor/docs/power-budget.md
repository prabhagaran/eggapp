# Coop monitor — power budget

> ESP32 figures are from the **ESP32-WROOM-32E/32UE datasheet v2.0**
> ([common/datasheets/](../../common/datasheets/)), Tables 14 and 16. Everything
> else is an estimate, and **nothing has been measured on this hardware**. See
> [Unverified](#unverified).

The load depends entirely on which sensors are fitted, and the firmware treats
every channel as optional. So this is budgeted twice: the minimum shippable
build, and everything fitted.

## 3.3 V rail

| Load | Typical | Peak | Basis |
|---|---|---|---|
| ESP32-WROOM-32E, RX / associated | 112 mA | 118 mA | Datasheet Table 16. **Lowest RF-active figure there is** |
| ESP32 TX, 802.11b @19.5 dBm | 239 mA avg | **379 mA** | Table 16 — worst case, and likely here (see below) |
| DHT22 | 1.5 mA | 2.5 mA | During measurement |
| BH1750 | 0.2 mA | — | If fitted |
| Pull-ups (DHT, I2C) | 1 mA | — | |
| **Total** | **~115 mA** | **~383 mA** | |

**Regulator: 800 mA minimum. Do not fit a 500 mA part.** Datasheet Table 14
requires ≥ 0.5 A from the external supply for the module alone, and the
802.11b TX peak is 379 mA — the 240 mA figure often quoted is the TX
*average*, not the peak. 10 µF bulk + 100 nF at the module pin.

**Assume the 802.11b row, not the 802.11n one.** A coop node is at the edge of
range from a house AP; a weak link falls back to the lowest rate at the highest
TX power, which is exactly the 239 mA / 379 mA line. The efficient 11n figures
(165 mA / 211 mA) apply to a node sitting next to the router. This one is not.

## 5 V rail

| Load | Typical | Peak | Basis | Fitted by default |
|---|---|---|---|---|
| 3.3 V regulator input | 115 mA | 383 mA | Pass-through | yes |
| **MQ-137 heater** | **150 mA** | 180 mA | Continuous heater, always on | no |
| MH-Z19B CO₂ | 18 mA | 150 mA | Peak during the NDIR lamp pulse | no |
| HC-SR04 ×2 | 4 mA | 30 mA | Peak during the ping burst | no |
| **Minimum build** (DHT22 only) | **~115 mA** | ~383 mA | | |
| **Full build** (all sensors) | **~290 mA** | ~740 mA | | |

**Design the 5 V rail for 1.5 A.** The full-build peak is ~740 mA once the
corrected ESP32 TX figure is used — a 1 A supply has almost no margin, and the
worst case (TX burst coinciding with the MH-Z19B lamp pulse and a ping) is not
exotic: all three run on independent timers and will collide regularly.

**The 5 V rail must also hold ≥ 4.8 V at that 740 mA transient**, because the
3.3 V LDO downstream has only 300 mV of dropout margin. A 5 V rail that sags to
4.7 V takes the 3.3 V rail down with it, during a TX burst. See
[common/3v3-rail-lm1117.md](../../common/3v3-rail-lm1117.md#rule-5-check-the-dropout-against-a-sagging-5-v-rail).

## The 3.3 V regulator

**LM1117I-3.3, SOT-223** — analysis in
[common/3v3-rail-lm1117.md](../../common/3v3-rail-lm1117.md). Three things that
bind this board specifically:

- The **`I` grade is mandatory here**, not a preference. The plain LM1117 is
  rated 0 to +125 °C; a coop drops below freezing in winter and would put the
  standard part outside its rated range.
- It must be fed from **5 V, never 12 V** — at 12 V in there is no SOT-223
  layout that avoids thermal shutdown.
- Its tab is **VOUT**, so the ≥ 0.3 in² heatsink pour goes on the 3.3 V net,
  not ground.

## The MQ-137 dominates

At ~150 mA continuous, the ammonia sensor's heater draws more than the ESP32
average and roughly doubles the node's consumption on its own. It is also the
one load that cannot be duty-cycled without destroying the reading — MQ-series
heaters need thermal equilibrium, and a burn-in measured in hours.

This is the single fact that decides the power architecture. A build with the
MQ-137 fitted is a mains or PoE build. Full stop.

## Battery viability

Worth writing down, because "put it on a battery" is the obvious first
instinct and it does not survive contact with the firmware.

The firmware never sleeps. It holds an MQTT keepalive of 60 s, publishes
telemetry every 60 s, and polls sensors every 5 s
([config.h:69-71](../../../apps/firmware/coop_monitor_v1/config.h#L69-L71),
[config.h:144](../../../apps/firmware/coop_monitor_v1/config.h#L144)). Radio
stays associated throughout.

The datasheet closes the question. Table 16's **lowest RF-active figure is
112 mA** (RX, peripherals disabled, CPU idle) — that is the floor for a node
that stays associated, and this one does. There is no 20 mA "connected idle"
state available to a device holding an MQTT session open.

At ~115 mA average for the minimum build, a 2000 mAh cell lasts about 17 hours.
Solar cannot carry that through a run of overcast days, and with the MQ-137
fitted (~265 mA) it is not close in any season.

**A battery build requires a firmware deep-sleep strategy first** — which means
dropping the MQTT session between publishes and accepting the reconnect cost
and the loss of prompt liveness detection. That is an architecture change with
consequences for the API's device-online logic, not a hardware tweak. It needs
an ADR before it needs a schematic.

Recommendation: design for mains or PoE. Revisit battery only if a sleep
strategy is adopted.

## Environment

Ammonia, humidity and dust, continuously. This is a part-selection constraint,
not a footnote:

- Ammonia attacks copper and silver. Conformal coat the populated board.
- Condensation is likely with coop temperature swings. Avoid a fully sealed
  box with no thermal management; a sealed box that breathes through a
  membrane vent is the usual answer.
- Rate electrolytics for the humidity, or use ceramics and polymer caps where
  the capacitance allows.

## Unverified

Before a fabrication release:

- ESP32 average draw with this firmware's actual duty cycle — the 5 s sensor
  poll plus 60 s publish plus keepalive means a higher radio duty cycle than a
  nominal sensor node.
- MQ-137 heater current for the specific module sourced. Clone modules vary
  widely and this is the dominant load.
- MH-Z19B peak during the lamp pulse, and whether it sags the shared 5 V rail
  enough to disturb the ADC reading taken on the same rail's divider.
- HC-SR04 ping-burst peak with the cable length actually used.
- Cold-start behaviour: MQ-137 draws more while its heater warms, coinciding
  with the ESP32's association burst. Worst-case inrush is the two together.
