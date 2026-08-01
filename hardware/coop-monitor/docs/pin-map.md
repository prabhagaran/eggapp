# Coop monitor — ESP32 pin map

Transcribed from
[coop_monitor_v1/config.h](../../../apps/firmware/coop_monitor_v1/config.h).
Firmware is authoritative until a board is fabricated
([../../README.md](../../README.md#who-owns-the-pin-map)).

Module: **ESP32-WROOM-32E-N4** (PCB antenna, 4 MB flash), matching the
incubator board. Antenna keepout is mandatory
([CONVENTIONS.md](../../CONVENTIONS.md#antenna-keepout)), and the range risk
for this board specifically is noted in
[block-diagram.md](block-diagram.md).

## Assigned

| Net | GPIO | Function | Defined | Fitted by default | Electrical notes |
|---|---|---|---|---|---|
| `DHT_PIN` | 4 | DHT22 data | config.h:106 | **yes** | 10 kΩ pull-up to 3.3 V |
| `I2C_SDA` | 21 | I2C data | config.h:109 | no | BH1750 |
| `I2C_SCL` | 22 | I2C clock | config.h:110 | no | BH1750 |
| `CO2_RX_PIN` | 16 | UART2 RX ← MH-Z19B TX | config.h:112 | no | Sensor TX is 3.3 V; safe direct |
| `CO2_TX_PIN` | 17 | UART2 TX → MH-Z19B RX | config.h:113 | no | |
| `NH3_ADC_PIN` | 35 | MQ-137 analog | config.h:114 | no | **ADC1** — mandatory, see below. Input-only; needs a divider |
| `FEED_TRIG_PIN` | 25 | HC-SR04 trigger | config.h:115 | no | 3.3 V out; most HC-SR04s accept it |
| `FEED_ECHO_PIN` | 33 | HC-SR04 echo | config.h:116 | no | **5 V out — level shift required** |
| `WATER_TRIG_PIN` | 32 | HC-SR04 trigger | config.h:117 | no | |
| `WATER_ECHO_PIN` | 34 | HC-SR04 echo | config.h:118 | no | Input-only. **5 V out — level shift required** |
| `PORTAL_TRIGGER_PIN` | 0 | BOOT button | config.h:57 | **yes** | See below |

## GPIO0 — the provisioning button

GPIO0 is the WiFi config-portal trigger: press and hold ~2 s within 5 s of
power-on to reopen the captive portal
([config.h:57-59](../../../apps/firmware/coop_monitor_v1/config.h#L57-L59)).
It is also the boot strapping pin.

**The board needs a real, accessible button on GPIO0.** This is the only way to
change the node's WiFi network without a laptop and a cable, and the node lives
in a coop. If the enclosure is sealed, the button must still be reachable —
a membrane button or a recessed hole, decided with the enclosure.

The strapping constraint is why the firmware checks *after* boot rather than at
reset: held LOW through reset, the ESP32 enters serial download mode and the
sketch never runs. So the button is a normal active-LOW momentary to GND with
the module's pull-up — no external pull-down, nothing that holds it LOW at
power-on.

## ADC constraint

`NH3_ADC_PIN` is GPIO35, on **ADC1**. ADC2 is unusable while WiFi is active,
and this node is never off WiFi. Any analog channel added later must land on
ADC1 (GPIO32–39) — but note GPIO32/33/34 are already taken by the ultrasonic
pairs, leaving **GPIO36 and GPIO39** as the only free ADC1 inputs.

The ESP32 ADC reads 0–3.3 V and is not usefully linear near either rail. The
MQ-137 output swings higher than 3.3 V on its own supply, so it needs a divider
sized to keep the useful range in the ADC's linear middle. That divider is part
of the calibration — change it later and every stored reading before the change
is on a different scale.

**Size the divider for a ~2.4 V full-scale, not 3.3 V.** Datasheet v2.0
Table 10 and its notes give the constraint explicitly: at `atten = 3` (the
11 dB setting needed for a wide range), *"when the measurement result is above
3000 (voltage at approx. 2450 mV), the ADC accuracy will be worse than
described"*. A divider that maps the sensor's top of range onto 3.3 V puts the
readings that matter most — high ammonia — into the region the datasheet
disclaims.

Two further cautions from the same table, both of which apply to this node:

- DNL is ±7 LSB and INL ±12 LSB, characterised **with Wi-Fi and Bluetooth
  off**. This node is never off Wi-Fi, so treat those as optimistic. The
  datasheet's own remedy is oversampling — take several samples and average,
  rather than trusting a single conversion.
- The ESP32 ADC needs per-chip calibration to be accurate in absolute terms.
  Combined with the MQ-137's uncalibrated linear conversion in firmware, an
  ammonia reading today is a trend indicator, not a ppm measurement. Do not let
  it drive a welfare threshold until both are calibrated.

## HC-SR04 level shifting

Both echo pins output 5 V. `WATER_ECHO_PIN` is GPIO34, which is input-only and
has **no internal protection diode to clamp it**. A resistor divider (e.g.
1 kΩ / 2 kΩ) or a proper level shifter per echo line is required on both
channels.

Trigger lines are outputs at 3.3 V. Most HC-SR04 modules trigger reliably at
3.3 V; some clones do not. If a specific module is chosen and proves marginal,
that is a buffer, not a redesign — but check it on the bench before committing
to layout.

Firmware allows ~25 ms for the echo
(`ULTRASONIC_TIMEOUT_US 25000`,
[config.h:143](../../../apps/firmware/coop_monitor_v1/config.h#L143)), about
4 m round trip. Cable length between the board and a hopper-mounted sensor is
therefore an installation constraint worth documenting on the connector.

## Do not use

| GPIO | Reason |
|---|---|
| 1, 3 | UART0 TX/RX — serial logging at 115200, used during provisioning |
| 2 | Boot strapping; onboard LED on many devkits |
| 6–11 | Internal SPI flash — never |
| 12, 15 | Strapping (MTDI / MTDO). Free here, but avoid — see the incubator's [conflict](../../incubator/docs/pin-map.md#strapping-pin-conflicts) |

## Free

GPIO5, 13, 14, 18, 19, 23, 26, 27 as general-purpose. GPIO36 and GPIO39 as the
remaining ADC1 inputs.

Worth reserving one free GPIO for a **status LED**. The node has no display and
lives in a dark shed; a heartbeat LED distinguishes "powered but not connected"
from "dead" without a serial cable, and it is the cheapest field diagnostic
available.

## Programming and debug

- **UART0 header** — TX0/RX0/GND/3V3, 115200 baud. The provisioning flow tells
  the user to watch the serial monitor during setup, so this must be reachable.
- **EN and IO0 access** — auto-reset circuit or reset/boot buttons. The GPIO0
  button above doubles as the boot button if it is wired conventionally.
