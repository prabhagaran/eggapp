#include <Arduino.h>

/* ========== QUEUE & TASK HANDLES ========== */
QueueHandle_t sensorQueue;
TaskHandle_t safetyTaskHandle;

/* ========== DATA STRUCT ========== */
typedef struct {
  float temperature;
} SensorData_t;

/* ========== TASKS ========== */

void SensorTask(void *pvParameters) {
  SensorData_t data;
  float temp = 25.0;

  while (1) {
    temp += 1.0;
    if (temp > 45.0) temp = 25.0;

    data.temperature = temp;

    Serial.print("Sensor -> Temp: ");
    Serial.println(temp);

    xQueueSend(sensorQueue, &data, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void ControlTask(void *pvParameters) {
  SensorData_t rx;

  while (1) {
    xQueueReceive(sensorQueue, &rx, portMAX_DELAY);

    Serial.print("Control <- Temp: ");
    Serial.println(rx.temperature);

    if (rx.temperature > 35.0) {
      Serial.println("⚠ OVER TEMP! Notify SafetyTask");

      xTaskNotify(
        safetyTaskHandle,
        0x01,          // notification bit
        eSetBits
      );
    }
  }
}

void SafetyTask(void *pvParameters) {
  uint32_t notifyValue;

  while (1) {
    // BLOCK until notified
    xTaskNotifyWait(
      0x00,
      0xFFFFFFFF,
      &notifyValue,
      portMAX_DELAY
    );

    if (notifyValue & 0x01) {
      Serial.println("🚨 SAFETY: Heater OFF! System shutdown!");
    }
  }
}

/* ========== SETUP ========== */

void setup() {
  Serial.begin(115200);

  sensorQueue = xQueueCreate(5, sizeof(SensorData_t));

  xTaskCreate(SensorTask,  "SensorTask",  2048, NULL, 2, NULL);
  xTaskCreate(ControlTask, "ControlTask", 2048, NULL, 3, NULL);
  xTaskCreate(SafetyTask,  "SafetyTask",  2048, NULL, 5, &safetyTaskHandle);
}

void loop() {
  // Empty
}
