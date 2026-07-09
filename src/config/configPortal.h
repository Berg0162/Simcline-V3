#ifndef CONFIGPORTAL_H_
#define CONFIGPORTAL_H_

#include <Arduino.h>

// Enable the startup WiFi configuration portal only on builds that have enough
// IRAM headroom for WiFi + NimBLE. Classic ESP32 builds can overflow IRAM with
// the current MITM feature set, even with a minimal WiFiServer portal.
//
// Uncomment to force-enable after testing the selected board/core combination.
//#define ENABLE_CONFIG_PORTAL


// Access Point SSID and password used by the startup configuration portal.
#define PORTAL_APPNAME 	"SimclineCfg"
#define PORTAL_PASSWORD "12345678"

// Enable automatically for newer ESP32 targets that are better suited to
// combining WiFi, NimBLE, and display code. Keep classic ESP32 excluded unless
// explicitly force-enabled above because it has shown IRAM overflow here.
#if defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C3)  || \
    defined(CONFIG_IDF_TARGET_ESP32C5)  || \
    defined(CONFIG_IDF_TARGET_ESP32C6)  || \
    defined(CONFIG_IDF_TARGET_ESP32C61)
#define ENABLE_CONFIG_PORTAL
#endif

#endif // CONFIGPORTAL_H_
