# Coop monitor — block diagram

```mermaid
flowchart TB
    subgraph PWR["POWER"]
        IN["Input — SOURCE UNDECIDED<br/>mains adapter / PoE / battery+solar"]
        P5["5 V rail<br/>reverse-polarity + TVS"]
        P33["LM1117I-3.3 SOT-223<br/>5 V → 3.3 V LDO<br/>tab = VOUT, 0.3 in² pour"]
    end

    subgraph CORE["CORE"]
        ESP["ESP32-WROOM-32E<br/>PCB antenna + keepout"]
        BTN["GPIO0 button<br/>WiFi portal trigger"]
        LED["Status LED<br/>(reserve a GPIO)"]
        UART["UART0 header<br/>+ auto-reset"]
    end

    subgraph SENSE["SENSOR CONNECTORS — all optional, all separate"]
        DHT["DHT22<br/>GPIO4 · fitted"]
        CO2["MH-Z19B CO₂<br/>UART2 · 5 V supply"]
        NH3["MQ-137 NH₃<br/>ADC1 GPIO35<br/>divider + 5 V heater"]
        LUX["BH1750 lux<br/>I2C"]
        US1["HC-SR04 feed<br/>5 V · echo level shift"]
        US2["HC-SR04 water<br/>5 V · echo level shift"]
    end

    IN --> P5 --> P33 --> ESP
    P5 --> CO2
    P5 --> NH3
    P5 --> US1
    P5 --> US2
    P33 --> DHT
    P33 --> LUX

    DHT --> ESP
    CO2 <--> ESP
    NH3 --> ESP
    LUX <--> ESP
    US1 <--> ESP
    US2 <--> ESP
    BTN --> ESP
    ESP --> LED
    UART <--> ESP

    ESP -.MQTT over WiFi, 60 s.-> CLOUD["Mosquitto on Radxa<br/>eggapp/devices/COOP_01"]
```

## Blocks

**Power input.** Source undecided and blocking —
[../README.md](../README.md#blocking-decisions). Whatever it is, the input
needs reverse-polarity protection and a TVS: this board is wired up in a shed,
by hand, possibly in the dark.

**Two rails.** 3.3 V for the ESP32, DHT22 and BH1750; 5 V for the MH-Z19B, the
MQ-137 heater and both HC-SR04s. The 5 V loads are the majority, which is why
the power decision cannot be deferred.

**ESP32-WROOM-32E.** PCB antenna, matching the incubator board. The keepout is
mandatory: no copper on any layer beneath it, module overhanging the board edge
([CONVENTIONS.md](../../CONVENTIONS.md#antenna-keepout)).

**This is the board where that choice carries risk.** The AP is typically in
the house and the node is in an outbuilding, possibly through a wall or two. A
PCB antenna has meaningfully less range than an external one, and the failure
mode is not a dead node — it is a node that associates, drops, and reconnects
all day, publishing intermittently. Range-test a module at the actual mounting
point before committing to layout. If it is marginal, the -32UE is a
same-family swap that is cheap now and a respin later.

**GPIO0 button.** The only field-accessible control on the device. See
[pin-map.md](pin-map.md#gpio0--the-provisioning-button).

**Status LED.** Not in the firmware yet, but reserve the GPIO and the footprint.
A sensor-only node with no display, no buttons and no screen gives the user
nothing to look at when it stops publishing.

**Sensor connectors.** One per sensor, each independently fittable. The
firmware compiles out absent channels and omits the field from the payload
entirely, so the consumer shows "no sensor" rather than a fault — the board
should support the same incremental build-up. A node with only a DHT22 fitted
is a valid, shippable configuration.

**Level shifting.** Both HC-SR04 echo lines. Mandatory, not optional; GPIO34
has no clamp. See [pin-map.md](pin-map.md#hc-sr04-level-shifting).

**MQ-137 divider.** Sized to keep the useful range in the ADC's linear middle.
This divider is part of the sensor's calibration — changing it invalidates
every prior reading.

## What this board deliberately does not have

No relays, no mains switching, no control outputs, no display. Per
[ADR 0009](../../../docs/architecture/adr/0009-coop-monitoring-devices.md) the
coop node cannot energise anything. If a future requirement wants a coop-side
actuator — a fan, a light, an auto-door — that is a new board and a new ADR,
not a populate option on this one.

## Open questions

- Power source (blocking everything else).
- Whether the sensors mount on the board or on flying leads. Ammonia and dust
  argue for a sealed box with remote sensor heads; cost and connector count
  argue the other way. This decides the connector strategy.
- Ingress rating and how the sensors see coop air without the electronics
  seeing it too.
