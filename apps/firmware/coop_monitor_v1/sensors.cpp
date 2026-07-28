#include "sensors.h"

#if HAS_TEMP_HUM
  #include <DHT.h>
  static DHT dht(DHT_PIN, DHT_TYPE);
#endif

#if HAS_CO2_SENSOR
  #include <HardwareSerial.h>
  static HardwareSerial co2Serial(2);   // UART2
#endif

#if HAS_LIGHT_SENSOR
  #include <Wire.h>
  #include <BH1750.h>
  static BH1750 lightMeter;
  static bool   lightReady = false;
#endif

// ─────────────────────────────────────────────────────────────────────────────
// HC-SR04 ultrasonic distance in cm; -1.0f on timeout (no echo).
// ─────────────────────────────────────────────────────────────────────────────
#if HAS_FEED_LEVEL || HAS_WATER_LEVEL
static float readUltrasonicCm(uint8_t trigPin, uint8_t echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    unsigned long us = pulseIn(echoPin, HIGH, ULTRASONIC_TIMEOUT_US);
    if (us == 0) return -1.0f;
    return (float)us * 0.0343f / 2.0f;   // speed of sound, there and back
}

// Map distance onto 0-100% fill. The sensor sits at the top of the tank,
// so full = close and empty = far, hence fullCm < emptyCm and the
// inversion. Clamped: a reading slightly past either end is a real tank
// just outside its calibration marks, not an error.
static float distanceToPercent(float cm, float fullCm, float emptyCm) {
    if (cm < 0.0f) return -1.0f;
    float pct = (emptyCm - cm) / (emptyCm - fullCm) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MH-Z19B: 9-byte command/response over UART. Checksum is verified — a
// floating TX line otherwise reads as a plausible-looking low ppm value.
// ─────────────────────────────────────────────────────────────────────────────
#if HAS_CO2_SENSOR
static uint8_t mhz19Checksum(const uint8_t* packet) {
    uint8_t sum = 0;
    for (uint8_t i = 1; i < 8; i++) sum += packet[i];
    return 0xFF - sum + 1;
}

static float readCo2Ppm(void) {
    static const uint8_t CMD_READ[9] = { 0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79 };
    while (co2Serial.available()) co2Serial.read();   // drop stale bytes
    co2Serial.write(CMD_READ, sizeof(CMD_READ));

    uint8_t resp[9];
    unsigned long deadline = millis() + 200;
    uint8_t got = 0;
    while (got < 9 && millis() < deadline) {
        if (co2Serial.available()) resp[got++] = co2Serial.read();
    }
    if (got != 9)                           return -1.0f;
    if (resp[0] != 0xFF || resp[1] != 0x86) return -1.0f;
    if (resp[8] != mhz19Checksum(resp))     return -1.0f;

    return (float)((resp[2] << 8) | resp[3]);
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MQ-137 ammonia.
//
// WARNING: the raw ADC count is NOT ppm. A real conversion needs the
// sensor's Rs/R0 curve plus a clean-air calibration per unit, and the
// sensor needs a long burn-in before any of it means anything. The linear
// map below exists so the channel is wired end to end — it must be
// replaced with a proper curve before any reading here is trusted. Also
// flagged in telemetry-contract.md so it isn't mistaken for calibrated.
// ─────────────────────────────────────────────────────────────────────────────
#if HAS_NH3_SENSOR
static float readAmmoniaPpm(void) {
    int raw = analogRead(NH3_ADC_PIN);
    if (raw <= 0) return -1.0f;
    return (float)raw * (NH3_MAX_PPM / 4095.0f);
}
#endif

void sensorsBegin(void) {
#if HAS_TEMP_HUM
    dht.begin();
    delay(2000);   // DHT22 warm-up after power-on
#endif
#if HAS_CO2_SENSOR
    co2Serial.begin(9600, SERIAL_8N1, CO2_RX_PIN, CO2_TX_PIN);
#endif
#if HAS_LIGHT_SENSOR
    Wire.begin(I2C_SDA, I2C_SCL);
    lightReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    if (!lightReady) Serial.println("[SENSOR] BH1750 not found");
#endif
#if HAS_FEED_LEVEL
    pinMode(FEED_TRIG_PIN, OUTPUT);
    pinMode(FEED_ECHO_PIN, INPUT);
#endif
#if HAS_WATER_LEVEL
    pinMode(WATER_TRIG_PIN, OUTPUT);
    pinMode(WATER_ECHO_PIN, INPUT);
#endif
}

void sensorsRead(CoopReadings_t* out) {
    *out = CoopReadings_t{};   // every value 0, every validity bit false

#if HAS_TEMP_HUM
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && t >= TEMP_MIN_C && t <= TEMP_MAX_C) {
        out->temp_c = t;
        out->temp_valid = true;
    }
    if (!isnan(h) && h >= 0.0f && h <= 100.0f) {
        out->humidity_pct = h;
        out->hum_valid = true;
    }
#endif

#if HAS_CO2_SENSOR
    float co2 = readCo2Ppm();
    if (co2 >= CO2_MIN_PPM && co2 <= CO2_MAX_PPM) {
        out->co2_ppm = co2;
        out->co2_valid = true;
    }
#endif

#if HAS_NH3_SENSOR
    float nh3 = readAmmoniaPpm();
    if (nh3 >= NH3_MIN_PPM && nh3 <= NH3_MAX_PPM) {
        out->nh3_ppm = nh3;
        out->nh3_valid = true;
    }
#endif

#if HAS_LIGHT_SENSOR
    if (lightReady) {
        float lux = lightMeter.readLightLevel();
        if (lux >= LUX_MIN && lux <= LUX_MAX) {
            out->light_lux = lux;
            out->light_valid = true;
        }
    }
#endif

#if HAS_FEED_LEVEL
    float feed = distanceToPercent(readUltrasonicCm(FEED_TRIG_PIN, FEED_ECHO_PIN),
                                   FEED_FULL_CM, FEED_EMPTY_CM);
    if (feed >= 0.0f) { out->feed_pct = feed; out->feed_valid = true; }
#endif

#if HAS_WATER_LEVEL
    float water = distanceToPercent(readUltrasonicCm(WATER_TRIG_PIN, WATER_ECHO_PIN),
                                    WATER_FULL_CM, WATER_EMPTY_CM);
    if (water >= 0.0f) { out->water_pct = water; out->water_valid = true; }
#endif
}
