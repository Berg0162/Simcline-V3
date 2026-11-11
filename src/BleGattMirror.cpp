#include "BleGattMirror.h"
#include "Operations.h"
#include "Presentation.h"

#if __has_include("services/gap/ble_svc_gap.h")
#include "services/gap/ble_svc_gap.h"
#elif __has_include("nimble/nimble/host/services/gap/include/services/gap/ble_svc_gap.h")
#include "nimble/nimble/host/services/gap/include/services/gap/ble_svc_gap.h"
#endif

#include "config/configNimBLE.h"
#define DEBUG
#include "config/configDebug.h"
#ifdef DEBUG
#define DEBUG_GATT_CREATE
//#define DEBUG_SET_GAP_VALUES
#endif

#define UUID16_SVC_GENERIC_ACCESS    NimBLEUUID((uint16_t)0x1800)
#define UUID16_SVC_GENERIC_ATTRIBUTE NimBLEUUID((uint16_t)0x1801)
#define UUID16_CHR_DEVICE_NAME       NimBLEUUID((uint16_t)0x2A00)
#define UUID16_CHR_APPEARANCE        NimBLEUUID((uint16_t)0x2A01)

NimBLEServer* BleGattMirror::pServer = nullptr;

class BleGattMirror::ServerCallbacks : public NimBLEServerCallbacks {
private:
  BleGattMirror* bgmInstance = nullptr;
public:
  explicit ServerCallbacks(BleGattMirror* instance) : bgmInstance(instance) {}

  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    if(bgmInstance) {
      bgmInstance->serverConnectionCallbacksOnConnect(pServer, connInfo);
    }

  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    if(bgmInstance) {
      bgmInstance->serverConnectionCallbacksOnDisconnect(pServer, connInfo, reason);
    }
  }
};

class BleGattMirror::CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
  CharacteristicCallbacks(BleGattMirror* instance, MirroredCharacteristic* mirror)
    : bgmInstance(instance), mirror(mirror)
  {
  }

  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    if(bgmInstance) {
      bgmInstance->handleRead(mirror);
    }
  }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    if(bgmInstance) {
      bgmInstance->handleWrite(mirror);
    }
  }

  void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
    if(bgmInstance) {
      bgmInstance->handleSubscribe(mirror, subValue);
    }
  }

private:
  BleGattMirror* bgmInstance = nullptr;
  MirroredCharacteristic* mirror = nullptr;
};

BleGattMirror::BleGattMirror(BleClientBackend* backend)
{
    this->backend = backend;
    if(backend)
    {
        backend->setObserver(this);
        writeBridge = new BleWriteBridge(*backend);
    }
}

BleGattMirror::BleGattMirror(BleClientBackend& backend)
  : BleGattMirror(&backend)
{
}

BleGattMirror::~BleGattMirror()
{
    clearGattMirror();
    delete writeBridge;
    writeBridge = nullptr;
    delete serverCallbacks;
    serverCallbacks = nullptr;
}

BleGattMirrorState BleGattMirror::getState() const
{
  return state;
}

bool BleGattMirror::isConnected() const
{
  return centralConnected;
}

void BleGattMirror::setState(BleGattMirrorState newState)
{
  if(state == newState) {
    return;
  }

  BleGattMirrorState oldState = state;
  state = newState;

  if(newState == BleGattMirrorState::Error ||
     newState == BleGattMirrorState::Advertising ||
     newState == BleGattMirrorState::CentralConnected)
  {
    pendingPresentationFrom = oldState;
    pendingPresentationTo = newState;
    hasPendingPresentationState = true;
  }
}

void BleGattMirror::init(void) {
  LOG("BleGattMirror: Initializing the ServerSide");
  // Initialize NimBLE
  if(!NimBLEDevice::isInitialized()) {
    NimBLEDevice::init(THISDEVICENAME);  // Give the device a Shortname
  }
  // Set MTU to be negotiated during request from client peer
  if( NimBLEDevice::setMTU(ConnectionMTU) ) { 
    LOG("BleGattMirror: Preferred connection MTU succesfully set to: %d", ConnectionMTU);
  }
  // Setup Security
  NimBLEDevice::setSecurityAuth(false, true, true);  // NO BONDING at ESP32 side
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  // Optional: set the transmit power, default is 3db
  NimBLEDevice::setPower(9); // +9db
  // Set the ESP-board hardware static address to PUBLIC
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);

  pServer = NimBLEDevice::createServer();
  // Setup callbacks for onConnect, onDisconnect a.o.
  delete serverCallbacks;
  serverCallbacks = new ServerCallbacks(this);
  pServer->setCallbacks(serverCallbacks, false);
  // Set server auto-restart advertise on
  pServer->advertiseOnDisconnect(true); 
}

