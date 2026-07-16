# <img src="../images/SC_logo.png" width="64" height="64" alt="SIMCLINE Icon"> &nbsp; SIMCLINE for Bluetooth Smart trainers <br> based on the Adafruit Feather ESP32-S3 board with OLED display.

# Setup Feather ESP32-S3 for Simcline-V3
The board and display to work with can be activated in Simcline-V3 with the right settings.<br>
1) Open file, edit and save(!): `src/config/configBoard.h`
```C++
// ------------------------------------------------------------------------------------------------
// Define the board here, that is part of your system setup
//#define XIAO_ESP32S3
//#define LILYGO_T_DISPLAY_S3
#define ADAFRUIT_Feather_ESP32_S3
//#define ADAFRUIT_FEATHER_ESP32_V2
//#define YOUR_ESP32_BOARD
```
2) Open file, edit and save(!): `src/config/configDisplay.h`<br>
```C++
// ------------------------------------------------------------------------------------------------
// Define the display, that is part of your system setup
//#define NODISPLAY
#define OLEDSSD1306_128x64
//#define LILYGO_T_DISPLAY_S3
//#define YOURDISPLAY
//#define SERIALDISPLAY
// ------------------------------------------------------------------------------------------------
```
<b>Warning</b>: When a new version of Simcline-V3 is installed in Arduino IDE 2 it will override <b>ALL</b> files of a previous version! If you have made modifications in a file that is part of `Simcline-V3` --> <b>Make</b> a <b>copy</b> of the file(s) in question <b>BEFORE</b> you <b>install</b> a new library version!

## Arduino IDE 2.x
The present code is developed on Arduino IDE 2.2. Notice that you will need an Arduino IDE that is tailored for this specific <b>Adafruit Feather ESP32-S3</b> processor! Please follow the installation instructions for the [Adafruit Feather ESP32-S3](https://learn.adafruit.com/adafruit-esp32-s3-feather))<br>

## Before you start
Select in the menu bar of Arduino IDE 2.2 <b>Tools</b>, the settings for the specific type of your <b>Adafruit Feather ESP32-S3</b> processor!

# Feather ESP32-S3 pinout

<img src="../images/adafruit_Feather_ESP32S3.png"  alt="Feather ESP32-S3 pinout">

# Electronic Components and Wiring for version 2.0
<p align=left>
<img src="../images/Feather_ESP32S3_Wiring.png"  alt="Circuitry version 2.0">
</p>
<br clear="left">

In this project is choosen for the following 5 compact active components that are slightly different from the earlier SIMCLINE project and that can finally all be mounted inside the components box:<br>

<b>Adafruit DRV8871 DC Motor Driver</b><br>
A small one channel motor driver for 12 V (6.5 - 48 V) and 3,6 Amperes max. This board enables the processor to set the Actuator motor in up or down movement. It transforms logical digital levels (Go Up, Go Down and Stop) from the Feather ESP32 to switching of 12 Volt at 3,6 Amperes max., the levels at which the Actuator works. Notice that default the board comes limited to 2,6 Amperes and you need to add a resistor to set for max current level. Install Vertical Through Hole Male PCB Header Pins on the board; this will allow correct mounting of the board inside the components box!<br>

<b>Adafruit Feather ESP32-S3</b><br>
**1. Adafruit Feather ESP32-S3 (8 MB Flash, No PSRAM)**
- Processor: ESP32-S3 dual-core @ 240 MHz
- Flash: 8 MB
- PSRAM: None
- Internal SRAM: 512 KB
- Connectivity: Wi-Fi 4 (2.4 GHz) + Bluetooth LE

**2. Adafruit Feather ESP32-S3 (4 MB Flash, 2 MB PSRAM)**
- Processor: ESP32-S3 dual-core @ 240 MHz
- Flash: 4 MB
- PSRAM: 2 MB
- Internal SRAM: 512 KB
- Connectivity: Wi-Fi 4 (2.4 GHz) + Bluetooth LE

For future development with Simcline-V3, you should instead look for an ESP32-S3 board with **8 MB Flash** and **8 MB PSRAM** (such as the XIAO ESP32-S3), because neither of these Feather variants provides both large Flash and large PSRAM. However, both boards will work (for now) with an **APP Partition Scheme** of at least **3 MB**! Notice: **I have NOT tested these boards**.<br>
[Adafruit Feather ESP32-S3](https://learn.adafruit.com/adafruit-esp32-s3-feather/overview)

<b>OLED display blue/white 128x64 pixels (0,96 Inch, I2C)</b><br>
Small display board has a critical overall board size of 25 mm * 27 mm (!); (See for example: [Webshop](https://www.gadgetbouw.nl/shop/0-96inch-128x64-i2c-oled-scherm-ssd1306-wit-of-blauw)). Display area itself is: 25 mm x 14 mm. Shows cycling data and diagnostic info that is gathered during operation by the Feather ESP32 to inform the SIMCLINE user about relevant information. NOTICE: a) Many different formfactors are offered at webshops; b) Install Vertical Through Hole Male PCB Header Pins on the board; this will allow correct mounting of the board inside the components box!<br>

<b>Pololu Time-of-Flight-Distance sensor VL6180X</b><br> The sensor board (12.7 * 17.8 mm) contains a very tiny laser source, and a matching sensor. The VL6180X can detect the "time of flight", or how long the laser light has taken to bounce back to the sensor. Since it uses a very narrow light source, it is perfect for determining distance of only the surface directly in front of it. The sensor registers quite accurately the (change in) position of the wheel axle during operation, by measuring the distance between the top of the inner frame and the reflection plate that is mounted on the carriage. The distance feedback of the sensor is crucial for determining how to set the position of the carriage and axle in accordance with the grade information that for example Zwift is using to set the resistance of the trainer. NOTICE: a) VL6180X boards are also offered by different suppliers and have different formfactors; b) Install Straight Angle Through Hole Male PCB Header Pins on the board; this will allow later flat mounting of the sensor board in the components box!<br>

<b>Pololu D24V5F5</b><br>
This is a small 5V, 500mA Step-Down Voltage Regulator that is responsible for voltage conversion from 12V to 5V, the power supply for all components boards. NOTICE: Install Straight Angle Through Hole Male PCB Header Pins on the board; this will allow later easy mounting of the sensor board in the components box!<br>

All components are documented very well and are low cost. There are lots of examples for use in an Arduino environment. They have turned out to be very reliable. The exact wiring of the components can be followed in the figure above.<br>
