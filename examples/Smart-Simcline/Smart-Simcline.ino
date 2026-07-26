/*********************************************************************
 This is programming code for boards with Espressif ESP32-S3 Xtensa
 
      Tested with:  Seeed Studio XIAO ESP32-S3
                    LilyGo ESP32 T-Display S3 (170x320)

  The code uses heavily the supplied ESP32 libraries for different components! 
        
      see: https://github.com/h2zero/NimBLE-Arduino NimBLE Version 2.x

 Many have invested time and resources providing open source code!
 
        GPL-3.0 license, check LICENSE for more information
        All text must be included in any redistribution
*********************************************************************/


/* -----------------------------------------------------------------------------------------------------
 *             This code should work with all indoor cycling trainers that fully support,
 *        Fitness Machine Service, Cycling Power Service and Cycling Speed & Cadence Service
 * ------------------------------------------------------------------------------------------------------
 * NOTICE: that you need to have set first all config file settings in accordance with your specific setup!
 *         see: ../Documents/Arduino/libraries/Simcline-V3/src/config
 *
 *  The code links a BLE Server (a Peripheral to Zwift) and a BLE Client (a Central to the Trainer) with a bridge 
 *  in between, the ESP32-S3 being man-in-the-middle (MITM). The ESP32-S3 is an integral part of the Simcline,
 *  that interprets the exchanged road grade and moves the front wheel up and down with the change in inclination.
 *  The ESP32-S3-bridge can control, filter and alter the bi-directional interchanged data!
 *  The client-side (central) scans and connects with the Trainer relevant services: CPS and FTMS. It collects 
 *  all cyling data of the services and passes these on to the server-side....  
 *  The client-side supplies the Indoor Trainer with target and resistance control data.
 *  The server-side (peripheral) advertises and enables connection with cycling apps like Zwift and collects the app's  
 *  control commands, target and resistance data. It passes these on to the client-side....  
 *  The server-side supplies the app with the generated cycling data in return. 
 *  
 *  The client plus server (MITM) are transparent to the Indoor Trainer as well as to the training app Zwift or alike!
 *  
 *  Requirements: Zwift app or alike, ESP32-S3 board + TFT/Oled display and a supported Indoor Trainer
 *  0) Upload and Run this code on the Simcline (i.c. ESP32-S3 board)
 *  1) Start the Serial Monitor to catch debugging info and check the TFT display
 *  2) The code will do basic testing of mechanical parts and sensors
 *  3) Start/Power On the Indoor Trainer  
 *  4) Simcline and Trainer will pair as reported in the output
 *  5) Start Zwift on your computer or tablet and wait....
 *  6) Search on the Zwift pairing screens for the Simcline a.k.a. SIM32
 *  7) Pair: Power Source, Resistance and Cadence one after another with SIM32
 *  8) Optionally one can pair as well devices for heartrate and/or steering (Sterzo)
 *  9) Start the default Zwift ride or any ride you wish
 * 10) Make Serial Monitor output window visible on top of the Zwift window 
 * 11) Hop on the bike: do the work and feel resistance change with the road
 * 12) Inspect the info presented by Serial Monitor.....
 *  
 *   This device is identified with the name SIM32. You will see this only when connecting to Zwift on the 
 *   pairing screens! Notice: Zwift extends device names with additional numbers for identification!
 *  
*/ 

// ------------------------------------------------------------------------------------------------
// COMPILER DIRECTIVE to allow/suppress debug messages that help debugging...
// Uncomment "#define DEBUG" to activate debug messages from this main code section
#define DEBUG

#include <Simcline-V3.h>

// ------------------------------------------------------------------------------------------------
#include "WebConfigPortal.h"
// Create instance of the web configuration portal.
WebConfigPortal webConfigPortal;
// ------------------------------------------------------------------------------------------------
#include "BleClientBackend.h"
// ------------------------------------------------------------------------------------------------
// Instantiate the BLE Client Backend implementation.
// ------------------------------------------------------------------------------------------------
BleClientBackend bleclientbackend;
// ------------------------------------------------------------------------------------------------
#include "BleGattMirror.h"
// ------------------------------------------------------------------------------------------------
// Instantiate the BLE GATT mirror implementation.
// ------------------------------------------------------------------------------------------------
BleGattMirror blegattmirror(bleclientbackend);
// ------------------------------------------------------------------------------------------------
// Include code for low level measuring (VL6180X), control and execution of up- and down-movement
#include "lifter/Lifter.h"
// Create instance of Lifter class
Lifter* lift = new Lifter();
// Global variable for correct working of the mechanical motor functions
boolean IsBasicMotorFunctions = false; 

// -------------------------------- xControlLoop task definitions ---------------------------------
// Central Control Loop task to check for buttons pressed, connection status and change of road grade
TaskHandle_t xControlLoopHandle = NULL;
void xControlLoop(void* arg);

// ------------------------------------------------------------------------------------------------
void checkMITMdataChanged(void);

void loop() { // Arduino default loop() is no longer used: delete!!!
  vTaskDelete(NULL);  
}