void BleGattMirror::startAdvertising(void) {
  if(pAdvertising && pAdvertising->start()) {
    setState(BleGattMirrorState::Advertising);
    LOG("BleGattMirror: Server is advertising!");
  } else {
    setState(BleGattMirrorState::Error);
    LOG("BleGattMirror: Server failed to start advertising!");
  }
}

void BleGattMirror::setupAdvertising(void) {
  LOG("BleGattMirror: Setup Advertising");
  // Prepare for advertising
  pAdvertising = NimBLEDevice::getAdvertising(); 

  addAdvertisedServices();

  if( pAdvertising->setAppearance(gapAppearance) )
    LOG("BleGattMirror: Setting Appearance in Advertised data to [%d]", gapAppearance);

  if( pAdvertising->setName(ADVERTISED_SHORT_NAME) )
    LOG("BleGattMirror: Set DeviceName in Advertised data to [%s]", ADVERTISED_SHORT_NAME);

/*
  // Add the transmission power level to the advertisement packet.
  if( pAdvertising->addTxPower() ) 
    LOG("BleGattMirror: Setting TxPower in Advertised data");
*/

  pAdvertising->enableScanResponse(true); // Respond to active scans
  pAdvertising->setMinInterval(AdvertiseMinInterval); 
  pAdvertising->setMaxInterval(AdvertiseMaxInterval); 
}

bool BleGattMirror::start()
{
  setState(BleGattMirrorState::Initializing);

  if(backend == nullptr || !backend->isReady()) {
    LOG("BleGattMirror: backend is not ready for Gatt building");
    setState(BleGattMirrorState::Error);
    return false;
  }

  init();

  if(writeBridge == nullptr || !writeBridge->begin()) {
    LOG("BleGattMirror: failed to start write bridge");
    setState(BleGattMirrorState::Error);
    return false;
  }

  if(backend && !backend->releaseConnectionForGattRegistration()) {
    LOG("BleGattMirror: Backend connection still active, cannot start local GATT server");
    setState(BleGattMirrorState::Error);
    return false;
  }
  setState(BleGattMirrorState::BackendReleasedForGatt);

  if(!buildGattMirror()) {
    LOG("BleGattMirror: Failed to build GATT mirror");
    setState(BleGattMirrorState::Error);
    return false;
  }

  // NimBLE may report the client disconnected before its internal GAP
  // connection state has been fully cleared. Starting the local GATT server
  // too early causes ble_gatts_add_svcs() to return BLE_HS_EBUSY and assert.
  LOG("BleGattMirror: Waiting for NimBLE disconnect cleanup");
  delay(250);

  if(pServer && !pServer->start()) {
    LOG("BleGattMirror: Failed to start GATT server");
    setState(BleGattMirrorState::Error);
    return false;
  }
  setState(BleGattMirrorState::GattRegistered);
  LOG("BleGattMirror: GATT database registered and server started");

  // Reconnect Client Backend a.s.a.p.
  setState(BleGattMirrorState::BackendReconnecting);
  if(!backend->reconnectUsingCachedTrainer()) {
    LOG("BleClientBackend: Quick Reconnect failed");
    setState(BleGattMirrorState::Error);
    return false;
  }
  LOG("BleGattMirror: Backend reconnected");

  // Complete the controlled startup after reconnect
  handleBackendReadyState(false);

  setupAdvertising();
  // Start advertising BleClientBackend advertised services
  startAdvertising();
  // Establish connection with zwift
  return true;
}

void BleGattMirror::update()
{
  processPendingBackendOperations();
  processPendingPresentationState();
}

void BleGattMirror::onBackendReady()
{
  if(state == BleGattMirrorState::Idle) {
    LOG("BleGattMirror: Handle backend ready!");
    backendHasBeenReady = true;
    return;
  }

  const bool restoreSubscriptions = backendHasBeenReady && \
                                    state == BleGattMirrorState::BackendReconnecting;

  if(restoreSubscriptions) {
    LOG("BleGattMirror: Backend reconnected");
  } else {
    LOG("BleGattMirror: Handle backend ready!");
  }

  handleBackendReadyState(restoreSubscriptions);
}

