#pragma once

#include <Arduino.h>
#include <map>
#include <deque>
#include <vector>
#include <string>
#include <NimBLEDevice.h>

#include "GattCharacteristic.h"
#include "BleBackendObserver.h"

enum class BleClientBackendState
{
    Idle,
    Scanning,
    ScanTimeout,
    Connecting,
    Discovering,
    Ready,
    GattRegistrationPause,
    Disconnected,
    ReconnectingWait,
    Error
};

/**
 * BleClientBackend
 *
 * BLE central/client implementation that connects to the real smart trainer.
 *
 * Purpose
 * -------
 * Maintain the trainer-side BLE connection and expose a dynamically discovered
 * GATT view to the rest of the MITM.
 *
 * Architectural position
 * ----------------------
 * Sits on the trainer side of the bridge. BleGattMirror uses its discovery,
 * read, write, subscribe, unsubscribe, and notification paths to build and
 * operate the local GATT mirror presented to Zwift or another training app.
 *
 * Responsibilities
 * ----------------
 * - Scan for and connect or reconnect to the configured trainer.
 * - Apply trainer MAC address policy through Operations.
 * - Discover services and characteristics without fixed trainer profiles.
 * - Cache service, characteristic, property, and remote characteristic mappings.
 * - Execute trainer reads, writes, subscribes, and unsubscribes.
 * - Forward trainer notifications and link-state changes to BleBackendObserver.
 *
 * Design notes
 * ------------
 * The backend favors dynamic GATT discovery so different trainers can be
 * mirrored with minimal UUID-specific code. After reconnect, the trainer GATT
 * cache is rebuilt and the server side is expected to restore subscriptions
 * that the central application still believes are active.
 */
class BleClientBackend
{
public:
    // Constructor
    BleClientBackend();

    void setObserver(BleBackendObserver* observer);
    bool start();
    void update();

    bool isConnected() const;
    bool isReady() const;
    bool isDiscoveryComplete() const;
    bool releaseConnectionForGattRegistration();
    bool reconnectUsingCachedTrainer();

   // ----- Operations -----

    bool readCharacteristic(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic,
        std::string& value);

    bool subscribeCharacteristic(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic);

    bool unsubscribeCharacteristic(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic);

    bool writeCharacteristic(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic,
        const uint8_t* data,
        size_t length,
        bool withResponse);

    // ----- DirCon discovery interface -----

    std::vector<NimBLEUUID> getServices();
    std::vector<NimBLEUUID> getAdvertisedServices();
    std::vector<GattCharacteristic> getCharacteristics(const NimBLEUUID& service);

    bool hasCharacteristic(const NimBLEUUID& characteristic) const;

    bool getServiceForCharacteristic(
        const NimBLEUUID& characteristic,
        NimBLEUUID& serviceOut);

    uint32_t getCharacteristicProperties(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic);
    
    bool isSubscribed(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic);
private:
    void clearDiscoveryCache();
    bool BleScanServer();
    bool BleServerConnect();
    void setState(BleClientBackendState newState);
    void processPendingPresentationState();

    void handleDisconnect(void);
    bool establishConnection(void);
    bool discoverServices();   
    bool discoverCharacteristics();

    static std::string canonicalUuidString(const NimBLEUUID& uuid);

    GattCharacteristic* findCharacteristic(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic);

    static void notifyCallback(
        NimBLERemoteCharacteristic* characteristic,
        uint8_t* data,
        size_t length,
        bool isNotify);

    static std::string makeKey(const NimBLEUUID& service,
                    const NimBLEUUID& characteristic);

private:
    static BleClientBackend* instance;
    NimBLEClient* client = nullptr;
    const NimBLEAdvertisedDevice* targetDevice;
    BleBackendObserver* observer = nullptr;

    // Status
    BleClientBackendState getState() const;
    BleClientBackendState state = BleClientBackendState::Idle;
    bool hasPendingPresentationState = false;
    BleClientBackendState pendingPresentationFrom = BleClientBackendState::Idle;
    BleClientBackendState pendingPresentationTo = BleClientBackendState::Idle;
    bool discoveryComplete = false;
    bool hadConnection = false;

    // Reconnect
    uint8_t reconnectAttemptCount = 0;
    unsigned long reconnectAttemptDueMs = 0;
    static constexpr uint8_t RECONNECT_FAST_RETRY_LIMIT = 3;
    static constexpr unsigned long RECONNECT_DELAY_MS_FAST = 1000;
    static constexpr unsigned long RECONNECT_DELAY_MS_SLOW = 5000;
    void scheduleReconnectRetry();

    // Cached GATT database
    std::vector<NimBLEUUID> services;
    std::vector<NimBLEUUID> advertisedServices;
    
    std::deque<GattCharacteristic> characteristics;
    std::map<std::string, GattCharacteristic*> characteristicLookup;
    std::map<std::string, GattCharacteristic*> characteristicByUuid;
    std::map<NimBLERemoteCharacteristic*, GattCharacteristic*> remoteLookup;

};
