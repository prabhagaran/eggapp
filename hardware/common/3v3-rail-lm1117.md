# 3.3 V rail — LM1117I-3.3 (SOT-223)

Shared by both boards. All figures from the **LM1117 datasheet SNOS412Q
rev. January 2023** ([lm1117.pdf](datasheets/lm1117.pdf)) and the ESP32
datasheet v2.0. Nothing here has been measured.

## The part

`LM1117IMPX3.3/NOPB` — in [eggubator.SchLib](../eggubator/eggubator.SchLib),
footprint `SOT230P700X180-4N`.

| | |
|---|---|
| Type | Fixed 3.3 V LDO, bipolar |
| Rated output | **800 mA** |
| Package | SOT-223 (DCY), 4-pin |
| Junction temp | **−40 to +125 °C** (the `I` grade) |
| Dropout | 1.2 V typ @ 800 mA, **1.4 V max** over temperature |
| Quiescent (ground) current | 5 mA typ, **15 mA max** over temperature |
| Max input | 15 V recommended, 20 V absolute |

**This is the LM1117, not the AMS1117.** They are pin-compatible SOT-223 parts
and get used interchangeably in hobby designs, but they are different silicon
from different vendors with different specs — AMS1117 dropout is worse
(~1.3 V typ at 1 A) and its quiescent current is higher. The BOM says LM1117I;
buy that. If a board is ever populated with an AMS1117 instead, the numbers
below need redoing.

Choosing the **`I` grade was right** and worth keeping: the standard LM1117 is
0 to +125 °C. A coop node in winter goes below 0 °C, so the plain part would be
outside its rated range on cold mornings.

## Rule 1: feed it from 5 V, never 12 V

A linear regulator burns `(VIN − VOUT) × I` as heat. That makes the input
voltage the single most important decision here.

| Input | Drop | PD at 140 mA typ | PD at 267 mA sustained |
|---|---|---|---|
| **5 V** | 1.7 V | 0.29 W | **0.50 W** |
| 12 V | 8.7 V | 1.34 W | **2.44 W** |

At 12 V input the part needs RθJA ≤ 35 °C/W at 40 °C ambient. The best SOT-223
figure in the datasheet's own Table 9-2 — a full square inch of copper — is
66 °C/W. TO-252 with 1 in² is 47 °C/W. **There is no layout that makes 12 V
work in this package.** It would run into thermal shutdown, which presents as
the 3.3 V rail cycling and the ESP32 rebooting in a loop.

This matters because the old block-diagram sheet shows a **12 V supply** as the
system rail. If 12 V is the input to the board, the architecture must be:

```
12 V ──[ buck converter ]── 5 V ──[ LM1117I-3.3 ]── 3.3 V
```

The buck does the heavy lifting efficiently; the LDO post-regulates. That is
also the *better* answer on noise — a switching converter's ripple would land
straight on the ESP32's ADC reference and the RF section, and on the coop board
the MQ-137 ammonia reading is taken against that same rail. An LDO after a buck
cleans it up.

## Rule 2: the SOT-223 tab is VOUT, not ground

Datasheet Table 6-1: on SOT-223, `VOUT` is **pins 2 and 4**, and **pin 4 is the
tab**. `ADJ/GND` is pin 1, `VIN` is pin 3.

The heatsink copper therefore pours on the **3.3 V net**. Pouring ground under
the tab — the reflex, because most power packages tab to ground — shorts the
output rail. Check this explicitly at layout review.

Conveniently the pour is useful twice: it is both the regulator's heatsink and
low-impedance 3.3 V distribution to the module.

## Rule 3: give it copper

Max **continuous** load current at 5 V input, by copper area and ambient,
computed from Table 9-2 and `PD = (TJmax − TA) / RθJA`:

| Top-side copper | RθJA | TA = 25 °C | TA = 40 °C | TA = 80 °C |
|---|---|---|---|---|
| 0.0123 in² (bare pads) | 136 °C/W | 403 mA | 338 mA | 165 mA |
| 0.066 in² | 123 °C/W | 449 mA | 377 mA | 186 mA |
| **0.3 in² (~195 mm²)** | **84 °C/W** | **671 mA** | **566 mA** | **286 mA** |
| 0.53 in² | 75 °C/W | 755 mA | 637 mA | 324 mA |
| 1.0 in² | 66 °C/W | 862 mA | 728 mA | 372 mA |

