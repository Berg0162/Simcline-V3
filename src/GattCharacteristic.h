#pragma once

/**
 * GattCharacteristic
 *
 * Internal representation of a BLE GATT characteristic within the MITM
 * Gatt architecture.
 *
 * Purpose
 * -------
 * Store the minimum metadata needed to identify, access, and mirror one
 * trainer-side characteristic discovered by BleClientBackend.
 *
 * Architectural position
 * ----------------------
 * Owned by the backend discovery cache and consumed by BleGattMirror when it
 * builds the local GATT database. It is the small shared record that links a
 * discovered remote characteristic to the mirrored server-side characteristic.
 *
 * Responsibilities
 * ----------------
 * - Preserve the service UUID and characteristic UUID pair.
 * - Keep the NimBLE remote characteristic pointer used for backend operations.
 * - Store characteristic properties discovered from the trainer.
 * - Track backend subscription state for the discovered characteristic.
 *
 * Design notes
 * ------------
 * The same characteristic UUID may appear under different services, so the
 * service UUID is part of the identity. This type intentionally contains data
 * only; policy, protocol decoding, and BLE operation sequencing live in the
 * backend, mirror, write bridge, and observer components.
 *
 */

#include <NimBLEDevice.h>

struct GattCharacteristic
{
    NimBLEUUID service;
    NimBLEUUID characteristic;

    NimBLERemoteCharacteristic* remote = nullptr;

    uint32_t properties = 0;

    bool isSubscribed = false;
};
