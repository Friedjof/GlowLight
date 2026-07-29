// Host shim for the FreeRTOS core types used by CommunicationService.
#ifndef GLOW_SHIM_FREERTOS_H
#define GLOW_SHIM_FREERTOS_H

#include <cstdint>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xffffffffUL

#endif
