/**
 * @file Lab2_2_Shared.h
 * @brief Pure-C shared data structures for Lab 2.2 FreeRTOS tasks
 */

#ifndef LAB2_2_SHARED_H
#define LAB2_2_SHARED_H

#include <stdint.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <queue.h>

/* ── Pin assignments ───────────────────────────────────────────── */
#define BTN_PIN             4
#define LED_GREEN_PIN       13
#define LED_RED_PIN         12
#define LED_YELLOW_PIN      11

/* ── Thresholds ────────────────────────────────────────────────── */
#define SHORT_THRESHOLD_MS  500
#define DEBOUNCE_SAMPLES    2
#define PRESS_EVENT_QUEUE_LEN  64

/* ── Shared data types ─────────────────────────────────────────── */
typedef struct {
    uint32_t durationMs;
    uint8_t  isShort;       /* 1 = short, 0 = long */
} PressEventData;

typedef struct {
    uint16_t totalPresses;
    uint16_t shortPresses;
    uint16_t longPresses;
    uint32_t totalDurationMs;
    uint32_t shortDurationMs;
    uint32_t longDurationMs;
} PressStats;

typedef struct {
    QueueHandle_t     pressEventQueue;
    SemaphoreHandle_t statsMutex;
    SemaphoreHandle_t ioMutex;
    PressStats        stats;
} SharedState;

/* Defined in Lab2_2_main.c */
extern SharedState g_shared;

#endif /* LAB2_2_SHARED_H */
