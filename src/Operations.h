#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#include <Arduino.h>
#include <string>

#include <NimBLEAddress.h> 

#include <tuple>
using PhonePrefs = std::tuple<unsigned char, unsigned char, unsigned char, unsigned char>;
using std::get;

// Struct containing Device info to administer dis/connected devices
typedef struct
{
  NimBLEAddress PeerAddress;
  std::string PeerName;
} Device_info_t;

class UTILS;

/**
 * OPS
 *
 * Central operations state and preference manager for the MITM and lifter.
 *
 * Purpose
 * -------
 * Provide one authoritative place for runtime ride state, road-grade updates,
 * device pairing data, user configuration, and NVS-backed preferences.
 *
 * Architectural position
 * ----------------------
 * Used as a singleton by BLE, configuration, presentation, and lifter-related
 * components. BLE observers publish decoded grade and connection context here,
 * while the control loop and WebConfigPortal read or update operational settings
 * through this class.
 *
 * Responsibilities
 * ----------------
 * - Store trainer and laptop peer address information.
 * - Manage the one-time unknown-device pairing escape.
 * - Accept passive road-grade updates from BLE write observers.
 * - Provide thread-safe "latest grade changed" access for the control loop.
 * - Convert road grade into lifter target position using configured limits.
 * - Validate, expose, and persist phone/configuration preferences.
 *
 * Design notes
 * ------------
 * Operations is deliberately the single source of truth for preference values
 * and NVS storage. Other components should not duplicate bounds checking or
 * persistent storage logic. Grade updates use a latest-value model: consumers
 * receive the newest grade change available when they poll, not a history of
 * every received BLE write.
 */
class OPS {
private:
    OPS();
    static OPS* instance;
    UTILS* utils;
    float grade;
    boolean gradeChanged;
    boolean enableNVS; 
    SemaphoreHandle_t xGradeChangeMutex;
    long RawgradeValue;
    uint16_t aRGVmin;
    uint16_t aRGVmax;
    uint8_t GradeChangeFactor;
    uint8_t OledDisplaySelection;
    int16_t EMA_TargetPositionFilter(int16_t current_value);
    float filterGrade(float currentGrade);
public:
    ~OPS();
    static OPS* getInstance();
    Device_info_t Trainer;
    Device_info_t Laptop;
    boolean enableUnknownDev;
    void setEnableUnknownDev(void);
    bool getNewGradeIfChanged(float& gradeOut);
    void setNewGrade(float grade);
    void setGradeChanged(boolean changed);
    void stepGrade(bool Up);
    void levelGrade(void);
    int16_t GetNewTargetPosition(float RoadGrade);
    void init(boolean storage=true);
    void storePhonePrefs(PhonePrefs phoneTuple);
    PhonePrefs getPhonePrefs(void);
}; // Class OPS

#endif
