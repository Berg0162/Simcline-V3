#include "BleWriteObserverHub.h"

// Unsigned little endian base 128 (ULEB128) implementation, 
// which stores arbitrarily large unsigned integers in a variable length format. 
#include <uleb128.h>

#define UUID16_SVC_FITNESS_MACHINE NimBLEUUID((uint16_t)0x1826)
#define UUID16_FTMS_CONTROL_POINT NimBLEUUID((uint16_t)0x2AD9)
#define FTMS_OP_SET_INDOOR_BIKE_SIMULATION_PARAMETERS 0x11

#define UUID16_SVC_CYCLING_POWER NimBLEUUID((uint16_t)0x1818)
#define WAHOO_CYCLING_POWER_CONTROL_POINT_UUID NimBLEUUID("a026e005-0a7d-4ab3-97fa-f1500f9feb8b")
#define WAHOO_OP_SET_SIM_GRADE 0x46

#define TACX_FEC_PRIMARY_SERVICE_UUID NimBLEUUID("6e40fec1-b5a3-f393-e0a9-e50e24dcca9e")
#define TACX_FEC_TXD_CHARACTERISTIC_UUID NimBLEUUID("6e40fec3-b5a3-f393-e0a9-e50e24dcca9e")
#define TACX_FEC_PACKET_SIZE 13
#define TACX_FEC_HEADER_SIZE 4
#define TACX_FEC_PAGE_TRACK_RESISTANCE 0x33

#define ZWIFT_OLD_SERVICE_UUID NimBLEUUID("00000001-19ca-4651-86e5-fa29dcdd09d1")
#define ZWIFT_NEW_SERVICE_UUID NimBLEUUID((uint16_t)0xFC82)
#define ZWIFT_SYNCRX_CHARACTERISTIC_UUID NimBLEUUID("00000003-19ca-4651-86e5-fa29dcdd09d1")

#define DEBUG
#include "config/configDebug.h"

// Constructor
BleWriteObserverHub::BleWriteObserverHub() 
{
    // Create instance of the Operations class
    operations = OPS::getInstance();
}

// Destructor
BleWriteObserverHub::~BleWriteObserverHub() 
{}

void BleWriteObserverHub::observeWrite(
    const NimBLEUUID& service,
    const NimBLEUUID& characteristic,
    const uint8_t* data,
    size_t length,
    BleWriteKind kind)
{
    (void)kind;

    if(data == nullptr && length > 0) {
        return;
    }

#ifdef DEBUG
LOG("BleWriteObserverHub::observeWrite [%u] %s", static_cast<unsigned>(length), service.toString().c_str());

Serial.print(" data:");
for(size_t i = 0; i < length; ++i) {
    Serial.printf(" %02X", data[i]);
}
Serial.println();
#endif

    if(service.equals(UUID16_SVC_FITNESS_MACHINE) &&
       characteristic.equals(UUID16_FTMS_CONTROL_POINT)) {
        observeFTMSWriteRequest(data, length);
        return;
    }

    if(service.equals(UUID16_SVC_CYCLING_POWER) &&
       characteristic.equals(WAHOO_CYCLING_POWER_CONTROL_POINT_UUID)) {
        observeWahooControlPointWrite(data, length);
        return;
    }

    if(service.equals(TACX_FEC_PRIMARY_SERVICE_UUID) &&
       characteristic.equals(TACX_FEC_TXD_CHARACTERISTIC_UUID)) {
        observeTacxFecTxdWrite(data, length);
        return;
    }

    if(service.equals(ZWIFT_OLD_SERVICE_UUID) &&
       characteristic.equals(ZWIFT_SYNCRX_CHARACTERISTIC_UUID)) {
        observeZwiftOldSyncRxWrite(data, length);
        return;
    }

    if(service.equals(ZWIFT_NEW_SERVICE_UUID) &&
        characteristic.equals(ZWIFT_SYNCRX_CHARACTERISTIC_UUID)) {
            if(length > 0 && data[0] == 0x3F) { // Wahoo Kickr specific !! Test !!
                observeZwiftOldSyncRxWrite(data, length);
            } else {
                observeZwiftNewSyncRxWrite(data, length);
            }
            return;
    }
}

// Shortest version
void BleWriteObserverHub::observeFTMSWriteRequest(const uint8_t* data, size_t length)
{
    if(data == nullptr || operations == nullptr) {
        return;
    }

#ifdef DEBUG
LOG(" -> observeFTMSWriteRequest");
#endif

    // FTMS Set Indoor Bike Simulation Parameters:
    // opcode + wind speed(2) + grade(2) + crr(1) + cw(1)
    if(length != 7 || data[0] != FTMS_OP_SET_INDOOR_BIKE_SIMULATION_PARAMETERS) {
        LOG("BleWriteObserverHub: Undecoded len: %d opcode: 0x%02X", length, data[0]);
        return;
    }

    const int16_t ftmsGrade = static_cast<int16_t>(static_cast<uint16_t>(data[3]) | \
                                                  (static_cast<uint16_t>(data[4]) << 8));
    const float grade = ftmsGrade / 100.0f;

    operations->setNewGrade(grade);
}

void BleWriteObserverHub::observeWahooControlPointWrite(const uint8_t* data, size_t length)
{
    if(data == nullptr || operations == nullptr) {
        return;
    }

#ifdef DEBUG
LOG(" -> observeWahooControlPointWrite");
#endif

    // Wahoo proprietary CPS control point: opcode + parameters.
    // Opcode 0x46 is Set Simulation Grade, with a uint16 parameter mapped to
    // -100.0% .. +100.0%.
    if(length < 3 || data[0] != WAHOO_OP_SET_SIM_GRADE) {
        return;
    }

    const uint16_t rawGrade = static_cast<uint16_t>(data[1]) | (static_cast<uint16_t>(data[2]) << 8);
    const float grade = 100.0f * ((static_cast<float>(rawGrade) * 2.0f / 65535.0f) - 1.0f);

    operations->setNewGrade(grade);
}

