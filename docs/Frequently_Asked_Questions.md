# [1]
<b>Who is in control?</b><br>

When a training app such as Zwift connects to a trainer through Bluetooth FTMS, it normally requests control through the FTMS Control Point. Although a trainer may support more than one Bluetooth connection, control permission and telemetry connections are not the same thing. In practice, many trainers allow only one app or device at a time to control resistance. The exact behavior depends on the trainer and its firmware.

If the trainer does not appear on an app's pairing screen, it may still be connected to another app or device. Close other cycling and fitness apps that could connect automatically, including apps on nearby phones and tablets. If necessary, temporarily disable Bluetooth on those devices. Avoid enabling Flight Mode when the device's Wi-Fi connection is needed for Zwift or the Zwift Companion app.

Simcline-V3 sits between Zwift and the trainer: Zwift connects to Simcline-V3, and Simcline-V3 connects to the trainer. Do not also connect Zwift directly to the trainer as the controllable device.

# [2]
<b>What about ANT+ FE-C and FTMS at the same time?</b><br>

Some trainers advertise both ANT+ FE-C and Bluetooth FTMS. Whether they accept control commands over both protocols at the same time depends on the trainer's firmware. Some trainers arbitrate between controllers, while others reject a second controller or behave unpredictably.

For a reliable Simcline-V3 setup, use one control path only. When using Bluetooth FTMS, configure Zwift to connect to **SIM32** and not directly to the trainer. If Zwift keeps selecting the trainer through ANT+ FE-C, remove the ANT+ dongle temporarily or disable that pairing, restart the equipment, and pair again. Removing the dongle is a troubleshooting measure rather than a requirement of the FTMS specification.

# [3]
<b>What about Zwift Hub and JetBlack trainer users?</b><br>

Some trainers can bridge a heart-rate monitor over the trainer's Bluetooth connection. This is particularly useful on platforms with a limited number of simultaneous Bluetooth connections, such as Apple TV. Trainer features and Zwift support change over time, so consult the current documentation for your exact trainer model and firmware.

Simcline-V3 mirrors the relevant heart-rate service and is designed to pass the bridged heart-rate data through transparently. This functionality has been implemented for supported Zwift Hub and JetBlack configurations, but compatibility cannot be assumed for every JetBlack model or future firmware version. When reporting a problem, include the exact trainer model, trainer firmware, Zwift platform, and connection method.

