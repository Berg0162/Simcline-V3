#ifndef COMPONENTS_UTILS_H_
#define COMPONENTS_UTILS_H_

#include <Arduino.h>
#include <string>
#include <cstring>
#include <tuple>
#include <stdarg.h>  // Required for variable argument functions

using SimPrefs = std::tuple<unsigned short, unsigned short, unsigned char, unsigned char>;
using std::get;

#include <NimBLEAddress.h> 

/**
 * UTILS
 *
 * Shared helper class for small conversions, validation, formatting, and
 * diagnostic support.
 *
 * Purpose
 * -------
 * Provide reusable utility functions that do not belong to one specific BLE,
 * presentation, configuration, or lifter component.
 *
 * Architectural position
 * ----------------------
 * Used as a singleton by application components that need common helper
 * behavior, especially for BLE address handling, hexadecimal formatting,
 * logging, and compact protocol-related conversions.
 *
 * Responsibilities
 * ----------------
 * - Convert NimBLE addresses and binary payloads to printable strings.
 * - Validate MAC address strings and NimBLE address values.
 * - Read and write small legacy/simple NVS helper values when needed.
 * - Provide formatted logging support.
 * - Offer small domain helpers such as virtual shifting gear-ratio lookup.
 *
 * Design notes
 * ------------
 * Utilities should stay lightweight and stateless wherever possible. It should
 * not become the owner of application policy; operational state, bounds
 * checking, and authoritative preference storage belong in Operations.
 */
class UTILS {
private:
  UTILS();
  static UTILS* instance;
public:
  ~UTILS();
  static UTILS* getInstance();
  SimPrefs getSIMprefsNVS(void);
  void setSIMprefsNVS(SimPrefs simTuple);
  void clearSIMprefsNVS(void);
  void setMacAddressNVS(const char* key, NimBLEAddress &address);
  NimBLEAddress getMacAddressNVS(const char* key);
  bool parseMacAddressString(const String& macAddress, std::string& address, uint8_t& type);
  std::string toString(const NimBLEAddress& macAddress);
  std::string toHexString(const uint8_t* source, uint8_t length);
  std::string toHexString(const std::string& str);
  bool isValid(const std::string& macString);
  bool isValidNimBLEAddress(const NimBLEAddress &address);
  void logF(const char* format, ...);

  std::string getHexString(const uint8_t* data, size_t length);
  std::string getHexString(const std::string& str);
  std::string getHexString(std::vector<uint8_t> data);
  std::string getHexString(std::vector<uint8_t>* data);

  int getGearNumberFromRatio(float gearRatio);
  static const std::vector<float> knownRatios;
};

#endif // COMPONENTS_UTILS_H_
