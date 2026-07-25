#pragma once

#include <Arduino.h>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "BleBackendObserver.h"
#include "BleClientBackend.h"
#include "BleWriteBridge.h"

enum class BleGattMirrorState
{
  Idle,
  Initializing,
  BackendReleasedForGatt,
  BuildingGattMirror,
  GattRegistered,
  BackendReconnecting,
  BackendReady,
  Disconnected,
  Advertising,
  CentralConnected,
  Error
};

/**
 * BleGattMirror
 *
 * Dynamic BLE GATT server mirror presented to the training application.
 *
 * Purpose
 * -------
 * Present the discovered trainer GATT surface as a local BLE peripheral so
 * Zwift or another central can interact with the MITM as if it were the trainer.
 *
 * Architectural position
 * ----------------------
 * Sits between the training app central and BleClientBackend. It owns the local
 * NimBLE server/GATT model and uses the backend plus BleWriteBridge to forward
 * traffic between the central and the trainer.
 *
 * Responsibilities
 * ----------------
 * - Build local services and characteristics from backend discovery.
 * - Configure advertising and scan response data.
 * - Serve central reads and forward central writes through the write bridge.
 * - Track central subscriptions and restore backend subscriptions after trainer
 *   reconnect.
 * - Forward trainer notifications to subscribed central characteristics.
 * - Track mirror workflow, central connection, and backend connection effects.
 *
 * Design notes
 * ------------
 * The mirror keeps GATT behavior general and leaves protocol-specific write
 * observation to passive observer code. State transitions are used for display
 * presentation, but presentation work is deferred through update paths instead
 * of being done directly inside NimBLE callbacks.
 */
class BleGattMirror : public BleBackendObserver
{
public:
  explicit BleGattMirror(BleClientBackend* backend);
  explicit BleGattMirror(BleClientBackend& backend);

  bool start();
  void update();
  bool isConnected() const;

  ~BleGattMirror();
  static NimBLEServer* pServer; // Pointer to NimBLEServer instance
  void serverConnectionCallbacksOnConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo);
  void serverConnectionCallbacksOnDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason);

  void init(void);
  void startAdvertising(void);
  void setupAdvertising(void);
  BleGattMirrorState getState() const;

  void onNotification(
      const NimBLEUUID& service,
      const NimBLEUUID& characteristic,
      const uint8_t* data,
      size_t length) override;

  void onBackendReady() override;
  void onBackendDisconnected() override;

private:
  class ServerCallbacks;
  class CharacteristicCallbacks;

  struct MirroredCharacteristic
  {
    NimBLEUUID service;
    NimBLEUUID characteristic;
    NimBLECharacteristic* local = nullptr;
    uint32_t properties = 0;
    bool centralSubscribed = false;
    bool centralWantsNotify = false;
    bool centralWantsIndicate = false;
    bool backendSubscribed = false;
    bool pendingSubscriptionUpdate = false;
  };

  bool buildGattMirror();
  void clearGattMirror();
  bool addAdvertisedServices();
  void processPendingBackendOperations();
  void refreshGapValues();
  void refreshReadableValues();
  void handleBackendReadyState(bool restoreSubscriptions);
  void restoreClientSubscriptions();
  void handleRead(MirroredCharacteristic* mirror);
  void handleWrite(MirroredCharacteristic* mirror);
  void handleSubscribe(MirroredCharacteristic* mirror, uint16_t subValue);
  void processPendingPresentationState();
  MirroredCharacteristic* findMirror(const NimBLEUUID& service, const NimBLEUUID& characteristic);
  static std::string canonicalUuidString(const NimBLEUUID& uuid);
  static std::string makeKey(const NimBLEUUID& service, const NimBLEUUID& characteristic);
  void setState(BleGattMirrorState newState);

  NimBLEAdvertising *pAdvertising = nullptr;
  BleClientBackend* backend = nullptr;
  BleWriteBridge* writeBridge = nullptr;
  ServerCallbacks* serverCallbacks = nullptr;
  BleGattMirrorState state = BleGattMirrorState::Idle;
  std::string gapDeviceName = "NimBLE-Server";
  uint16_t gapAppearance = 1152;
  bool gattMirrorBuilt = false;
  bool backendHasBeenReady = false;
  bool forcedCentralDisconnect = false;
  bool centralConnected = false;
  bool hasPendingPresentationState = false;
  BleGattMirrorState pendingPresentationFrom = BleGattMirrorState::Idle;
  BleGattMirrorState pendingPresentationTo = BleGattMirrorState::Idle;

  std::deque<MirroredCharacteristic> mirroredCharacteristics;
  std::deque<CharacteristicCallbacks*> characteristicCallbacks;
  std::map<std::string, MirroredCharacteristic*> mirrorLookup;
};
