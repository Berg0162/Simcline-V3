#pragma once


#include <NimBLEDevice.h>

/**
 * BleBackendObserver
 *
 * Observer interface for BLE trainer-side events emitted by BleClientBackend.
 *
 * Purpose
 * -------
 * Decouple the trainer client from the component that mirrors trainer data to a
 * connected central application.
 *
 * Architectural position
 * ----------------------
 * Implemented by BleGattMirror and called by BleClientBackend when the trainer
 * connection becomes ready, disconnects, or emits notifications.
 *
 * Responsibilities
 * ----------------
 * - Report backend readiness and backend disconnection.
 * - Deliver trainer notifications with service and characteristic UUID context.
 * - Keep BleClientBackend independent from a specific server-side mirror.
 *
 * Design notes
 * ------------
 * Callbacks are intentionally small and synchronous. Implementations should keep
 * callback work lightweight and defer heavier processing to queued or update
 * paths.
 */
class BleBackendObserver {
public:
    virtual ~BleBackendObserver() = default;

    virtual void onNotification(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic,
        const uint8_t* data,
        size_t length) {}
        
    virtual void onBackendReady() {}
    virtual void onBackendDisconnected() {}
};