**Specify ≥ 0.3 in² (≈ 195 mm²) of top-side copper on the tab, both boards.**
Against a 267 mA sustained worst case that gives comfortable margin at normal
ambient, and it is a pour, not a cost.

Note what the bare-pad row means: with minimum copper the part manages 338 mA
at 40 °C — **below the 415 mA peak** the ESP32 can pull. Minimum copper is not
an option here, it is a fault.

## Rule 4: the 800 mA rating is not the design headroom

The earlier budgets said "800 mA regulator minimum". The LM1117 meets that
exactly at its **rated maximum**, which is not the same as having margin:

- Electrically it is fine — 415 mA peak against an 800 mA part is 2× headroom.
- Thermally it can never approach 800 mA in this application. At 5 V in,
  800 mA is 1.41 W, which needs RθJA ≤ 60 °C/W at 40 °C ambient — better than
  1 in² of copper achieves.

That is acceptable **because the load never goes there**. It is worth writing
down so nobody later reads "800 mA part" as "800 mA available".

### Why the regulator must supply the peak, not a capacitor

The instinct is to let bulk capacitance ride out the ESP32's TX burst. It
cannot. Holding 379 mA for a 1 ms packet within a 100 mV droop needs **3800 µF**;
for a 3 ms burst, 11000 µF. Those are not board-level parts.

So the regulator carries the full transient, and the capacitors do what the
datasheet actually asks of them — loop stability and high-frequency transient
response.

## Rule 5: check the dropout against a sagging 5 V rail

| | |
|---|---|
| Headroom at 5.0 V in | 1.7 V |
| Max dropout over temperature | 1.4 V |
| **Margin** | **0.3 V** |

If the 5 V rail sags to **4.7 V** under load, the LM1117 is exactly at its
dropout limit and the 3.3 V rail follows the input down — during a TX burst,
which is the moment it matters. A cheap 5 V adapter under a 740 mA coop
full-build transient will do this.

Consequences to design for:

- Specify the 5 V source to hold ≥ 4.8 V at the full transient load, not just
  at its nominal rating.
- Keep the trace from the 5 V source to `VIN` short and wide — 300 mV of
  headroom does not survive much IR drop.

## Capacitors

Datasheet requirements, not suggestions:

- **Output: minimum 10 µF tantalum**, required for stability and transient
  response. The LM1117 is a bipolar LDO and needs some ESR; a bare low-ESR
  ceramic can make it ring or oscillate. Use tantalum, or a ceramic with a
  deliberate series resistance — do not silently substitute an X7R because it
  is cheaper.
- **Input: 10 µF**, plus 100 nF close to `VIN`.
- Keep the ESP32's own 10 µF + 100 nF at the module pin as well. That pair is
  for the module's TX transient and is separate from the regulator's stability
  caps.
- A protection diode from `VOUT` to `VIN` is only needed with very large output
  capacitance (~1000 µF), which this design does not have. Not required.

## Layout checklist

- [ ] Tab pour on the **3.3 V** net, ≥ 0.3 in², top side
- [ ] Confirm no ground pour under the tab
- [ ] `VIN` fed from 5 V, never the 12 V rail
- [ ] Wide, short trace from the 5 V source to `VIN`
- [ ] 10 µF tantalum at `VOUT`, close to the pin
- [ ] 10 µF + 100 nF at `VIN`
- [ ] Regulator placed away from the ESP32 antenna keepout
- [ ] On the incubator: regulator not inside the heated volume — see
      [incubator/docs/power-budget.md](../incubator/docs/power-budget.md#thermal)

## Open

- **What is the actual board input voltage?** 12 V (per the old block diagram),
  5 V, or something else. This decides whether a buck stage is needed and is the
  first thing to settle.
- The 5 V rail's own regulation under the coop full-build transient (~740 mA)
  is unspecified — see
  [coop-monitor/docs/power-budget.md](../coop-monitor/docs/power-budget.md).
- None of the thermal figures have been confirmed with a thermocouple on a real
  board. Table 9-2's numbers are for TI's test coupon, not this layout.