For background information, see the [DCRainmaker Zwift Hub review](https://www.dcrainmaker.com/2022/10/zwift-hub-smart-trainer-in-depth-review-the-best-bang-for-your-buck.html). Notice that this 2022 review covers the original Zwift Hub; it is not a compatibility list for every later Zwift Hub or JetBlack model.

# [4]
<b>How do I remove old devices from Zwift?</b><br>

Zwift can retain pairing information for trainers and sensors used in earlier sessions. Because devices are identified using more than just the names shown on the pairing screen, changing a device name does not necessarily remove an old pairing.

To remove stored device pairings on a PC or Mac:

1. Close Zwift completely.
2. Open the `Zwift` folder inside your `Documents` folder.
3. Make a backup if you may want to restore the current settings.
4. Delete `knowndevices.xml`. Editing this XML file manually is recommended only if you understand its structure.
5. Launch Zwift and pair the devices again.

Zwift's official instructions for a complete preference reset may also tell you to delete `prefs.xml`. Doing so resets more than the stored device pairings, so it is not normally necessary when the only goal is to remove old devices.

# [5]
<b>How does Simcline-V3 use the two processor cores of the ESP32-S3?</b><br>

The ESP32-S3 has two Xtensa processor cores. Simcline-V3 uses them to separate time-sensitive actuator control from the substantial amount of Bluetooth traffic handled through NimBLE-Arduino.

During a ride, Zwift regularly sends new simulation settings. One of these settings is road grade, expressed as a percentage. Simcline-V3 converts the received grade into an actuator target position. The actuator can move up, move down, or stop, so its position must be monitored continuously with the Time-of-Flight sensor. A semaphore protects target-position data that is shared by different tasks.

At the same time, the trainer sends cycling data through services such as CPS, CSC, FTMS, and optionally HRM. Simcline-V3 mirrors this traffic between the trainer and Zwift and forwards control data in the opposite direction.

The software is configured so that:

- Core 0 primarily handles Bluetooth and NimBLE-Arduino work.
- Core 1 handles actuator control, the display, and other application tasks.

This division describes the intended Simcline-V3 configuration. Actual scheduling also depends on the ESP32 Arduino core, NimBLE-Arduino, and the board settings selected in the Arduino IDE. Follow the project's documented core settings when compiling the software.

# [6]
<b>Why does my trainer connect to SIMCLINE only intermittently?</b><br>

Connection problems are often caused by a competing app or by starting the components in an unsuitable order. Use the following sequence:

1. Close Zwift and any other app that might connect to the trainer. Power off the trainer and Simcline-V3.
2. Power on the trainer and wait until it has completed startup and is advertising.
3. Power on or reset Simcline-V3. It will perform its startup checks and scan for the trainer.
4. Wait until Simcline-V3 and the trainer are connected. If they do not connect, reset Simcline-V3 once and try again.
5. Start Zwift only after the trainer and Simcline-V3 have connected.
6. On the Zwift pairing screen, select **SIM32** as **POWER SOURCE**.
7. Select **SIM32** as **RESISTANCE** or **CONTROLLABLE**, depending on the wording used by your Zwift version.
8. Select **SIM32** as **CADENCE**.
9. Confirm that the selected devices show **CONNECTED**, then continue.

It is normally sufficient to close Zwift and competing apps; the laptop itself does not need to be powered off. If the first connection still fails, follow the troubleshooting advice in [FAQ #1](#1) and check that Zwift has not paired directly with the trainer through Bluetooth or ANT+.

# [7]
<b>Will Simcline-V3 work out of the box with my ESP32-S3 development board?</b><br>

In most cases, yes, provided that the ESP32-S3 board has at least **4 MB of flash memory**; **8 MB is preferred**. PSRAM is optional. In the Arduino IDE **Tools** menu, select a partition scheme that provides an application partition of at least **3 MB**.

Start with the **Smart-MITM** application to confirm that the Bluetooth MITM concept works with your board. A complete SIMCLINE setup also requires a board definition for the hardware connections. Complete the ESP32 board class in:

`src/board/YOUR_ESP32_Board.h`

At minimum, define the pins for the Time-of-Flight sensor over I2C and the DRV8871 actuator motor driver. Two button inputs are optional. Some boards may also require board-specific initialization code.

```cpp
#define PIN_ACTUATOR_1  -1  // Connect to IN2 on the Adafruit DRV8871 motor-driver board
#define PIN_ACTUATOR_2  -1  // Connect to IN1 on the Adafruit DRV8871 motor-driver board

// Optional button connections
//#define PIN_BUTTON_1  -1
//#define PIN_BUTTON_2  -1

#define PIN_I2C_SCL     SCL // Verify the Arduino IDE default for your board
#define PIN_I2C_SDA     SDA // Verify the Arduino IDE default for your board
```

To find the default pin assignments used by Arduino-ESP32, locate your board in the [Arduino-ESP32 variants directory](https://github.com/espressif/arduino-esp32/tree/master/variants).

# [8]
<b>How do Zwift, the trainer, and Simcline-V3 handle road-grade changes?</b><br>

Several factors can cause the grade shown by Simcline-V3 to differ temporarily from the grade displayed by Zwift:

1. **Trainer Difficulty:** Zwift applies its Trainer Difficulty setting to the gradient sent to the controllable trainer. At 50%, an uphill gradient is typically sent at half its displayed magnitude; at 100%, the full uphill gradient is sent. This setting changes the simulated gradient and therefore the gearing needed for a climb, but it does not directly scale the rider's power. Simcline-V3 reacts to the value that Zwift sends to the trainer.

2. **Timing and trainer response:** A trainer cannot change resistance instantaneously. Zwift, the trainer, the Bluetooth connection, and the actuator each introduce some delay. To accommodate for the trainer delay, the Zwift app sends gradients 1 to 1.5 second earlier(!) than what you observe on your screen (the displayed road grade value on the Zwift window). SIMCLINE may therefore begin moving before the on-screen grade appears to match, particularly on rapidly rolling terrain. The precise timing varies by trainer, firmware, Zwift version, and connection method. When riding rolling hills, in rapid succession, like Zwift's **Titans Grove**, this is most visible! Notice that the responsiveness is benefiting!

3. **Downhill behavior:** Zwift can reduce the magnitude of negative gradients sent to a trainer. As a commonly observed example, an on-screen grade of -8% may result in approximately -4% being sent. This behavior can change and may depend on Zwift settings or trainer support, so regard the exact ratio as observed behavior rather than a protocol guarantee. Simcline-V3 displays and responds to the grade value it receives.

4. **Initial simulation command:** Zwift may send a non-zero simulation value (randomly between 2.0 and 5.0) while preparing a ride. Simcline-V3 can respond to this command before the selected ride has started, however, Simcline-V3 filters the early values and keeps the actuator at level (flat road).

5. **Grade-Change-Factor:** Simcline-V3 has its own Grade-Change-Factor, originally introduced to reduce actuator movement when using a noisy or slow actuator. Its default value is 100%, and it can be changed dynamically with the Simcline app. The factor affects the actuator movement and displayed Simcline grade; the original grade command is forwarded to the trainer, so trainer resistance is not modified by this setting.

When the Zwift and Simcline-V3 displays differ, first check Zwift's Trainer Difficulty and the Simcline Grade-Change-Factor. Short-lived differences during grade transitions can be normal. Other cycling apps may apply their own grade-scaling and timing rules; do not assume that Rouvy, MyWhoosh, and Zwift behave identically.

# [9]
<b>How do I manage changed device addresses?</b><br>

When **Smart-MITM** runs, it detects the Bluetooth device addresses of the trainer and the Zwift device and stores them in ESP32-S3 NVS (Non-Volatile Storage). **Smart-Simcline** uses the stored addresses to reconnect to the intended client and server.

To inspect or change the addresses stored in NVS, use the **`Test_Board_plus_NVS`** tool in `Simcline-V3/examples`. An address must have both the correct value and the correct address type to be resolved during connection.

As a second option, fixed addresses can be defined in:

`src/config/configNimBLE.h`

Look for the following section:

```cpp
// Fixed Bluetooth addresses for the Zwift device and trainer.
// Format: "00:01:02:03:04:05 0"
// Address type: public (0) or random (1).

#define LAPTOPADDRESS  "00:01:02:03:04:05 0" // Example public address
#define TRAINERADDRESS "00:01:02:03:04:05 1" // Example random address
```

When uncommented, `LAPTOPADDRESS` and `TRAINERADDRESS` override the corresponding values stored in NVS.

Be aware that phones, tablets, and computers may use private or rotating Bluetooth addresses. Such an address may change and is therefore unsuitable as a permanent compile-time value. Prefer the NVS discovery procedure unless you know that the device exposes a stable address. Do not publish real device addresses in screenshots, logs, or repository files.
