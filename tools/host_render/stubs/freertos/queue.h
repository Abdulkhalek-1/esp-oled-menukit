#pragma once

#include "FreeRTOS.h"

typedef void *QueueHandle_t;

static inline BaseType_t xQueueReceive(QueueHandle_t q, void *buf, TickType_t wait)
{
    (void)q;
    (void)buf;
    (void)wait;
    return pdFALSE;
}
