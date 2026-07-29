// Host shim for FreeRTOS queues: a plain ring buffer with the same semantics
// (fixed item size, fixed length, non-blocking when the timeout is zero).
#ifndef GLOW_SHIM_QUEUE_H
#define GLOW_SHIM_QUEUE_H

#include <cstddef>

#include "freertos/FreeRTOS.h"

struct GlowShimQueue;
typedef GlowShimQueue* QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t itemSize);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t ticksToWait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t ticksToWait);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t queue);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);

namespace glow_shim {
// Forces the next N xQueueCreate() calls to fail, to exercise the error path.
extern int queueCreateFailuresRemaining;
}  // namespace glow_shim

#endif
