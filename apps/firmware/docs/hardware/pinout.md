# Pinout

!!! info "As-built — verified against `egg_incubator_v2/config.h` (FW 2.0.0)"
    This page reflects the wiring of the shipped firmware. The original
    design-phase pinout (rotary encoder, water-level sensor, 8-relay bank)
    was superseded during implementation.

## Inputs — Buttons (active-LOW, internal pull-ups)

| Button | GPIO |
|--------|------|
| UP     | GPIO 32 |
| DOWN   | GPIO 33 |
| OK     | GPIO 25 |

Debounced in software (50 ms, ezButton) by the button task.

## I²C bus (shared)

| Signal | GPIO | Devices |
|--------|------|---------|
| SDA | GPIO 21 | SSD1306 OLED (0x3C) + DS1307 RTC |
| SCL | GPIO 22 | |

## Sensors

| Sensor | GPIO | Notes |
|--------|------|-------|
| DHT22 (humidity + cross-check temp) | GPIO 4 | single-wire digital |
| DS18B20 (primary temperature) | GPIO 18 | 1-Wire, 4.7 kΩ pull-up to 3.3 V |

## Relay outputs (active-LOW relay board: `RELAY_ON = LOW`)

| Function | GPIO | Notes |
|----------|------|-------|
| Heater | GPIO 26 | |
| Cooler | GPIO 27 | climate-chamber profile only |
| Humidifier | GPIO 14 | ⚠ **active-HIGH** — inverse of every other channel |
| Fan | GPIO 13 | **LEDC PWM**, inverted duty (100 % speed = pin LOW) |
| Pump | GPIO 12 | ⚠ strapping pin — see warning below |
| Turner | GPIO 15 | ⚠ strapping pin — see warning below |

!!! warning "Known issue — strapping pins (BUG-004, open)"
    **GPIO 12 (MTDI)** selects flash voltage at reset: a relay-board pull-up on it
    can set VDD_SDIO to 1.8 V and prevent the ESP32 from booting at all.
    **GPIO 15 (MTDO)** is also a strapping pin. Recommended: move these channels
    to safe GPIOs (16, 17, 19, 23) on the next board revision, or burn the
    flash-voltage eFuse (`espefuse.py set_flash_voltage 3.3V`). Verify boot
    behavior with the actual relay board attached.

## Relay polarity

The relay board is active-LOW (`RELAY_ON = LOW`), but the **humidifier module
on GPIO 14 is active-HIGH** (`HUMIDIFIER_ON = HIGH`) — driving it LOW stops it.

| Channel | ON level | OFF level |
|---------|----------|-----------|
| Heater, cooler, pump, turner | LOW | HIGH |
| **Humidifier** | **HIGH** | **LOW** |
| Fan | LEDC PWM, inverted duty | — |

Nothing writes `RELAY_ON`/`RELAY_OFF` to a pin directly. Both write sites —
`setRelay()` in `globals.cpp` and the boot-time OFF sweep in `setup()` — resolve
the level through `relayLevel(pin, on)` in `globals.h`, so the two cannot drift
apart when a channel's polarity changes.

!!! warning "GPIO 14 pulses at reset"
    GPIO 14 emits a brief pulse during ESP32 boot, before `setup()` runs. On the
    old active-LOW wiring that pulse read as *off*; active-HIGH inverts that, so
    the humidifier can twitch on for a few milliseconds at every reset. Harmless
    for a mister, but if the channel ever drives something that must not be
    pulsed, add an external pull-down on GPIO 14 rather than relying on firmware.

## Safety-relevant electrical notes

- All relay pins are driven to their **OFF level** (per the polarity table above)
  **first thing in `setup()`**, before any peripheral init.
- The fan LEDC channel initializes at duty 255 (= relay OFF for the inverted
  active-LOW convention).
- All ESP32 GPIOs are 3.3 V logic; relay coils must be driven via the opto-isolated
  board, never directly.
- GPIO 6–11 are unavailable (internal flash).

## Differences from the design-phase pinout

| Item | Design | As-built |
|------|--------|----------|
| Input device | Rotary encoder (18/19/23) | 3 buttons (32/33/25) |
| DS18B20 | GPIO 4 | GPIO 18 |
| DHT22 | GPIO 16 | GPIO 4 |
| Water level sensor | GPIO 34 | not fitted |
| Relays | 8 channels (25/26/27/14/12/13/32/33) | 6 channels (26/27/14/13/12/15) |
| RTC | — | DS1307 on I²C |
