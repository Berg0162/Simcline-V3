#include "BleClientBackend.h"
#include "Operations.h"
#include "Presentation.h"
#include "config/configNimBLE.h"

// NimBLE Client ---------------------------------------------------------------------------
#define UUID16_SVC_CYCLING_POWER    NimBLEUUID((uint16_t)0x1818)

#define DEBUG
#include "config/configDebug.h"
#ifdef DEBUG
//#define DEBUG_DISCOVER
#endif

BleClientBackend* BleClientBackend::instance = nullptr;

class TrainerAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
public:
    const NimBLEAdvertisedDevice* foundDevice = nullptr;

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if( advertisedDevice->isAdvertisingService(UUID16_SVC_CYCLING_POWER) )
        {
            OPS* operations = OPS::getInstance();
            UTILS* utils = UTILS::getInstance();
            NimBLEAddress remoteAddress = advertisedDevice->getAddress();
            std::string deviceName = "Unknown";
            if(advertisedDevice->haveName()) { 
                deviceName = advertisedDevice->getName(); 
            }
            LOG("BleClientBackend: Found Trainer with Mac: [%s] Name: %s",
                utils->toString(remoteAddress).c_str(), deviceName.c_str());

            if(remoteAddress != operations->Trainer.PeerAddress) {
                if(operations->enableUnknownDev) {
                    // One-time pairing escape: accept this trainer and persist it for later runs.
                    operations->Trainer.PeerAddress = remoteAddress;
                    LOG("BleClientBackend: Unknown Mac Address registered for Peripheral (Trainer)");
                    utils->setMacAddressNVS("Trainer", remoteAddress);
                } else {
                    LOG("BleClientBackend: >>> NO MATCH! Keep Scanning for Trainer Mac Address: [%s]",
                        utils->toString(operations->Trainer.PeerAddress).c_str());
                    return;
                }
            }
            
            operations->Trainer.PeerName = deviceName;
            foundDevice = advertisedDevice;
            NimBLEDevice::getScan()->stop();
        }
    }
} ScanCallbacks;

// Constructor
BleClientBackend::BleClientBackend()
{
    this->observer = nullptr;
    client = nullptr;
    targetDevice = nullptr;
    instance = this;
}

bool BleClientBackend::start()
{
    LOG("BleClientBackend: Starting NimBLE Client");

    if (state != BleClientBackendState::Idle) {
        setState(BleClientBackendState::Error);
        return false;
    }

    // Initialize NimBLE and set the device name

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init(THISDEVICENAME);
    }

    if(!establishConnection()) {
        return false;        
    }

    return true;
}

void BleClientBackend::update()
{
    if (state == BleClientBackendState::GattRegistrationPause) {
        return;
    }

    processPendingPresentationState();

    if (state == BleClientBackendState::Ready) {
        if (client == nullptr || !client->isConnected()) {
            handleDisconnect();
        }
        return;
    }

    if (state == BleClientBackendState::Disconnected) {
        LOG("BleClientBackend: Entering reconnect wait state");
        setState(BleClientBackendState::ReconnectingWait);
        return;
    }

    if (state == BleClientBackendState::ReconnectingWait) {
        if (millis() >= reconnectAttemptDueMs) {
            establishConnection();
        }
        return;
    }
}

void BleClientBackend::setObserver(BleBackendObserver* observer)
{
    this->observer = observer;
}

    // -------- BleClientBackend state -------------

BleClientBackendState BleClientBackend::getState() const
{
    return state;
}

void BleClientBackend::setState(BleClientBackendState newState)
{
    if(state == newState) {
        return;
    }

    BleClientBackendState oldState = state;
    state = newState;

    if(newState == BleClientBackendState::Scanning ||
       newState == BleClientBackendState::ScanTimeout ||
       newState == BleClientBackendState::ReconnectingWait ||
       newState == BleClientBackendState::Ready ||
       newState == BleClientBackendState::Error)
    {
        pendingPresentationFrom = oldState;
        pendingPresentationTo = newState;
        hasPendingPresentationState = true;
    }
}

