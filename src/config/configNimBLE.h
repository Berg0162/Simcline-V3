#ifndef CONFIG_NIMBLE_H_
#define CONFIG_NIMBLE_H_
// ----------------------------------------------------------------------------------------
// Your FIXED hardware Laptop/Trainer/Smartphone BLE MAC- or DEVICE-Addresses 
// Enter address string here like the printed format like [00:01:02:03:04:05 0]
// LAPTOP Fixed Device Address --------Public Type (0) --------- Random Type (1)-----------
//#define LAPTOPADDRESS "00:01:02:03:04:05 0" // Example Mac string of Public Type

//#define LAPTOPADDRESS "80:86:F2:7E:B2:87 0" // DELL Inspiron laptop of Public Type 
//#define LAPTOPADDRESS "84:FC:E6:6B:B7:85 0" // XIAO ESP32 S3 of Public Type


// TRAINER Fixed Device Address --------Public Type (0) --------- Random Type (1)----------
//#define TRAINERADDRESS "00:01:02:03:04:05 0"  // Example Mac string of Public Type

//#define TRAINERADDRESS "FE:B2:48:6C:2B:E1 1"  // Sim nRF52 of Random Type
//#define TRAINERADDRESS "FD:6C:BD:F3:50:95 0"  // nRF52 Sterzo Simulator of Public Type
//#define TRAINERADDRESS "E8:9F:6D:26:93:C2 0"  // Adafruit Feather ESP32 V2 of Public Type
//#define TRAINERADDRESS "84:FC:E6:6B:B7:85 0"  // XIAO ESP32 S3 of Public Type


// ---------------------------------------------------------------------------------------
// NimBLE Client, Server MITM related settings -------------------------------------------
// Notice All Wahoo KICKR trainers have BLE_APPEARANCE is ZERO !!!
#define BLE_APPEARANCE              1152 		// Generic Cycling
#define THISDEVICENAME              "Sim32"
#define ADVERTISED_SHORT_NAME       "Sim32"

// ---------------------------------------------------------------------------------------
// Training Apps set MTU after connection from 23 to 255, this allows for max data throughput!
const uint8_t MAX_PAYLOAD = 20;  // Max 20 (23-3) byte size for array DATA and strings
// Preferred Connection Parameters for max data throughput: Client-side (!) AND Server-side !
const uint16_t ConnectionMinInterval = 6; 	// Min interval (7.5ms)  
const uint16_t ConnectionMaxInterval = 12;	// Max interval (15ms)  
const uint16_t ConnectionLatency = 0;   	// No slave latency (immediate response)
const uint16_t ConnectionTimeout = 200; 	// Supervision timeout (2 seconds before disconnect)
const uint16_t ScanInterval = 160;			// Scan every 100ms (160 * 0.625ms)
const uint16_t ScanWindow = 80;				// Active scanning for 50ms (80 * 0.625ms)
const uint32_t ScanDuration = 15*1000;		// Duration to scan in milliseconds (seconds*1000)
const uint16_t ConnectionMTU = 255;     	// ATT (Attribute Protocol) Maximum Transmission Unit
const uint16_t AdvertiseMinInterval = 160;	// 100ms (160 * 0.625ms)
const uint16_t AdvertiseMaxInterval = 240;	// 150ms (240 * 0.625ms)

/* ---------------------------------------------------------------------------------------
* The Data field is dependent on the Bluetooth specification. In Bluetooth v4.0 and 4.1, 
* the maximum size of the Data field was 27 bytes. With Bluetooth v4.2, a new feature was 
* added to negotiate the Link Layer Data field length with Data Length Extension.
*/
//#define EXTENDEDDATALEN
#ifdef EXTENDEDDATALEN
const uint16_t ExtendedDataLen = 0x00FB; // Link Layer Preferred Extended Data Len: 251
#endif

// ---------------------------------------------------------------------------------------
// Set Arduino IDE Tools Menu --> Events Run On: "Core 1"
// Set Arduino IDE Tools Menu --> Arduino Runs On: "Core 1"
// xTask constants to set xTasks to run on "Core 0" or "Core 1"
const BaseType_t xTaskCoreID0 = 0;
const BaseType_t xTaskCoreID1 = 1;

#endif // CONFIG_NIMBLE_H_
