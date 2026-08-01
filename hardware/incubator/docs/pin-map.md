# Incubator — ESP32 pin map

Transcribed from
[egg_incubator_v2/config.h](../../../apps/firmware/egg_incubator_v2/config.h)
and [egg_incubator_v2/pins.csv](../../../apps/firmware/egg_incubator_v2/pins.csv).
Firmware is authoritative until a board is fabricated
([../../README.md](../../README.md#who-owns-the-pin-map)).

Module: **ESP32-WROOM-32E-N4** (PCB antenna, 4 MB flash), per
[MCU.SchDoc](../../eggubator/MCU.SchDoc). Antenna keepout is mandatory —
[CONVENTIONS.md](../../CONVENTIONS.md#antenna-keepout).

## Assigned

| Net | GPIO | Function | Defined | Electrical notes |
|---|---|---|---|---|
| `BTN_UP` | 32 | Button | config.h:13 | Active-LOW, internal pull-up, 50 ms debounce in firmware |
| `BTN_DOWN` | 33 | Button | config.h:14 | Active-LOW, internal pull-up |
| `BTN_OK` | 25 | Button | config.h:15 | Active-LOW, internal pull-up; 3 s hold = fault reset |
| `I2C_SDA` | 21 | I2C data | config.h:20 | Shared: SSD1306 OLED (0x3C) + DS1307 RTC |
| `I2C_SCL` | 22 | I2C clock | config.h:21 | Same bus |
| `DHT_PIN` | 4 | DHT22 data | config.h:26 | Humidity + cross-check temperature |
| `DS18B20_PIN` | 18 | 1-Wire data | config.h:29 | **4.7 kΩ pull-up to 3.3 V required**; primary temperature |
| `RELAY_HEATER` | 26 | Heater | config.h:34 | Active-LOW; SSR-40DA control pair |
| `RELAY_COOLER` | 27 | Cooler | config.h:35 | Active-LOW; climate-chamber profile only |
| `RELAY_HUMIDIFIER` | 14 | Humidifier | config.h:36 | Active-LOW |
| `RELAY_FAN` | 13 | Fan PWM | config.h:37 | **Not a relay.** LEDC PWM, inverted duty — 100 % speed = pin LOW. Drives IRL540N |
| `RELAY_PUMP` | **16** | Pump | config.h:43 | Relocated from GPIO12 |
| `RELAY_TURNER` | **17** | Turner | config.h:44 | Relocated from GPIO15 |

Relay polarity: `RELAY_ON == LOW`
([config.h:42](../../../apps/firmware/egg_incubator_v2/config.h#L42)). Every
actuator channel needs an external pull-up to 3.3 V so the load stays OFF while
the ESP32 is in reset and its GPIOs float.

## Strapping-pin conflicts — RESOLVED

BUG-004 is fixed. Both actuator channels have been moved off strapping pins in
`MCU.SchDoc` and in `config.h`, in the same change:

| Channel | Was | Now | Sheet location |
|---|---|---|---|
| `RELAY_PUMP` | 12 (MTDI) | **16** | relay block at (2270, 650) |
| `RELAY_TURNER` | 15 (MTDO) | **17** | relay block at (1500, 670) |

### Why it mattered

**MTDI (GPIO12)** is the strapping pin for internal LDO (`VDD_SDIO`) voltage
selection, sampled at reset. Datasheet v2.0 Table 4 gives its default as
**pull-down, bit value 0** — that default is what selects 3.3 V flash and lets
the module boot. Anything holding it high at reset flips the bit, the module
selects 1.8 V flash, and it does not start. Not intermittent: a board that
never boots once the channel is populated.

**MTDO (GPIO15)** controls U0TXD printing, and with GPIO5 the SDIO slave
timing. Default **pull-up, bit value 1**. Less severe, but it silences the boot
log — the first thing anyone looks at when a unit will not start.

### Keep 12 and 15 free

They are now unused, and they should stay that way for anything driven at
reset. If more outputs are needed, take **GPIO19 and GPIO23** first — both safe
and non-strapping.

Avoid GPIO5 for anything that must not energise at power-on: it boots HIGH,
which on an active-LOW channel leaves the load off, but it is a strapping pin
regardless.

Burning the flash-voltage eFuse is the other documented escape, but it is
one-way, per-module, and easy to forget on the second unit. Choosing different
pins on a board that does not exist yet costs nothing.

## Input-only pins

No pull-ups, no output drive. ADC-capable.

| GPIO | Note |
|---|---|
| 34 | ADC |
| 35 | ADC |
| 36 | ADC (SVP) |
| 39 | ADC (SVN) |

ADC2 is unusable while WiFi is active — any analog channel added later must
land on ADC1 (GPIO32–39).

## Do not use

| GPIO | Reason |
|---|---|
| 0 | Boot strapping — must be HIGH at reset; BOOT button on devkits |
| 1, 3 | UART0 TX/RX — serial logging |
| 2 | Boot strapping — must be LOW/floating to flash; onboard LED on many devkits |
| 6–11 | Internal SPI flash — never |

## Free

**GPIO19 and GPIO23** — safe, non-strapping, and the first choice for any
further output.

GPIO5 is usable with the caveat above. GPIO12 and GPIO15 are free since the
relocation but should stay unused for anything driven at reset. GPIO16 and
GPIO17 are now taken by the pump and turner.

## Programming and debug

Not in the firmware pin map, but the board needs them:

- **UART0 header** — TX0/RX0/GND/3V3 for flashing and the serial monitor
  (115200 baud). Not optional: this is how the device is provisioned and
  diagnosed.
- **EN and IO0 access** — auto-reset circuit (the standard DTR/RTS transistor
  pair) or, at minimum, reset and boot pushbuttons. Without one of the two,
  flashing means shorting pins by hand.

**EN RC — done.** 10 kΩ to 3.3 V plus 100 nF to GND, matching Espressif's own
ESP32-DevKitC v4 reference schematic (`R11` 10K, `C1` 0.1 µF —
[esp32_devkitc_v4-sch.pdf](../../common/esp32_devkitc_v4-sch.pdf)). It holds EN
low until the rail is up; without it the module boots unreliably on a
slow-rising supply, which looks exactly like an intermittent firmware fault.

The DevKitC schematic is also the reference for the auto-reset circuit if you
add one — its `Q1`/`Q2` pair with `R21`/`R22` (10 K each) is the DTR/RTS
arrangement every ESP32 flashing tool expects.
