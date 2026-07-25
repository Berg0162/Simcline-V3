#include "BleWriteBridge.h"

#define DEBUG
#include "config/configDebug.h"

BleWriteBridge::BleWriteBridge(BleClientBackend& backend) : backend(backend)
{}

BleWriteBridge::~BleWriteBridge()
{
    if(taskHandle != nullptr) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }

    if(queueMutex != nullptr) {
        vSemaphoreDelete(queueMutex);
        queueMutex = nullptr;
    }

    if(queueSignal != nullptr) {
        vSemaphoreDelete(queueSignal);
        queueSignal = nullptr;
    }
}

bool BleWriteBridge::begin()
{
    if(taskHandle != nullptr) {
        return true;
    }

    // Mutex protects std::deque; binary semaphore wakes the worker on demand.
    queueMutex = xSemaphoreCreateMutex();
    queueSignal = xSemaphoreCreateBinary();

    if(queueMutex == nullptr || queueSignal == nullptr) {
        LOG("BleWriteBridge: failed to create FreeRTOS primitives");
        return false;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "BleWriteBridge",
        TASK_STACK_WORDS,
        this,
        TASK_PRIORITY,
        &taskHandle,
        xTaskCoreID0);

    if(result != pdPASS) {
        LOG("BleWriteBridge: failed to create worker task");
        taskHandle = nullptr;
        return false;
    }

    return true;
}

bool BleWriteBridge::enqueueWrite(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic,
    const uint8_t* data,
    size_t length,
    BleWriteKind kind)
{
    if(data == nullptr && length > 0) {
        return false;
    }

    if(queueMutex == nullptr || queueSignal == nullptr) {
        return false;
    }

    PendingWrite write;
    write.service = service;
    write.characteristic = characteristic;
    // Copy payload before the NimBLE callback returns; its value buffer is not
    // treated as stable after the local callback path completes.
    if(length > 0) {
        write.data.assign(data, data + length);
    }
    write.kind = kind;
    write.enqueuedAtMs = millis();

    if(xSemaphoreTake(queueMutex, 0) != pdTRUE) {
        LOG("BleWriteBridge: queue busy, dropping write");
        return false;
    }

    if(queue.size() >= WRITE_QUEUE_DEPTH) {
        xSemaphoreGive(queueMutex);
        LOG("BleWriteBridge: queue full, dropping write");
        return false;
    }

    queue.push_back(write);
    xSemaphoreGive(queueMutex);

    // Passive write observation starts only after this write has been accepted
    // into the forwarding queue. The observer must not alter the queued write.
    observerHub.observeWrite(
        write.service,
        write.characteristic,
        write.data.empty() ? nullptr : write.data.data(),
        write.data.size(),
        write.kind);
    xSemaphoreGive(queueSignal);
    return true;
}

void BleWriteBridge::taskEntry(void* arg)
{
    BleWriteBridge* bridge = static_cast<BleWriteBridge*>(arg);
    if(bridge != nullptr) {
        bridge->taskLoop();
    }

    vTaskDelete(nullptr);
}

void BleWriteBridge::taskLoop()
{
    while(true) {
        // Sleep when idle. This avoids depending on Arduino loop() scheduling
        // and wakes immediately when the server-side callback enqueues a write.
        xSemaphoreTake(queueSignal, portMAX_DELAY);

        PendingWrite write;
        while(popWrite(write)) {
            if(!backend.isReady()) {
                // Preserve ordering across reconnects: the front write remains
                // first once the trainer link is ready again.
                requeueFront(write);
                vTaskDelay(BACKEND_WAIT_TICKS);
                break;
            }

            const bool withResponse = (write.kind == BleWriteKind::WithResponse);
            const uint8_t* payload = write.data.empty() ? nullptr : write.data.data();
            bool success = backend.writeCharacteristic(
                write.service,
                write.characteristic,
                payload,
                write.data.size(),
                withResponse);

            if(!success) {
                LOG("BleWriteBridge: write failed for %s", write.characteristic.toString().c_str());
            }

            // Give the NimBLE host and other bridge work a chance between
            // writes during bursts.
            taskYIELD();
        }
    }
}

bool BleWriteBridge::popWrite(PendingWrite& write)
{
    if(queueMutex == nullptr) {
        return false;
    }

    if(xSemaphoreTake(queueMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    if(queue.empty()) {
        xSemaphoreGive(queueMutex);
        return false;
    }

    write = queue.front();
    queue.pop_front();

    const bool hasMore = !queue.empty();
    xSemaphoreGive(queueMutex);

    // Binary semaphores do not count queue length, so re-signal when more
    // entries remain after this pop.
    if(hasMore && queueSignal != nullptr) {
        xSemaphoreGive(queueSignal);
    }

    return true;
}

bool BleWriteBridge::requeueFront(const PendingWrite& write)
{
    if(queueMutex == nullptr || queueSignal == nullptr) {
        return false;
    }

    if(xSemaphoreTake(queueMutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    queue.push_front(write);
    xSemaphoreGive(queueMutex);
    xSemaphoreGive(queueSignal);
    return true;
}
