#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

// Include operations control settings class
#include "Operations.h"

enum class BleWriteKind
{
    WithResponse,
    WithoutResponse
};

/**
 * BleWriteObserverHub
 *
 * Passive inspection point for BLE writes that pass through the MITM.
 *
 * Purpose
 * -------
 * Decode selected write commands for application-side observation, such as road
 * grade updates used by the lifter, without interfering with BLE forwarding.
 *
 * Architectural position
 * ----------------------
 * Called by BleWriteBridge while writes are being queued and forwarded from
 * BleGattMirror to BleClientBackend. It signals Operations but does not own BLE
 * transport state.
 *
 * Responsibilities
 * ----------------
 * - Route observed writes by service UUID and characteristic UUID.
 * - Decode supported passive protocols such as FTMS, Tacx FE-C, Wahoo CPS, and
 *   Zwift virtual shifting variants.
 * - Publish decoded road grade changes through Operations.
 * - Ignore unknown, unsupported, or malformed payloads safely.
 *
 * Design notes
 * ------------
 * The observer hub must never modify, delay, reject, or acknowledge a BLE write.
 * Keeping protocol-specific peeking here protects BleGattMirror and
 * BleClientBackend from growing trainer-specific branching.
 */
class BleWriteObserverHub
{
public:
    BleWriteObserverHub();
    ~BleWriteObserverHub();
    void observeWrite(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic,
        const uint8_t* data,
        size_t length,
        BleWriteKind kind);

private:
    OPS* operations; // Private pointer to store the singleton instance
    void observeFTMSWriteRequest(const uint8_t* data, size_t length);
    void observeTacxFecTxdWrite(const uint8_t* data, size_t length);
    void observeWahooControlPointWrite(const uint8_t* data, size_t length);
    void observeZwiftOldSyncRxWrite(const uint8_t* data, size_t length);
    void observeZwiftNewSyncRxWrite(const uint8_t* data, size_t length);
};
