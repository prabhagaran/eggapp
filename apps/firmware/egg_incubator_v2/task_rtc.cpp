#include "task_rtc.h"
#include "globals.h"
#include "config.h"
#include <Arduino.h>
#include "RTClib.h"

extern RTC_DS1307 rtc;  // defined in main .ino

// ─────────────────────────────────────────────────────────────────────────────
// TASK: RTC POLLING
// Updates gRtcTime every second under rtcMutex.
// ─────────────────────────────────────────────────────────────────────────────
void task_rtc(void* pvParameters) {
    for (;;) {
        DateTime now   = rtc.now();
        uint32_t epoch = now.unixtime();  // local copy — used outside the mutex

        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            gRtcTime.now   = now;
            gRtcTime.epoch = epoch;
            xSemaphoreGive(rtcMutex);

            // Epoch sanity: DS1307 power-loss returns 2000-01-01 (epoch ~946684800)
            static unsigned long lastEpochErrMs = 0;
            if (epoch < 1700000000UL) {
                rtcEpochValid = false;
                if (millis() - lastEpochErrMs > 30000UL) {
                    lastEpochErrMs = millis();
                    pushError("FAULT", "RTC epoch invalid");
                }
            } else {
                rtcEpochValid = true;
            }
        }

        static bool rtc_first = true;
        if (rtc_first) {
            rtc_first = false;
            Serial.printf("[RTC] nowEpoch = %lu\n", (unsigned long)epoch);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
