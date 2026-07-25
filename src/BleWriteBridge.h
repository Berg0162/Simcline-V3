#pragma once

/**
 * BleWriteBridge
 *
 * Asynchronous write pipeline between the local GATT mirror and the trainer
 * backend.
 *
 * Purpose
 * -------
 * Keep NimBLE server write callbacks short while preserving ordered forwarding
 * of central write commands to the trainer.
 *
 * Architectural position
 * ----------------------
 * Owned by BleGattMirror and backed by BleClientBackend. Server-side
 * characteristic callbacks enqueue writes here, and a FreeRTOS worker forwards
 * them to the trainer-side client connection.
 *
 * Responsibilities
 * ----------------
 * - Copy write payloads before the NimBLE callback returns.
 * - Preserve write ordering through a bounded FIFO queue.
 * - Distinguish Write With Response from Write Without Response.
 * - Pass writes through BleWriteObserverHub for passive observation.
 * - Serialize writes to BleClientBackend when the backend is ready.
 *
 * Design notes
 * ------------
 * This class is a transport bridge, not a protocol interpreter. Observation is
 * passive and must not alter the payload or decide whether the trainer receives
 * the write. The queue is deliberately bounded to protect RAM during bursts.
 */

#include <Arduino.h>
#include <deque>
#include <vector>
#include <NimBLEDevice.h>

#include "BleClientBackend.h"
#include "BleWriteObserverHub.h"

class BleWriteBridge
{
public:
    explicit BleWriteBridge(BleClientBackend& backend);
    ~BleWriteBridge();

    bool begin();

    bool enqueueWrite(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic,
        const uint8_t* data,
        size_t length,
        BleWriteKind kind);

private:
    struct PendingWrite
    {
        NimBLEUUID service;
        NimBLEUUID characteristic;
        std::vector<uint8_t> data;
        BleWriteKind kind = BleWriteKind::WithResponse;
        unsigned long enqueuedAtMs = 0;
    };

    static void taskEntry(void* arg);
    void taskLoop();
    bool popWrite(PendingWrite& write);
    bool requeueFront(const PendingWrite& write);

    BleClientBackend& backend;
    BleWriteObserverHub observerHub;
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t queueMutex = nullptr;
    SemaphoreHandle_t queueSignal = nullptr;
    std::deque<PendingWrite> queue;

    // Bounded FIFO: protects RAM while preserving short write bursts.
    static constexpr uint8_t WRITE_QUEUE_DEPTH = 16;
    static constexpr uint32_t TASK_STACK_WORDS = 4096;
    // Keep below NimBLE host priority; the worker should not starve BLE internals.
    static constexpr UBaseType_t TASK_PRIORITY = 2;
    static constexpr TickType_t BACKEND_WAIT_TICKS = pdMS_TO_TICKS(20);
    static constexpr BaseType_t xTaskCoreID0 = 0; // Run on Core #0
};