void BleClientBackend::processPendingPresentationState()
{
    if(!hasPendingPresentationState) {
        return;
    }

    const BleClientBackendState from = pendingPresentationFrom;
    const BleClientBackendState to = pendingPresentationTo;
    hasPendingPresentationState = false;

    Presentation* presentation = Presentation::getInstance();

    if(to == BleClientBackendState::Error) {
        presentation->ShowMessageWindow(THISDEVICENAME, "State", "Error!", 0);
    } else if(to == BleClientBackendState::Scanning) {
        presentation->ShowMessageWindow(THISDEVICENAME, "Trainer", "Scanning!", 500);
    } else if(from == BleClientBackendState::Scanning &&
              to == BleClientBackendState::ScanTimeout) {
        presentation->ShowMessageWindow(THISDEVICENAME, "Trainer", "Timeout!", 0);
    } else if(to == BleClientBackendState::ReconnectingWait) {
        presentation->ShowMessageWindow(THISDEVICENAME, "Trainer", "Reconnect?", 0);
    } else if((from == BleClientBackendState::Connecting ||
               from == BleClientBackendState::Discovering) &&
               to == BleClientBackendState::Ready) {
        presentation->ShowMessageWindow(THISDEVICENAME, "Trainer", "Connected!", 0);
    }
}

bool BleClientBackend::isConnected() const
{
    return (client != nullptr && client->isConnected());
}

void BleClientBackend::handleDisconnect(void)
{
    if (state == BleClientBackendState::Disconnected ||
        state == BleClientBackendState::ReconnectingWait)
    {
        return;
    }

    LOG("BleClientBackend: BLE client disconnected");

    setState(BleClientBackendState::Disconnected);
    scheduleReconnectRetry();
    
    if (observer) {
        observer->onBackendDisconnected();
    }
}

bool BleClientBackend::establishConnection()
{
    LOG("BleClientBackend: Establishing connection");

    clearDiscoveryCache();
    discoveryComplete = false;
    hadConnection = false;

    if (!BleScanServer() || !BleServerConnect()) {
        if(state == BleClientBackendState::Disconnected) {
            setState(BleClientBackendState::ReconnectingWait);
            scheduleReconnectRetry();
        }
        return false;
    }

    setState(BleClientBackendState::Discovering);

    if (!discoverServices()) {
        LOG("BleClientBackend: Service discovery failed");
        setState(BleClientBackendState::ReconnectingWait);
        scheduleReconnectRetry();
        return false;
    }

    if (!discoverCharacteristics()) {
        LOG("BleClientBackend: Characteristic discovery failed");
        setState(BleClientBackendState::ReconnectingWait);
        scheduleReconnectRetry();
        return false;
    }

    LOG("BleClientBackend: Connected and ready");
    discoveryComplete = true;
    hadConnection = true;
    reconnectAttemptCount = 0;
    setState(BleClientBackendState::Ready);

    if (observer) {
        observer->onBackendReady();
    }

    return true;
}

bool BleClientBackend::isDiscoveryComplete() const
{
    return discoveryComplete;
}

bool BleClientBackend::isReady() const
{
    return (state == BleClientBackendState::Ready) &&
           (client != nullptr) &&
           client->isConnected() &&
           discoveryComplete;
}

bool BleClientBackend::releaseConnectionForGattRegistration()
{
    if(client == nullptr || !client->isConnected()) {
        return true;
    }

    LOG("BleClientBackend: Temporarily disconnecting trainer for local GATT registration");
    client->disconnect();

    // This is an intentional short trainer-side release while the local GATT
    // database is registered. Keep update() from treating it as a trainer loss.
    setState(BleClientBackendState::GattRegistrationPause);
    if (observer) {
        observer->onBackendDisconnected();
    }

    // Wait for effectuated client link disconnection
    const unsigned long disconnectTimeoutMs = millis() + 3000;
    while(client->isConnected() && millis() < disconnectTimeoutMs) {
        delay(10);
    }

    if(client->isConnected()) {
        LOG("BleClientBackend: Trainer disconnect timed out");
        setState(BleClientBackendState::Error);
        return false;
    }

    // Measured as necessary: after the link drops, NimBLE still needs a short
    // settling window before the local GATT database can be registered.
    delay(100);

    return true;
}

bool BleClientBackend::reconnectUsingCachedTrainer()
{
    // Startup-only fast path: the trainer was just discovered, its services and
    // characteristics are still cached, and the link was released only so the
    // local GATT mirror could be registered. Do not scan or rediscover here.
    if(targetDevice == nullptr || client == nullptr) {
        return false;
    }

    if(client->isConnected()) {
        return true;
    }

    return BleServerConnect();
}

   // ----- Basic operations -----

