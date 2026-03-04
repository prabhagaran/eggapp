#ifndef CONFIG_H
#define CONFIG_H

// ─────────────────────────────────────────────────────────────────────────────
// DEVICE IDENTITY
// ─────────────────────────────────────────────────────────────────────────────
#define DEVICE_ID          "INCUBATOR_01"
#define FW_VERSION         "2.0.0"

// ─────────────────────────────────────────────────────────────────────────────
// INPUT PINS — Buttons (active-LOW with internal pull-up)
// ─────────────────────────────────────────────────────────────────────────────
#define BTN_UP             32
#define BTN_DOWN           33
#define BTN_OK             25

// ─────────────────────────────────────────────────────────────────────────────
// I2C BUS (shared by OLED + RTC DS1307)
// ─────────────────────────────────────────────────────────────────────────────
#define I2C_SDA            21
#define I2C_SCL            22

// ─────────────────────────────────────────────────────────────────────────────
// SENSOR PINS
// ─────────────────────────────────────────────────────────────────────────────
#define DHT_PIN            4
#define DHT_TYPE           DHT11
#define DS18B20_PIN        18

// ─────────────────────────────────────────────────────────────────────────────
// RELAY OUTPUT PINS
// ─────────────────────────────────────────────────────────────────────────────
#define RELAY_HEATER       26
#define RELAY_COOLER       27
#define RELAY_HUMIDIFIER   14
#define RELAY_FAN          13
#define RELAY_PUMP         12
#define RELAY_TURNER       15

// Active-LOW relay board
#define RELAY_ON           LOW
#define RELAY_OFF          HIGH

// ─────────────────────────────────────────────────────────────────────────────
// OLED DISPLAY
// ─────────────────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      64
#define OLED_RESET         -1
#define OLED_ADDR          0x3C

// ─────────────────────────────────────────────────────────────────────────────
// SAFETY LIMITS
// ─────────────────────────────────────────────────────────────────────────────
#define MAX_TEMP_INCUBATOR    39.5f   // °C — over-temp fault triggers above this
#define MAX_TEMP_CLIMATE      80.0f   // °C — climate chamber absolute limit
#define SENSOR_INVALID_LOW   -10.0f   // below this = sensor fault
#define SENSOR_INVALID_HIGH   85.0f   // above this = sensor fault
#define FAULT_RESET_HOLD_MS   3000    // ms hold time for OK button to reset fault

// ─────────────────────────────────────────────────────────────────────────────
// DEFAULT SETPOINTS
// ─────────────────────────────────────────────────────────────────────────────
#define DEFAULT_TEMP_SETPOINT     37.5f
#define DEFAULT_TEMP_HYSTERESIS    0.3f
#define DEFAULT_HUM_SETPOINT      60.0f
#define DEFAULT_HUM_HYSTERESIS     3.0f
#define DEFAULT_LOCKDOWN_HUM      70.0f  // humidity raised at lockdown

// ─────────────────────────────────────────────────────────────────────────────
// EGG TYPE INCUBATION PARAMETERS
// ─────────────────────────────────────────────────────────────────────────────
#define CHICKEN_TOTAL_DAYS     21
#define CHICKEN_LOCKDOWN_DAY   18
#define CHICKEN_DEFAULT_TEMP   37.5f
#define CHICKEN_DEFAULT_HUM    60.0f

#define DUCK_TOTAL_DAYS        28
#define DUCK_LOCKDOWN_DAY      25
#define DUCK_DEFAULT_TEMP      37.5f
#define DUCK_DEFAULT_HUM       75.0f

#define QUAIL_TOTAL_DAYS       17
#define QUAIL_LOCKDOWN_DAY     14
#define QUAIL_DEFAULT_TEMP     37.5f
#define QUAIL_DEFAULT_HUM      60.0f

// ─────────────────────────────────────────────────────────────────────────────
// EGG TURNER DEFAULTS
// ─────────────────────────────────────────────────────────────────────────────
#define DEFAULT_TURNER_INTERVAL_MIN   240   // 4 hours
#define DEFAULT_TURNER_DURATION_SEC    30   // 30 seconds
#define TURNER_MIN_INTERVAL_MIN         1   // 60 min minimum allowed
#define TURNER_MAX_INTERVAL_MIN       720   // 12 hours maximum
#define TURNER_MIN_DURATION_SEC        10
#define TURNER_MAX_DURATION_SEC       120

