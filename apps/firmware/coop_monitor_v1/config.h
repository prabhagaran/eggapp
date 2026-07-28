#ifndef CONFIG_H
#define CONFIG_H

// ─────────────────────────────────────────────────────────────────────────────
// Coop monitoring node — ADR 0009.
//
// A sensor-only device for the bird house. It drives no relays, runs no
// control loop and has no display: it reads what it can, publishes
// profile:"COOP" telemetry on the shared topic tree, and sleeps. Anything
// resembling incubator behaviour belongs in ../egg_incubator_v2/, not here.
// ─────────────────────────────────────────────────────────────────────────────

#define FW_VERSION            "1.0.0"

// Must match the Device.hardwareId registered in the app before telemetry
// is accepted — BR-007: an unknown id is logged and dropped, never
// auto-registered. Change per physical unit.
#define DEVICE_ID             "COOP_01"

// ─────────────────────────────────────────────────────────────────────────────
// MQTT — same broker, same topic shape as the incubator (mqtt-topics.md)
// ─────────────────────────────────────────────────────────────────────────────
// MQTT_BROKER_HOST/PORT/USERNAME/PASSWORD come from secrets.h (gitignored),
// same split as the incubator sketch — deployment-specific credentials
// never live in a tracked file.
#define MQTT_TOPIC_PREFIX     "eggapp/devices"
#define MQTT_BUFFER_SIZE      512
#define MQTT_KEEPALIVE_SEC    60
#define MQTT_RECONNECT_BACKOFF_MS  5000UL
#define MQTT_TELEMETRY_INTERVAL_MS 60000UL   // 60 s, matching the incubator

// ─────────────────────────────────────────────────────────────────────────────
// SENSORS — all optional, all default to "not fitted".
//
// A disabled channel is compiled out entirely: no pin is claimed, no read
// is attempted, and the field is omitted from the payload so the consumer
// shows "no sensor" rather than a fault. Enable a flag only once the
// hardware is physically attached.
// ─────────────────────────────────────────────────────────────────────────────
#define HAS_TEMP_HUM          1     // DHT22 — the one sensor assumed present
#define HAS_CO2_SENSOR        0     // MH-Z19B NDIR, UART2
#define HAS_NH3_SENSOR        0     // MQ-137, analog
#define HAS_LIGHT_SENSOR      0     // BH1750, I2C
#define HAS_FEED_LEVEL        0     // HC-SR04 over the feed hopper
#define HAS_WATER_LEVEL       0     // HC-SR04 over the water reservoir

#define DHT_PIN               4
#define DHT_TYPE              DHT22

#define I2C_SDA               21
#define I2C_SCL               22

#define CO2_RX_PIN            16    // ESP32 RX <- MH-Z19B TX
#define CO2_TX_PIN            17    // ESP32 TX -> MH-Z19B RX
#define NH3_ADC_PIN           35    // input-only ADC1 — ADC2 is unusable while WiFi is on
#define FEED_TRIG_PIN         25
#define FEED_ECHO_PIN         33
#define WATER_TRIG_PIN        32
#define WATER_ECHO_PIN        34    // input-only; echo is read-only, so that's fine

// ─────────────────────────────────────────────────────────────────────────────
// Ultrasonic tank geometry — distance from the sensor face to the product
// at 100% full and at empty; percent is linear between the two.
//
// These are PER-INSTALLATION and must be measured on the real rig. The
// values below are placeholders: they will produce plausible-looking but
// wrong percentages if left as-is.
// ─────────────────────────────────────────────────────────────────────────────
#define FEED_FULL_CM          5.0f
#define FEED_EMPTY_CM         40.0f
#define WATER_FULL_CM         5.0f
#define WATER_EMPTY_CM        30.0f

// Plausibility gates — a reading outside these publishes as null.
#define TEMP_MIN_C            -10.0f
#define TEMP_MAX_C            60.0f
#define CO2_MIN_PPM           350.0f
#define CO2_MAX_PPM           5000.0f
#define NH3_MIN_PPM           0.0f
#define NH3_MAX_PPM           100.0f
#define LUX_MIN               0.0f
#define LUX_MAX               65535.0f

#define ULTRASONIC_TIMEOUT_US 25000UL   // ~4 m round trip; beyond = no echo
#define SENSOR_INTERVAL_MS    5000      // sensor poll period

#endif // CONFIG_H
