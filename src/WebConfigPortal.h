#pragma once

#include <Arduino.h>
#include "config/configPortal.h"

#if defined(ENABLE_CONFIG_PORTAL)
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#endif

#include "Operations.h"

/**
 * WebConfigPortal
 *
 * Lightweight startup WiFi configuration portal for ride-related preferences.
 *
 * Purpose
 * -------
 * Open a short browser-based configuration window before the ride so the user
 * can adjust operational settings without a dedicated smartphone BLE app.
 *
 * Architectural position
 * ----------------------
 * Runs alongside startup initialization and is serviced from the main update
 * loop while the portal is active. It reads and writes configuration only
 * through Operations, which remains the authoritative owner of bounds checking
 * and NVS storage.
 *
 * Responsibilities
 * ----------------
 * - Start and stop the temporary WiFi access point and HTTP listener.
 * - Serve the configuration page for ascent, descent, grade factor, and display
 *   selection.
 * - Refresh displayed values from Operations before serving configuration.
 * - Store submitted values back through Operations.
 * - Keep WiFi activity limited to the configured startup window.
 *
 * Design notes
 * ------------
 * This portal is intentionally small and self-contained to reduce memory and
 * timing pressure on the BLE MITM. It does not maintain a parallel preference
 * store; all persistent configuration flows through Operations.
 */
class WebConfigPortal
{
public:
    WebConfigPortal();

    bool start();
    void update();
    void stop();
    bool isActive() const;

private:
    bool initialized = false;
    OPS* operations = nullptr;
    uint8_t ascent = 0;
    uint8_t descent = 0;
    uint8_t factor = 0;
    uint8_t display = 0;

#if defined(ENABLE_CONFIG_PORTAL)
    void refreshFromOperations();
    void storeToOperations();
    String pageHtml() const;
    void handleClient();
    void handleSave(NetworkClient& client, const String& body);
#endif
};