void BleWriteObserverHub::observeTacxFecTxdWrite(const uint8_t* data, size_t length)
{
    if(data == nullptr || operations == nullptr) {
        return;
    }

#ifdef DEBUG
LOG(" -> observeTacxFecTxdWrite");
#endif

    // TACX FE-C transports a generic 13-byte ANT+ packet:
    // header(4) + body(8) + checksum(1). Body[0] is the FE-C data page.
    if(length != TACX_FEC_PACKET_SIZE) {
        return;
    }

    const uint8_t* body = data + TACX_FEC_HEADER_SIZE;
    if(body[0] != TACX_FEC_PAGE_TRACK_RESISTANCE) {
        return;
    }

    // Data Page 51 / 0x33 Track Resistance:
    // body[5..6] = grade with 20000 offset, units 0.01%.
    const uint16_t rawGrade =
        static_cast<uint16_t>(body[5]) |
        (static_cast<uint16_t>(body[6]) << 8);
    const float grade = (static_cast<float>(rawGrade) - 20000.0f) / 100.0f;

    operations->setNewGrade(grade);
}

// Handle very first generations that implemented ZVS with 128-bit UUID
void BleWriteObserverHub::observeZwiftOldSyncRxWrite(const uint8_t* data, size_t length)
{
    if(data == nullptr || operations == nullptr) {
        return;
    }

#ifdef DEBUG
LOG(" -> observeZwiftOldSyncRxWrite");
#endif

    if(length < 5) {
        return;
    }

    // Accept either Wahoo Kickr request (0x3F) or Zwift Hub One legacy request (0x04)
    if(data[0] != 0x3F && data[0] != 0x04) {
        return;
    }

    // Notice that Zwift implemented ZVS at first in the Zwift Hub One and redesigned it completely 
    // in later versions! This is a One-Off,
    // Zwift Hub One legacy road grade messages:
    // 8-bit  [04 22 02 10 xx]       scale 0.025%
    // 16-bit [04 22 03 10 xx xx]    scale 0.0025%
    if(data[0] == 0x04 && data[1] == 0x22 && data[3] == 0x10) {
        float grade = 0.0f;
        bool decoded = false;

        if(data[2] == 0x02 && length >= 5) {
            grade = static_cast<float>(data[4]) * 0.025f;
            decoded = true;
        } else if(data[2] == 0x03 && length >= 6) {
            const uint16_t value =
                static_cast<uint16_t>(data[4]) |
                (static_cast<uint16_t>(data[5]) << 8);
            grade = static_cast<float>(value) * 0.0025f;
            decoded = true;
        }

        if(decoded) {
            operations->setNewGrade(grade);
        }
        return;
    }

    // Restrict further decoding to ID (0x3F) -> Wahoo Kickr request
    if(data[0] != 0x3F) {
        return;
    }

    // Wahoo Kickr request with Road value at tag == 0x28
    size_t offset = 1;
    while(offset < length) {
        const uint8_t tag = data[offset++];
        if(offset >= length) {
            break;
        }

        uint64_t uval = 0;
        const size_t consumed = bfs::DecodeUleb128(data + offset, length - offset, &uval);
        // Invalid or truncated ULEB128 → abort safely
        if(consumed == 0 || offset + consumed > length) {
            break;
        }
        offset += consumed;

        // Wahoo Kickr road grade. Value is ZigZag-encoded and scaled in 0.01%.
        if(tag == 0x28) {
            int64_t zwiftGrade = static_cast<int64_t>(uval);
            zwiftGrade = (zwiftGrade >> 1) ^ -(zwiftGrade & 1);
            const float grade = static_cast<float>(zwiftGrade) / 100.0f;
            operations->setNewGrade(grade);
        }

        // Tag 0x38 carries gear ratio and is intentionally ignored here.
    }
}

void BleWriteObserverHub::observeZwiftNewSyncRxWrite(const uint8_t* data, size_t length)
{
    if(data == nullptr || operations == nullptr) {
        return;
    }

#ifdef DEBUG
LOG(" -> observeZwiftNewSyncRxWrite");
#endif

    if(length < 2 || data[0] != 0x04) {
        return;
    }

    const uint8_t messageType = data[1];
    size_t offset = 1;

    while(offset < length) {
        const uint8_t tag = data[offset++];
        if(offset >= length) {
            break;
        }

        uint64_t uval = 0;
        const size_t consumed = bfs::DecodeUleb128(data + offset, length - offset, &uval);
        if(consumed == 0 || offset + consumed > length) {
            break;
        }
        offset += consumed;

        // New Zwift virtual shifting road grade message. The grade value is
        // ZigZag-encoded and scaled in 0.01%.
        if(messageType == 0x22 && tag == 0x10) {
            int64_t zwiftGrade = static_cast<int64_t>(uval);
            zwiftGrade = (zwiftGrade >> 1) ^ -(zwiftGrade & 1);
            const float grade = static_cast<float>(zwiftGrade) / 100.0f;
            operations->setNewGrade(grade);
            return;
        }

        // Message type 0x2A / tag 0x10 carries gear ratio. It is intentionally
        // ignored here until virtual shifting needs an application-side sink.
    }
}
