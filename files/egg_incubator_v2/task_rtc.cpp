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
        DateTime now = rtc.now();

        if (xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            gRtcTime.now   = now;
            gRtcTime.epoch = now.unixtime();
            xSemaphoreGive(rtcMutex);
        }

        static bool rtc_first = true;
        if (rtc_first) {
            rtc_first = false;
            Serial.printf("[RTC] nowEpoch = %lu\n", (unsigned long)gRtcTime.epoch);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
