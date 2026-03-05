#ifndef TASK_INCUBATOR_H
#define TASK_INCUBATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

void task_turner(void* pvParameters);
void task_fan(void* pvParameters);
void setFanSpeed(uint8_t percent);
void task_pump(void* pvParameters);
void task_milestone(void* pvParameters);

// Shared milestone label for home screen — read under milestoneMutex
extern char gMilestoneLabel[24];
extern SemaphoreHandle_t milestoneMutex;

#endif
