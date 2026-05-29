#pragma once

#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks)
{
    (void)ticks;
}

static inline BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t stack,
                                     void *arg, UBaseType_t prio, void *out)
{
    (void)fn;
    (void)name;
    (void)stack;
    (void)arg;
    (void)prio;
    (void)out;
    return pdTRUE;
}
