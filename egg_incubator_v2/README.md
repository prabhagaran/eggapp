Egg Incubator v2 — Pin map and notes

Pins used by this project

- Buttons
  - BTN_UP = GPIO 32 (defined in config.h)
  - BTN_DOWN = GPIO 33 (defined in config.h)
  - BTN_OK = GPIO 25 (defined in config.h)

- I2C
  - I2C_SDA = GPIO 21 (defined in config.h)
  - I2C_SCL = GPIO 22 (defined in config.h)

- Sensors
  - DHT_PIN = GPIO 4 (DHT sensor)
  - DS18B20_PIN = GPIO 18 (1-wire)

- Relays / Outputs
  - RELAY_HEATER = GPIO 26
  - RELAY_COOLER = GPIO 27
  - RELAY_HUMIDIFIER = GPIO 14
  - RELAY_FAN = GPIO 13 (also used by LEDC PWM)
  - RELAY_PUMP = GPIO 12
  - RELAY_TURNER = GPIO 15
  (all relay defs in config.h)

Available / recommended free GPIOs

- General-purpose recommended: GPIO5, GPIO16, GPIO17, GPIO19, GPIO23
- Input-only pins (read-only, ADC): GPIO34, GPIO35, GPIO36, GPIO37, GPIO38, GPIO39

Pins to avoid or treat special

- UART / Serial: GPIO1 (TX0), GPIO3 (RX0) — used by `Serial`.
- Flash / SD pins (do not use): GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO11.
- Boot/strapping pins (use with caution): GPIO0, GPIO2, GPIO12, GPIO15.

Where pins are configured and used

- Definitions: `config.h`
- Relay/button pinMode and initial writes: `egg_incubator_v2.ino` (setup)
- Fan PWM uses `RELAY_FAN` with LEDC in `task_incubator.cpp` (setFanSpeed)
- Turner, pump and milestone logic reference relays in `task_incubator.cpp`

If you want these pin entries added to `pins.csv` or exported as JSON, tell me which format and I'll update it.