bool BleClientBackend::readCharacteristic(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic,
    std::string& value)
{
    GattCharacteristic* gc =
        findCharacteristic(service, characteristic);

    if(gc == nullptr) {
        LOG("BleClientBackend: readCharacteristic: not found");
        return false;
    }

    if(gc->remote == nullptr)
        return false;

    if(!(gc->properties & NIMBLE_PROPERTY::READ))
    {
        LOG("BleClientBackend: readCharacteristic: not readable");
        return false;
    }

    try
    {
        value = gc->remote->readValue();
        return true;
    }
    catch(...)
    {
        LOG("readCharacteristic: exception");
        return false;
    }
}


bool BleClientBackend::subscribeCharacteristic(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic)
{
    GattCharacteristic* gc =
        findCharacteristic(service, characteristic);

    if(gc == nullptr) {
        LOG("BleClientBackend: subscribeCharacteristic -> gc NOT Intantiated!");
        return false;
    }

    bool result = false;

    if(gc->properties & NIMBLE_PROPERTY::NOTIFY)
    {
        result = gc->remote->subscribe(true, notifyCallback);
    }
    else if(gc->properties & NIMBLE_PROPERTY::INDICATE)
    {
        result = gc->remote->subscribe(false, notifyCallback, true);
    }

    if(result) {
        gc->isSubscribed = true;

        if(gc->remote->canRead())   // Trainer might only notify when the value changes
            gc->remote->readValue();

        NimBLEUUID tmp(gc->characteristic);
        const std::string shortUUID = tmp.to16().toString();
        LOG("BleClientBackend: Subscribe to Characteristic %s", shortUUID.c_str());
    } else {
        LOG("BleClientBackend: subscribeCharacteristic Failed to %s", gc->characteristic.toString().c_str());
    }
    return result;
}

bool BleClientBackend::unsubscribeCharacteristic(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic)
{
    GattCharacteristic* gc = findCharacteristic(service, characteristic);

    if(gc == nullptr)
        return false;
    
    bool result = gc->remote->unsubscribe();

    if(result) {
        gc->isSubscribed = false;
        NimBLEUUID tmp(gc->characteristic);
        const std::string shortUUID = tmp.to16().toString();
        LOG("BleClientBackend: Unsubscribe to Characteristic %s", shortUUID.c_str());
    }

    return result;
}

bool BleClientBackend::writeCharacteristic(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic,
    const uint8_t* data,
    size_t length,
    bool withResponse)
{
    GattCharacteristic* gc =
        findCharacteristic(service, characteristic);

    if(gc == nullptr) {
        LOG("BleClientBackend: writeCharacteristic: not found");
        return false;
    }

    if(gc->remote == nullptr)
        return false;

    if( !(gc->properties & (NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)) )
    {
        LOG("BleClientBackend: writeCharacteristic: not writable");
        return false;
    }

    if(withResponse && !(gc->properties & NIMBLE_PROPERTY::WRITE)) {
        LOG("BleClientBackend: writeCharacteristic: write-with-response not supported");
        return false;
    }

    if(!withResponse && !(gc->properties & NIMBLE_PROPERTY::WRITE_NR)) {
        LOG("BleClientBackend: writeCharacteristic: write-without-response not supported");
        return false;
    }

#ifdef DEBUG
        NimBLEUUID tmp(gc->characteristic);
        const std::string shortUUID = tmp.to16().toString();
        LOG("BleClientBackend: Write to Characteristic %s", shortUUID.c_str());
#endif

    try
    {
        bool success = gc->remote->writeValue(data, length, withResponse);
        return success;
    }
    catch(...)
    {
        LOG("BleClientBackend: writeCharacteristic: exception");
        return false;
    }
}


std::vector<NimBLEUUID> BleClientBackend::getServices() 
{
    return services;
}

std::vector<NimBLEUUID> BleClientBackend::getAdvertisedServices()
{
    return advertisedServices;
}

std::vector<GattCharacteristic> BleClientBackend::getCharacteristics(const NimBLEUUID& service)
{
    std::vector<GattCharacteristic> result;

    for(auto& gc : characteristics)
    {
        if(gc.service == service)
            result.push_back(gc);
    }

    return result;
}

bool BleClientBackend::getServiceForCharacteristic(
    const NimBLEUUID& characteristic,
    NimBLEUUID& serviceOut)
{
    auto it = characteristicByUuid.find(canonicalUuidString(characteristic)); 
    if (it == characteristicByUuid.end()) return false; 
    GattCharacteristic* gc = it->second; 
    if (gc == nullptr) return false; 
    serviceOut = gc->service; 
    return true;    
}

