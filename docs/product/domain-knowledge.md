# Poultry Domain Knowledge

- **Owner agent:** poultry-domain-expert
- **Status:** Baseline v1 (2026-07-17)

Authoritative reference for poultry-specific facts used by requirements,
domain model, and seeded reference data. Values are established small-flock /
hatchery practice for **forced-air incubators** (the ESP32 incubator is
forced-air). Vaccination schedules are a **regional template — confirm with a
local vet/hatchery before relying on them**; the app treats them as editable
templates, not hardcoded truth.

## 1. Incubation parameters by species

| Species | Days | Temp (°C) | RH set–lockdown | RH lockdown | Stop turning (lockdown) | Candling days |
|---|---|---|---|---|---|---|
| Chicken | 21 | 37.5–37.8 | 50–55% | 65–70% | day 18 | 7, 14, 18 |
| Quail (Coturnix) | 17–18 | 37.5–37.8 | 45–55% | 65–70% | day 14–15 | 6, 12, 14 |
| Duck (common) | 28 | 37.5 | 55–60% | 70–75% | day 25 | 7, 14, 25 |
| Muscovy duck | 35 | 37.5 | 55–60% | 70–75% | day 31 | 10, 21, 31 |
| Turkey | 28 | 37.5 | 55–60% | 70–75% | day 25 | 8, 14, 25 |
| Goose | 28–32 | 37.4–37.6 | 55–65% | ~75% | day 25–28 | 7, 14, 25 |

General rules across species:
- **Lockdown** = final ~3 days: turning stops, humidity raised, incubator
  not opened. Lockdown day = incubation_days − 3.
- **Turning**: automatic turners typically hourly; minimum 3–5×/day if manual.
- **Temperature tolerance**: alarm at ±0.3 °C from setpoint sustained;
  sustained >38.9 °C or <36.1 °C for ~1 h is embryo-threatening (critical).
  Brief spikes during opening are normal — alerting must use sustained
  readings, not single samples.
- **Humidity tolerance**: ±5 percentage points warning; ±10 critical.

## 2. Egg selection and storage (pre-incubation)

- Store pointed-end **down**, 12–18 °C, ~65–75% RH.
- Set within **7 days** of collection for best hatchability; it declines
  roughly 0.5–1%/day after day 7. Over 14 days is generally not worth setting.
- If stored >7 days, turn stored eggs daily.
- Bring eggs to room temperature 6–12 h before setting (prevents condensation).
- Reject for setting: cracked, misshapen, double-yolk, very dirty (do not
  wash hatching eggs), abnormally small/large.

## 3. Candling

- Method: bright light against the shell in a dark room, blunt end (air cell)
  up. Total time out of incubator per session <30 min.
- **Day 7** (fertility check): spider-web veining + dark embryo spot =
  viable. Completely clear = infertile. Blood ring = early death. Dark-shelled
  eggs may need to wait to day 10.
- **Day 14**: embryo fills much of the egg, movement often visible; remove
  quitters (no development progress).
- **Day 18** (at lockdown entry): dark mass with air cell ≈ ⅓ of egg; remove
  clears/quitters, then do not open again until hatch.
- Never candle during lockdown.

## 4. Hatch metrics (fixed definitions)

- **Fertility %** = fertile at first candling ÷ eggs set
- **Hatch of set %** = hatched ÷ eggs set
- **Hatch of fertile % (hatchability)** = hatched ÷ fertile
- Small-flock benchmarks: fertility 85–95%; hatch-of-fertile 75–90%.

## 5. Brooding and flock stages

Layer-type reference stages (broilers noted separately):

| Stage | Age | Notes |
|---|---|---|
| Brooding | 0–6 wk | Brooder temp 32–35 °C week 1, −2–3 °C each week to ~21 °C ambient |
| Grower | 6–18 wk | |
| Pre-lay / point-of-lay | 16–20 wk | Transition to layer feed ~18 wk |
| Layer | 18–72+ wk | |
| Spent / cull | end | |

Broiler track: starter 0–2 wk, grower 2–5 wk, finisher to market ~6–7 wk.
Stage is **derived from hatch date** (or acquisition age) — never manually
tracked.

Mortality norms: ≤3–5% during first 2 weeks of brooding is within normal
range; ongoing adult mortality <1%/month. A **drop in feed/water consumption
precedes visible illness by 24–48 h** — consumption anomaly detection is a
genuinely useful early-warning signal, not a gimmick.

## 6. Vaccination (template — verify locally)

Layer template:

| Age | Vaccine | Disease | Route |
|---|---|---|---|
| Day 1 | Marek's (HVT) | Marek's disease | SC injection (usually at hatch) |
| Day 1–5 | ND + IB live | Newcastle, Infectious bronchitis | Eye drop / spray |
| Day 10–14 | IBD live | Gumboro | Drinking water |
| Day 18–21 | IBD booster | Gumboro | Drinking water |
| Day 21–28 | ND booster (LaSota) | Newcastle | Drinking water |
| Wk 6–8 | Fowl pox | Fowl pox | Wing-web stab |
| Wk 16–18 | ND + IB + EDS inactivated | pre-lay booster | IM injection |

Broiler template (short life): Day 1 ND+IB; Day 10–14 IBD; Day 18–21 ND booster.

**Required fields per vaccination record** (compliance-relevant, immutable):
date, flock, vaccine name, disease(s), manufacturer + lot number, expiry,
route, dose, number of birds vaccinated, administered by, adverse reactions,
next due date. Withdrawal period where applicable.

## 7. Feed and water norms

| Stage | Feed | Protein | Notes |
|---|---|---|---|
| Chick (0–6/8 wk) | Starter | 20–22% | |
| Grower (6–18 wk) | Grower | 16–18% | |
| Layer (18 wk+) | Layer | 16–17% | + 3.8–4.2% calcium |
| Broiler | Starter → Finisher | 22–24% → 18–20% | |

- Adult layer intake ≈ 105–120 g/bird/day.
- Water ≈ 1.8–2.5× feed weight; rises sharply above 30 °C ambient (heat
  stress indicator when water spikes and feed drops).

## 8. Housing environment (brooding/coop)

- Brooder temps per stage table above; RH 50–70%; ammonia <25 ppm.
- (Coop-level sensors are out of MVP scope — incubator monitoring first —
  but thresholds here apply if coop devices are added later.)

## 9. Traceability chain (compliance backbone)

```
Flock (source) → EggCollection → EggBatch (incubation) → HatchEvent → Flock (new)
                                                              ↓
                                        VaccinationRecords, Feed/Water logs,
                                        MortalityRecords per flock
```

Must be queryable in **both directions**: from a diseased flock back to
source flock/batch, and from a source flock forward to all descendant
batches/flocks. Records retained ≥12 months after a flock is disposed.
This chain is why vaccination and hatch records are immutable/append-only.
