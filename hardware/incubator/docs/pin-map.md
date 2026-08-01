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
| `RELAY_PUMP` | 12 | Pump | config.h:38 | ⚠ Strapping pin MTDI — see below |
| `RELAY_TURNER` | 15 | Turner | config.h:39 | ⚠ Strapping pin MTDO — see below |

Relay polarity: `RELAY_ON == LOW`
([config.h:42](../../../apps/firmware/egg_incubator_v2/config.h#L42)). Every
actuator channel needs an external pull-up to 3.3 V so the load stays OFF while
the ESP32 is in reset and its GPIOs float.

## Strapping-pin conflicts

**These must be fixed in hardware before layout.** Logged as BUG-004 in
[FIRMWARE_BUG_REVIEW.md](../../../apps/firmware/FIRMWARE_BUG_REVIEW.md).

**GPIO12 (MTDI) — `RELAY_PUMP`.** MTDI is the strapping pin for internal LDO
(`VDD_SDIO`) voltage selection, sampled at reset. Datasheet v2.0 Table 4 gives
its default configuration as **pull-down, bit value 0** — that default is what
selects 3.3 V flash, and it is the state the module needs to boot.

An active-LOW relay board pulls its input HIGH when idle, which is also exactly
what the fail-safe pull-up above requires. Either one overrides the internal
pull-down, flips the bit to 1, and the module selects 1.8 V flash and does not
boot. This is not intermittent: it is a board that never starts once the relay
is plugged in.

**GPIO15 (MTDO) — `RELAY_TURNER`.** MTDO strapping controls U0TXD printing —
and, together with GPIO5, SDIO slave timing. Datasheet default is **pull-up,
bit value 1**. Less severe than GPIO12 because the pull-up matches the default,
but it is still a strapping pin carrying an external pull-up on a channel that
also drives a load, and getting it wrong silences the boot log that is the
first thing anyone looks at when a unit will not start.

**Fix:** move both to free, non-strapping pins. `pins.csv` already identifies
the candidates:

| Move | From | To (preferred) | Note |
|---|---|---|---|
| `RELAY_PUMP` | 12 | **16** | Safe on WROOM; reserved for PSRAM only on WROVER |
| `RELAY_TURNER` | 15 | **17** | Same |

Spare non-strapping outputs if 16/17 are needed elsewhere: GPIO19, GPIO23.
Avoid GPIO5 for anything that must not energise at power-on — it boots HIGH,
which on an active-LOW channel means the load is briefly *off*, but it is a
strapping pin regardless.

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

## Free after the fix

GPIO19, GPIO23 (and 16/17 if the relocation lands elsewhere). GPIO5 with the
caveat above.

## Programming and debug

Not in the firmware pin map, but the board needs them:

- **UART0 header** — TX0/RX0/GND/3V3 for flashing and the serial monitor
  (115200 baud). Not optional: this is how the device is provisioned and
  diagnosed.
- **EN and IO0 access** — auto-reset circuit (the standard DTR/RTS transistor
  pair) or, at minimum, reset and boot pushbuttons. Without one of the two,
  flashing means shorting pins by hand.