uint32_t BleClientBackend::getCharacteristicProperties(
        const NimBLEUUID& service,
        const NimBLEUUID& characteristic)
{
    GattCharacteristic* gc =
        findCharacteristic(service, characteristic);

    if(gc == nullptr)
        return 0;

    return gc->properties;
}

bool BleClientBackend::isSubscribed(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic)
{
    GattCharacteristic* gc =
        findCharacteristic(service, characteristic);

    if(gc == nullptr)
        return false;

    return gc->isSubscribed;
}

bool BleClientBackend::BleScanServer()
{
    // Check for a Fast Reconnect possibility first, before start scanning
    if ( (client == NimBLEDevice::getDisconnectedClient()) && (targetDevice != nullptr) ) {
        LOG("BleClientBackend: Trying fast reconnect");
        return true;
    }

    setState(BleClientBackendState::Scanning);
    LOG("BleClientBackend: Scanning for CPS trainer...");

    ScanCallbacks.foundDevice = nullptr;
    targetDevice = nullptr;

    NimBLEDevice::setPower(3); // 3dbm 
    NimBLEScan* scan = NimBLEDevice::getScan();

    scan->setScanCallbacks(&ScanCallbacks, false);

    scan->setActiveScan(true);
    scan->setInterval(ScanInterval);
    scan->setWindow(ScanWindow);

    // Start scanning for undetermined time span -> scan->start(0, false, false); 
    // isContinue = false -> clear previous scan results, restart = false 
    scan->start(ScanDuration, false, false); // Scan for indicated duration
    while(scan->isScanning()) { 
        delay(100);
    }

    if(ScanCallbacks.foundDevice == nullptr)
    {
        LOG("BleClientBackend: Scan timeout after %2d seconds!", ScanDuration/1000);
        setState(BleClientBackendState::ScanTimeout);
        return false;
    }

    targetDevice = ScanCallbacks.foundDevice;
    return true;
}

bool BleClientBackend::BleServerConnect() {
    LOG("BleClientBackend: Connecting Server...");
    setState(BleClientBackendState::Connecting);
    bool hasConnectPassed = false;

    if(client == nullptr) { // First time -> create new client and NimBLE service database!
      client = NimBLEDevice::createClient();
      client->setConnectTimeout(2000); // Set the timeout to wait for connection attempt to complete
      // Set Client's Preferred Connection Parameters
      client->setConnectionParams(ConnectionMinInterval, ConnectionMaxInterval, \
                                  ConnectionLatency, ConnectionTimeout, ScanInterval, ScanWindow);
      // First Time Connect to the Trainer (Server/Peripheral) 
      //  -> Delete attribute objects and Create NimBLE service database
      hasConnectPassed = client->connect(targetDevice, true, false, false); // NO CLIENT INITIATED MTU EXCHANGE 
    } else if(client == NimBLEDevice::getDisconnectedClient()) { // Allow for a streamlined reconnect
        // Reconnect to the disconnected Trainer (Server/Peripheral) -> Just refresh the NimBLE service database 
        hasConnectPassed = client->connect(targetDevice, false, false, false); // NO CLIENT INITIATED MTU EXCHANGE
      } 

    if(!hasConnectPassed) {
        LOG("BleClientBackend: Connecting Server Failed");
        // If connected: disconnect, delete client object and clear from list
        NimBLEDevice::deleteClient(client); 
        client = nullptr; // Clear to null
        targetDevice = nullptr; // Clear to null
        setState(BleClientBackendState::Disconnected);
        return false;
    }

    LOG("BleClientBackend: Server Connected");
    delay(200);
    setState(BleClientBackendState::Ready);
    return true;
}

void BleClientBackend::scheduleReconnectRetry()
{
    ++reconnectAttemptCount;

    const bool fastRetry = (reconnectAttemptCount <= RECONNECT_FAST_RETRY_LIMIT);

    unsigned long delayMs = fastRetry ? RECONNECT_DELAY_MS_FAST : RECONNECT_DELAY_MS_SLOW;

    reconnectAttemptDueMs = millis() + delayMs;

#ifdef DEBUG
    LOG("BleClientBackend: Reconnect attempt %d scheduled (%s) in %d ms", \ 
                                reconnectAttemptCount, (fastRetry ? "fast" : "slow"), delayMs);
#endif
}

void BleClientBackend::clearDiscoveryCache()
{
    services.clear();
    advertisedServices.clear();

    characteristicLookup.clear();
    characteristicByUuid.clear();
    remoteLookup.clear();

    characteristics.clear();

    discoveryComplete = false;
}