void BleGattMirror::handleBackendReadyState(bool restoreSubscriptions)
{
  backendHasBeenReady = true;
  setState(BleGattMirrorState::BackendReady);

  refreshGapValues();
  refreshReadableValues();

  if(restoreSubscriptions) {
    restoreClientSubscriptions();
  }
}

void BleGattMirror::onBackendDisconnected()
{
  LOG("BleGattMirror: Backend disconnected");

  if(backendHasBeenReady) {
    setState(BleGattMirrorState::BackendReconnecting);
  }

  // Zwift keeps its local subscriptions during a trainer-side reconnect. The
  // backend link lost its CCCD state, so mark backend subscriptions as gone
  // while preserving the centralSubscribed registration.
  for(auto& mirror : mirroredCharacteristics) {
    mirror.backendSubscribed = false;
    mirror.pendingSubscriptionUpdate = false;
  }
}

void BleGattMirror::onNotification(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic,
    const uint8_t* data,
    size_t length)
{
  MirroredCharacteristic* mirror = findMirror(service, characteristic);

  if(mirror == nullptr || mirror->local == nullptr) {
    LOG("BleGattMirror: notification target not mirrored");
    return;
  }

  mirror->local->setValue(data, length);

  if(!mirror->centralSubscribed) {
    return;
  }

  if(mirror->centralWantsNotify && (mirror->properties & NIMBLE_PROPERTY::NOTIFY)) {
    mirror->local->notify();
  } else if(mirror->centralWantsIndicate && (mirror->properties & NIMBLE_PROPERTY::INDICATE)) {
    mirror->local->indicate();
  }
}

void BleGattMirror::serverConnectionCallbacksOnConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo)
{
    centralConnected = true;
    setState(BleGattMirrorState::CentralConnected);
    if(pAdvertising) {
      pAdvertising->stop();
    }
    const uint16_t server_ConnectionHandle = connInfo.getConnHandle();
    OPS* operations = OPS::getInstance();
    UTILS* utils = UTILS::getInstance();
    NimBLEAddress remoteAddress = connInfo.getIdAddress();

#ifdef DEBUG
    // Get some connection parameters
    const uint16_t server_MTU = connInfo.getMTU();
    const uint16_t server_ConnInterval = connInfo.getConnInterval();
    const uint16_t server_ConnTimeout = connInfo.getConnTimeout();
    const uint16_t server_ConnLatency = connInfo.getConnLatency();
    LOG("BleGattMirror: Server connects to Central (Laptop/Phone) MAC Address: [%s] Conn Handle: [%d]",
            utils->toString(remoteAddress).c_str(), server_ConnectionHandle);
    LOG("BleGattMirror: Present Connection Parameters: Interval: [%d] Latency: [%d] Timeout: [%d]  (MTU: %d)",\
                            server_ConnInterval, server_ConnLatency, server_ConnTimeout, server_MTU);
#endif

    if(remoteAddress != operations->Laptop.PeerAddress) {
      if(operations->enableUnknownDev) {
        operations->Laptop.PeerAddress = remoteAddress;
        LOG("BleGattMirror: Unknown Mac Address registered for Central (Laptop)");
        utils->setMacAddressNVS("Laptop", remoteAddress);
      } else {
        LOG("BleGattMirror: >>> ERROR Forced Server Disconnect: Unknown Laptop Mac Address!");
        pServer->advertiseOnDisconnect(false);
        forcedCentralDisconnect = true;
        setState(BleGattMirrorState::Error);
        pServer->disconnect(server_ConnectionHandle);
        return;
      }
    }

    LOG("BleGattMirror: Central (Zwift) has to Subscribe to Characteristics and start..");
}

void BleGattMirror::serverConnectionCallbacksOnDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
  LOG("BleGattMirror: Server disconnected from Central");
  setState(BleGattMirrorState::Disconnected);
  centralConnected = false;

  // Put Simcline in neutral: 0% flat road position
  OPS* operations = OPS::getInstance();
  operations->levelGrade();

  if(forcedCentralDisconnect) {
    forcedCentralDisconnect = false;
    setState(BleGattMirrorState::Error);
    LOG("BleGattMirror: >>> Advertising stopped, configure Laptop Mac Address");
    return;
  }

  // Because of Auto-restart advertising
  setState(BleGattMirrorState::Advertising);
  for(auto& mirror : mirroredCharacteristics) {
    mirror.centralSubscribed = false;
    mirror.centralWantsNotify = false;
    mirror.centralWantsIndicate = false;
    if(mirror.backendSubscribed) {
      mirror.pendingSubscriptionUpdate = true;
    }
  }
}

