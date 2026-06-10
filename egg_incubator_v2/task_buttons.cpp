#include "task_buttons.h"
#include "globals.h"
#include "config.h"
#include <ezButton.h>
#include <Arduino.h>
#include <Preferences.h>

static ezButton btnUp(BTN_UP);
static ezButton btnDown(BTN_DOWN);
static ezButton btnOk(BTN_OK);

// ─────────────────────────────────────────────────────────────────────────────
// TASK: BUTTON HANDLER
//
// Polls buttons every 10 ms. In normal mode, sends UiEvent to uiEventQueue.
// In fault mode, only monitors OK for a 3-second hold to reset the fault.
// ─────────────────────────────────────────────────────────────────────────────
void task_buttons(void* pvParameters) {

    btnUp.setDebounceTime(50);
    btnDown.setDebounceTime(50);
    btnOk.setDebounceTime(50);

    unsigned long okPressStartMs = 0;
    bool          okHeld         = false;

    for (;;) {
        btnUp.loop();
        btnDown.loop();
        btnOk.loop();

        // ── FAULT MODE ───────────────────────────────────────────────────────
        bool fault = false;
        portENTER_CRITICAL(&faultMux);
        fault = overTempFault;
        portEXIT_CRITICAL(&faultMux);

        if (fault) {

            if (btnOk.isPressed()) {
                okPressStartMs = millis();
                okHeld         = true;
            }

            if (btnOk.isReleased() && okHeld) {
                if ((millis() - okPressStartMs) >= FAULT_RESET_HOLD_MS) {

                    // Reset fault under critical section
                    portENTER_CRITICAL(&faultMux);
                    overTempFault = false;
                    portEXIT_CRITICAL(&faultMux);

                    { Preferences p; p.begin("incubator", false); p.putBool("otFault", false); p.end(); }

                    allRelaysOff();
                    pushError("FAULT_RESET", "Manual fault reset by user");
                    Serial.println("[FAULT] Reset by user");
                }
                okHeld = false;
            }

            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // ── NORMAL MODE ──────────────────────────────────────────────────────
        UiEvent evt;

        if (btnUp.isPressed()) {
            evt = UI_EVT_UP;
            xQueueSend(uiEventQueue, &evt, 0);
            Serial.printf("[BTN] UP pressed -> queued UI_EVT_UP (ms=%lu)\n", millis());
        }

        if (btnDown.isPressed()) {
            evt = UI_EVT_DOWN;
            xQueueSend(uiEventQueue, &evt, 0);
            Serial.printf("[BTN] DOWN pressed -> queued UI_EVT_DOWN (ms=%lu)\n", millis());
        }

        if (btnOk.isPressed()) {
            evt = UI_EVT_OK;
            xQueueSend(uiEventQueue, &evt, 0);
            Serial.printf("[BTN] OK pressed -> queued UI_EVT_OK (ms=%lu)\n", millis());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