// ─────────────────────────────────────────────────────────────────────────────
// FAN DEFAULTS
// ─────────────────────────────────────────────────────────────────────────────
#define DEFAULT_FAN_INTERVAL_MIN       60   // every 60 minutes
#define DEFAULT_FAN_DURATION_SEC      300   // on for 5 minutes
// Default PWM speed percent for fan (0-100)
#define DEFAULT_FAN_SPEED_PERCENT      100

// ─────────────────────────────────────────────────────────────────────────────
// PUMP DEFAULTS
// ─────────────────────────────────────────────────────────────────────────────
#define DEFAULT_PUMP_DURATION_SEC      10
#define PUMP_COOLDOWN_SEC             300   // 5-minute minimum between pump runs
// Pump triggers when humidity is this many % below (setpoint - hysteresis)
#define PUMP_TRIGGER_EXTRA_DEFICIT     5.0f

// Humidity calibration offset (additive, in %RH). Use to correct a consistent
// bias in the DHT readings. Positive values raise reported humidity.
#define HUM_CALIB_OFFSET               0.0f

// Enable raw DHT read debug output over Serial (1 = enabled, 0 = disabled)
#define DHT_DEBUG_RAW                  0
// ─────────────────────────────────────────────────────────────────────────────
// CLIMATE CHAMBER DEFAULTS
// ─────────────────────────────────────────────────────────────────────────────
#define DEFAULT_CLIMATE_HEAT_PERIOD_MIN  120  // 2 hours heat in cyclic mode
#define DEFAULT_CLIMATE_COOL_PERIOD_MIN  120  // 2 hours cool in cyclic mode
#define DEFAULT_SCHED_START_HOUR           8  // fixed schedule heat start
#define DEFAULT_SCHED_END_HOUR            20  // fixed schedule heat end
#define CLIMATE_MAX_RAMP_STEPS             8
#define COOLER_LOCKOUT_SEC                30  // min seconds between cooler toggling

// ─────────────────────────────────────────────────────────────────────────────
// CLOUD / TIMING
// ─────────────────────────────────────────────────────────────────────────────
#define CLOUD_TELEMETRY_INTERVAL_MS    60000UL   // 60 seconds
#define CLOUD_HEARTBEAT_INTERVAL_MS   300000UL   // 5 minutes
#define SENSOR_ERROR_THROTTLE_MS       60000UL   // min gap between same error pushes
#define HTTP_ERROR_THROTTLE_MS        300000UL   // 5 minutes
#define WIFI_PORTAL_TIMEOUT_SEC           180    // 3-minute WiFi config portal

#define ERROR_QUEUE_SIZE               20
#define UI_EVENT_QUEUE_SIZE            10

// Cloud HTTP retry/backoff configuration
#define CLOUD_HTTP_MAX_RETRIES          3
#define CLOUD_HTTP_BACKOFF_MS        2000UL  // base backoff in ms (exponential)

// ─────────────────────────────────────────────────────────────────────────────
// SETPOINT EDITING LIMITS
// ─────────────────────────────────────────────────────────────────────────────
#define TEMP_SETPOINT_MIN     25.0f
#define TEMP_SETPOINT_MAX     50.0f
#define TEMP_SETPOINT_STEP     0.1f
#define TEMP_HYST_MIN          0.1f
#define TEMP_HYST_MAX          2.0f
#define HUM_SETPOINT_MIN      30.0f
#define HUM_SETPOINT_MAX      95.0f
#define HUM_SETPOINT_STEP      1.0f
#define HUM_HYST_MIN           1.0f
#define HUM_HYST_MAX          10.0f

// ─────────────────────────────────────────────────────────────────────────────
// GOOGLE APPS SCRIPT URL — replace with your deployment URL
// ─────────────────────────────────────────────────────────────────────────────
#define GOOGLE_SCRIPT_URL \
  "https://script.google.com/macros/s/AKfycbyA-CXZL_GveJRRvon-KZzAfsdXJidMS27sZOz1lDj68bImDuDqWnH8d-nfqzCR_pUi3w/exec"

// Optional shared secret token (empty string = disabled)
// Generated token for cloud uploads
#define CLOUD_TOKEN  "a1b2c3d4e5f60718293a4b5c6d7e8f90"

#endif // CONFIG_H