bool BleGattMirror::buildGattMirror()
{
  if(getState() != BleGattMirrorState::BackendReleasedForGatt) {
    return false;
  }

  setState(BleGattMirrorState::BuildingGattMirror);
  LOG("BleGattMirror: Build Gatt mirror");

  clearGattMirror();

  std::vector<NimBLEUUID> services = backend->getServices();

  for(auto& serviceUUID : services) {

    if(serviceUUID.equals(UUID16_SVC_GENERIC_ACCESS) ||
       serviceUUID.equals(UUID16_SVC_GENERIC_ATTRIBUTE)) {
#ifdef DEBUG_GATT_CREATE
       LOG("BleGattMirror: skip NimBLE-owned service %s", serviceUUID.toString().c_str());
#endif
      continue;
    }

    NimBLEService* localService = pServer->createService(serviceUUID);
    if(localService == nullptr) {
      LOG("BleGattMirror: failed to create service %s", serviceUUID.toString().c_str());
      continue;
    } 

#ifdef DEBUG_GATT_CREATE
    LOG("BleGattMirror: created service %s", serviceUUID.toString().c_str());
#endif
    std::vector<GattCharacteristic> chars = backend->getCharacteristics(serviceUUID);

    for(auto& source : chars) {
      uint32_t properties = source.properties;
      NimBLECharacteristic* localChar =
        localService->createCharacteristic(source.characteristic, properties);

      if(localChar == nullptr) {
        LOG("BleGattMirror: failed to create characteristic %s", source.characteristic.toString().c_str());
        continue;
      } 

#ifdef DEBUG_GATT_CREATE
      LOG("BleGattMirror: created characteristic %s", source.characteristic.toString().c_str());
#endif
      mirroredCharacteristics.push_back(MirroredCharacteristic());
      MirroredCharacteristic* mirror = &mirroredCharacteristics.back();
      mirror->service = source.service;
      mirror->characteristic = source.characteristic;
      mirror->local = localChar;
      mirror->properties = properties;

      CharacteristicCallbacks* callbacks = new CharacteristicCallbacks(this, mirror);
      characteristicCallbacks.push_back(callbacks);
      localChar->setCallbacks(callbacks);

      mirrorLookup[makeKey(source.service, source.characteristic)] = mirror;
    }

    // localService->start(); Has been deprecated and will be removed in future versions of NimBLE
    // NimBLEServer::start() registers all services in one pass before advertising.
  }

  gattMirrorBuilt = !mirroredCharacteristics.empty();
#ifdef DEBUG_GATT_CREATE
  LOG("BleGattMirror: Mirrored %u characteristics", static_cast<unsigned>(mirroredCharacteristics.size()));
#endif
  return gattMirrorBuilt;
}

void BleGattMirror::clearGattMirror()
{
  mirrorLookup.clear();
  for(auto callbacks : characteristicCallbacks) {
    delete callbacks;
  }
  characteristicCallbacks.clear();
  mirroredCharacteristics.clear();
  gattMirrorBuilt = false;
}