bool BleClientBackend::discoverServices() {

    LOG("BleClientBackend: Discovering services...");

    services.clear();
    advertisedServices.clear();
    discoveryComplete = false;

    auto remoteServices = client->getServices(true);

    if(remoteServices.empty())
    {
        LOG("BleClientBackend: Service discovery failed");
        return false;
    }

    for(auto service : remoteServices)
    {
        NimBLEUUID uuid = service->getUUID();

        services.push_back(uuid);
        if (targetDevice->isAdvertisingService(uuid)) {
            advertisedServices.push_back(uuid);  
        }
#ifdef DEBUG_DISCOVER
        LOG("Service: ");
        LOG(uuid.toString().c_str());
#endif
    }

    return true;
}

// Helper
std::string BleClientBackend::canonicalUuidString(const NimBLEUUID& uuid)
{
    NimBLEUUID tmp(uuid);
    return tmp.to16().toString(); // Keeps real 128-bit UUUID unchanged!!
}

std::string BleClientBackend::makeKey(const NimBLEUUID& service,
                                const NimBLEUUID& characteristic) 
{
    return canonicalUuidString(service) + "|" + canonicalUuidString(characteristic);
}

bool BleClientBackend::discoverCharacteristics()
{
    LOG("BleClientBackend: Discovering characteristics...");

    characteristicLookup.clear();
    characteristicByUuid.clear();
    characteristics.clear();
    remoteLookup.clear();

    for(auto serviceUUID : services)
    {
        NimBLERemoteService* service = client->getService(serviceUUID);

        if(service == nullptr)
            continue;
#ifdef DEBUG_DISCOVER        
        LOG("Service: %s",serviceUUID.toString().c_str());
#endif
        auto chars = service->getCharacteristics(true);

        for(auto chr : chars)
        {
            GattCharacteristic gc;

            gc.service = serviceUUID;
            gc.characteristic = chr->getUUID();
            gc.remote = chr;

            gc.properties = 0;

            if(chr->canRead())
                gc.properties |= NIMBLE_PROPERTY::READ;

            if(chr->canWrite())
                gc.properties |= NIMBLE_PROPERTY::WRITE;

            if(chr->canWriteNoResponse())
                gc.properties |= NIMBLE_PROPERTY::WRITE_NR;

            if(chr->canNotify())
                gc.properties |= NIMBLE_PROPERTY::NOTIFY;

            if(chr->canIndicate())
                gc.properties |= NIMBLE_PROPERTY::INDICATE;

            gc.isSubscribed = false;

            characteristics.push_back(gc);

            GattCharacteristic* ptr = &characteristics.back();
            std::string key = makeKey(gc.service, gc.characteristic);
            characteristicLookup[key] = ptr;
            std::string keyChar = canonicalUuidString(gc.characteristic); 
            characteristicByUuid[keyChar] = ptr;

            remoteLookup[gc.remote] = ptr;
#ifdef DEBUG_DISCOVER
            LOG("  Char: ");
            LOG(gc.characteristic.toString().c_str());
            if(gc.properties & NIMBLE_PROPERTY::NOTIFY) {
                LOG(" notify");
            }
            if(gc.properties & NIMBLE_PROPERTY::INDICATE) {
                LOG(" indicate");
            }
            LOG();
#endif
        }
    }
    discoveryComplete = true;
    return true;
}

bool BleClientBackend::hasCharacteristic(const NimBLEUUID& characteristic) const
{
    auto it = characteristicByUuid.find(canonicalUuidString(characteristic));

    return (it != characteristicByUuid.end() && it->second != nullptr);
}

GattCharacteristic* BleClientBackend::findCharacteristic(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic)
{
    std::string key = makeKey(service, characteristic);

    auto it = characteristicLookup.find(key);

    if(it == characteristicLookup.end())
        return nullptr;

    return it->second;
}

void BleClientBackend::notifyCallback(
    NimBLERemoteCharacteristic* chr,
    uint8_t* data,
    size_t length,
    bool isNotify)
{
    //LOG("notifyCallback is called!");

    if(instance == nullptr) {
        LOG("BleClientBackend: notifyCallback -> instance NOT Intantiated!");
        return;
    }

    auto it = instance->remoteLookup.find(chr);
    if(it == instance->remoteLookup.end())
        return;
    GattCharacteristic* gc = it->second;

    if(instance->observer) {
        instance->observer->onNotification(
            gc->service,
            gc->characteristic,
            data,
            length);
    }
}