void setup() {
  // Init USB connection first
  espBoard->initUSB(115200);
  // Print framework versions and board specifications 
  espBoard->info();    
  // Init and (optionally) scan for attached devices      
  espBoard->initWire(true);  
  // Board-specific setup
  espBoard->setup();  
  LOG("espBoard: Initialized!");

  // Initialize the connected display
  presentation->initDisplay(); 
  LOG("Main: Presentation Initialized!");
  delay(1000);

  LOG("------------ Simcline-V3 ----------------");
  LOG("   Board: %s", BOARD_NAME);
  LOG(" Display: %s", IDISPLAY);
  LOG(" Library: %s", CODE_VERSION);

  operations->init();
  LOG("Main: Operations Initialized!");

  if(webConfigPortal.start()) {
      presentation->ShowMessageWindow("Config", "WiFi Open", "60 sec", 500);
  }

  // Initialize Lifter Class data, variables, test and set to work !
  LOG("Main: Setup Lifter and Actuator!");
  if(lift->Init()) { 
      // Test Actuator and VL8106X for proper functioning
      LOG("Main: Test Actuator Motor Functions!");
      presentation->ShowMessageWindow("Testing", "Simcline", "Lifting", 0);
      if( lift->TestBasicMotorFunctions() ) {
          presentation->ShowMessageWindow("Testing", "Simcline", "Done!", 500);
          LOG("Main: Simcline Basic Motor Funtions are working!");
          // Is working properly --> Start Motor Control Task, running on core 1
          if(lift->StartControl(xTaskCoreID1)) { //Run on Core #1
              IsBasicMotorFunctions = true;
              // Put Simcline in neutral: 0% flat road position
              operations->levelGrade();
          }
      }
      if(!IsBasicMotorFunctions) { // Test Failed OR start control failed!
          presentation->ShowMessageWindow("Testing", "Simcline", "Failed!", 500);
          LOG("Main: Simcline >> ERROR << Basic Motor Funtions are NOT working!!");
      }
  } else { // Lift init failed....
        presentation->ShowMessageWindow("Testing", "VL6180X", "Timeout!", 500);
  }

  // Start a Central Control task to check for buttons pressed, connection status and change of road grade
  xTaskCreatePinnedToCore(xControlLoop, "xControlLoop", 8192, NULL, 5, \
                                        &xControlLoopHandle, xTaskCoreID1); //Run on Core #1
  delay(100); // Allow some time to settle
  LOG("Main: Central Control Loop Created!");

  presentation->ShowMessageWindow("Trainer", "Laptop", "Pairing!", 1500);

  // Now start first the BLE Backend and connect with Trainer
  if(!bleclientbackend.start()) {
        LOG("Main: >>> Wake Up the Trainer, Reset Board and Try Again!");
        while(true) {delay(1000);} // Fatal -> Hang!
  }

  // Start BLE GATT mirror for connection to Zwift
  if (!blegattmirror.start()) {
        LOG("Main: >>> Failed to start BLE GATT mirror!");
        while(true) {delay(1000);} // Fatal -> Hang!
  }
} // End of setup.

void checkMITMdataChanged(void) { 
    float grade;
    if(operations->getNewGradeIfChanged(grade)) {
        int16_t targetPosition = operations->GetNewTargetPosition(grade);
        if(IsBasicMotorFunctions) {  
            lift->SetTargetPosition(targetPosition);
        } 
        presentation->ShowRoadGrade(grade);
    }
// ------------------------------------------------------------------------------------------------
// Update the BLE Client-side backend.
//
// Responsibilities may include:
//   - maintaining BLE client connectivity (client backend)
//   - handling reconnect logic
//   - processing BLE notifications/indications
//   - updating backend-specific state machines
//
// This keeps the BLE Client-side responsive and up-to-date.
// ------------------------------------------------------------------------------------------------
    bleclientbackend.update();
// ------------------------------------------------------------------------------------------------
// Update the BLE Server-side frontend.
//
// Responsibilities may include:
//   - maintaining BLE Zwift connectivity
//   - handling reconnect logic when Zwift connection is lost
//   - processing BLE read/write/subscribe/notifications/indications from Zwift
//   - and passing on to Ble Client-side backend of read/write/subscribe/notifications/indications
//   - updating frontend-specific state machines
//
// This keeps the BLE Server-side responsive and up-to-date.
// ------------------------------------------------------------------------------------------------
    blegattmirror.update();
// Keep the startup web configuration portal time-boxed and non-blocking.
    webConfigPortal.update();
// Keep track of connection status and show on display
    presentation->ShowIconsOnTopBar(bleclientbackend.isConnected(), blegattmirror.isConnected(), false);
}

// Central Control task to check for BLE connection status and change of road grade
// Zwift changes road grade NOT at a constant pace but only when relevant!
// Interval time does rarely fall below 800 ms.
void xControlLoop(void *arg) {
  const TickType_t xDelay = 250 / portTICK_PERIOD_MS; // Block for 250ms
  while(1) {
    checkMITMdataChanged();
    vTaskDelay(xDelay);
  }
}