bool BleGattMirror::addAdvertisedServices()
{
  if(backend == nullptr || pAdvertising == nullptr) {
    return false;
  }

  std::vector<NimBLEUUID> advertisedServices = backend->getAdvertisedServices();

  NimBLEUUID firstUuid128;
  bool hasUuid128 = false;

  for(auto& serviceUUID : advertisedServices) {
    if(serviceUUID.equals(UUID16_SVC_GENERIC_ACCESS) ||
       serviceUUID.equals(UUID16_SVC_GENERIC_ATTRIBUTE)) {
      continue;
    }

    if(serviceUUID.bitSize() == 128) {
      if(!hasUuid128) {
        firstUuid128 = serviceUUID;
        hasUuid128 = true;
      }
      continue;
    }

    LOG("BleGattMirror: Add service to advertising data %s", serviceUUID.toString().c_str());
    pAdvertising->addServiceUUID(serviceUUID);
  }

  // Primary advertising uses a short name. Scan response carries the trainer
  // GAP name and, when present, only the first advertised 128-bit service UUID.
  NimBLEAdvertisementData scanResp;
  /* Changed to avoid misunderstandings when Zwift is pairing...
  std::string scanName = gapDeviceName; 
  */
  std::string scanName = ADVERTISED_SHORT_NAME;
  bool scanNameIsComplete = true;

  if(hasUuid128 && scanName.length() > 11) {
    scanName = scanName.substr(0, 11);
    scanNameIsComplete = false;
  }

  if(!scanName.empty() && scanResp.setName(scanName, scanNameIsComplete)) {
    LOG("BleGattMirror: Set DeviceName in Scan Response to [%s]", scanName.c_str());
  } else if(!scanName.empty()) {
    LOG("BleGattMirror: FAILED Set DeviceName in Scan Response to [%s]", scanName.c_str());
  }

  if(hasUuid128) {
    if(scanResp.addServiceUUID(firstUuid128)) {
      LOG("BleGattMirror: Set Service in Scan Response %s", firstUuid128.toString().c_str());
    } else {
      LOG("BleGattMirror: FAILED Set Service in Scan Response %s", firstUuid128.toString().c_str());
    }
  }

  if(!scanResp.getPayload().empty()) {
    if(pAdvertising->setScanResponseData(scanResp)) {
      LOG("BleGattMirror: Set Scan Response Data");
    } else {
      LOG("BleGattMirror: FAILED Set Scan Response Data");
    }
  }

  return true;
}

void BleGattMirror::processPendingBackendOperations()
{
  if(backend == nullptr || !backend->isReady()) {
    return;
  }

  for(auto& mirror : mirroredCharacteristics) {
    if(mirror.pendingSubscriptionUpdate) {
      const bool shouldSubscribe = mirror.centralSubscribed;

      if(shouldSubscribe && !mirror.backendSubscribed) {
        if(backend->subscribeCharacteristic(mirror.service, mirror.characteristic)) {
          mirror.backendSubscribed = true;
          mirror.pendingSubscriptionUpdate = false;
        }
      } else if(!shouldSubscribe && mirror.backendSubscribed) {
        if(backend->unsubscribeCharacteristic(mirror.service, mirror.characteristic)) {
          mirror.backendSubscribed = false;
          mirror.pendingSubscriptionUpdate = false;
        }
      } else {
        mirror.pendingSubscriptionUpdate = false;
      }
    }
  }
}

void BleGattMirror::processPendingPresentationState()
{
  if(!hasPendingPresentationState) {
    return;
  }

  const BleGattMirrorState from = pendingPresentationFrom;
  const BleGattMirrorState to = pendingPresentationTo;
  hasPendingPresentationState = false;

  Presentation* presentation = Presentation::getInstance();

  if(to == BleGattMirrorState::Error) {
    presentation->ShowMessageWindow(THISDEVICENAME, "State", "Error!", 0);
  } else if((from == BleGattMirrorState::Advertising || 
             from == BleGattMirrorState::BackendReady) &&
             to == BleGattMirrorState::CentralConnected) {
    presentation->ShowMessageWindow(THISDEVICENAME, "Laptop", "Connected!", 0);
  } else if((from == BleGattMirrorState::BackendReady ||
             from == BleGattMirrorState::Disconnected) &&
             to == BleGattMirrorState::Advertising) {
    presentation->ShowMessageWindow(THISDEVICENAME, "Laptop", "Advertise!", 0);
  }
}

void BleGattMirror::restoreClientSubscriptions()
{
  if(backend == nullptr || !backend->isReady()) {
    return;
  }

  for(auto& mirror : mirroredCharacteristics) {
    if(!mirror.centralSubscribed) {
      continue;
    }

    if(!(mirror.properties & (NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE))) {
      continue;
    }

    mirror.backendSubscribed = false;
    mirror.pendingSubscriptionUpdate = true;
    LOG("BleGattMirror: restore backend subscription for %s",
         mirror.characteristic.toString().c_str());
  }
}

void BleGattMirror::refreshGapValues()
{
  if(backend == nullptr || !backend->isReady()) {
    return;
  }

  // 0x1800 is owned by NimBLE, so it cannot be part of the dynamic mirror.
  // These setters keep NimBLE's built-in GAP service aligned with the trainer
  // for the two values that NimBLE allows us to update safely at runtime.
  std::string value;
  if(backend->readCharacteristic(UUID16_SVC_GENERIC_ACCESS, UUID16_CHR_DEVICE_NAME, value) && !value.empty()) {
    gapDeviceName = value;
    if(NimBLEDevice::setDeviceName(gapDeviceName)) {
#ifdef DEBUG_SET_GAP_VALUES
      LOG("BleGattMirror: Set GAP DeviceName to [%s]", gapDeviceName.c_str());
#endif
    }
  }

  value.clear();
  if(backend->readCharacteristic(UUID16_SVC_GENERIC_ACCESS, UUID16_CHR_APPEARANCE, value) &&
     value.length() == sizeof(uint16_t)) {
    gapAppearance = static_cast<uint8_t>(value[0]) | (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);

    if(ble_svc_gap_device_appearance_set(gapAppearance) == 0) {
#ifdef DEBUG_SET_GAP_VALUES
      LOG("BleGattMirror: Set GAP Appearance to [%d]", gapAppearance);
#endif
    }
  }
}

void BleGattMirror::refreshReadableValues()
{
  if(backend == nullptr || !backend->isReady()) {
    return;
  }

  for(auto& mirror : mirroredCharacteristics) {
    if(mirror.local == nullptr || !(mirror.properties & NIMBLE_PROPERTY::READ)) {
      continue;
    }

    std::string value;
    if(backend->readCharacteristic(mirror.service, mirror.characteristic, value)) {
      mirror.local->setValue(reinterpret_cast<const uint8_t*>(value.data()), value.length());
    }
  }
}

void BleGattMirror::handleRead(MirroredCharacteristic* mirror)
{
  if(backend == nullptr || mirror == nullptr || mirror->local == nullptr) {
    return;
  }

  LOG("BleGattMirror: handle read of %s", mirror->characteristic.toString().c_str());
  // Do not perform a remote BLE read here. NimBLE is waiting for this local
  // callback to return so it can send the ATT read response.
}

void BleGattMirror::handleWrite(MirroredCharacteristic* mirror)
{
  if(backend == nullptr || mirror == nullptr || mirror->local == nullptr) {
    return;
  }

  std::string value = mirror->local->getValue();
  BleWriteKind kind = BleWriteKind::WithResponse;

  if((mirror->properties & NIMBLE_PROPERTY::WRITE_NR) &&
     !(mirror->properties & NIMBLE_PROPERTY::WRITE)) {
    kind = BleWriteKind::WithoutResponse;
  }

  if(writeBridge == nullptr ||
     !writeBridge->enqueueWrite(
       mirror->service,
       mirror->characteristic,
       reinterpret_cast<const uint8_t*>(value.data()),
       value.length(),
       kind)) {
    LOG("BleGattMirror: failed to enqueue write");
  }
}

void BleGattMirror::handleSubscribe(MirroredCharacteristic* mirror, uint16_t subValue)
{
  if(backend == nullptr || mirror == nullptr) {
    return;
  }
  
  const bool wantsNotifications = (subValue & 0x0001) != 0;
  const bool wantsIndications = (subValue & 0x0002) != 0;
  const bool subscribed = wantsNotifications || wantsIndications;
#ifdef DEBUG
  if(subscribed)
    LOG("BleGattMirror: handle Subscribe of %s", mirror->characteristic.toString().c_str());
  else
    LOG("BleGattMirror: handle Unsubscribe of %s", mirror->characteristic.toString().c_str());
#endif
  mirror->centralSubscribed = subscribed;
  mirror->centralWantsNotify = wantsNotifications;
  mirror->centralWantsIndicate = wantsIndications;
  mirror->pendingSubscriptionUpdate = true;
}

BleGattMirror::MirroredCharacteristic* BleGattMirror::findMirror(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic)
{
  auto it = mirrorLookup.find(makeKey(service, characteristic));
  if(it == mirrorLookup.end()) {
    return nullptr;
  }

  return it->second;
}

std::string BleGattMirror::canonicalUuidString(const NimBLEUUID& uuid)
{
  NimBLEUUID tmp(uuid);
  return tmp.to16().toString();
}

std::string BleGattMirror::makeKey(const NimBLEUUID& service, const NimBLEUUID& characteristic)
{
  return canonicalUuidString(service) + "|" + canonicalUuidString(characteristic);
}
