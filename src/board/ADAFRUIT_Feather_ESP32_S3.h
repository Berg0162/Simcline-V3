#ifndef ADAFRUITFEATHERESP32S3_H_
#define ADAFRUITFEATHERESP32S3_H_

/*
The Adafruit ESP32-S3 Feather is a highly-integrated, low-power, 2.4 GHz Wi-Fi/BLE System-on-Chip 
(SoC) solution that has built-in native USB. Memory comes in different flavours depending the product:
- with 4 MB flash and 2 MB PSRAM 
- 8MB Flash and NO PSRAM
*/

static const char* BOARD_NAME = "Adafruit Feather ESP32-S3";

/* Warning I/O Pins can have identical board position but different I/O Pin declarations for 
 * connection with the pins of the Motor driver board
*/

#define PIN_ACTUATOR_1 	A0 // GPIO26 --> connected to pin IN2 of the DRV8871 Motor Driver board
#define PIN_ACTUATOR_2 	A1 // GPIO25 --> connected to pin IN1 of the DRV8871 Motor Driver board

// Optional Button 1 and 2 PIN connections
//#define PIN_BUTTON_1   A2 // GPIO34
//#define PIN_BUTTON_2   A3 // GPIO39

// Wire (I2C) definitions and PIN connections
#define PIN_I2C_SCL 	SCL   // GPIO20 I2C clock pin SCL printed on Feather board
#define PIN_I2C_SDA 	SDA   // GPIO22 I2C data pin SDA printed on Feather board

// SPI definitions and PIN connections
#define PIN_SPI_MOSI 	MOSI // GPIO19 MO printed on Feather board
#define PIN_SPI_MISO 	MISO // GPIO21 MI printed on Feather board
#define PIN_SPI_CLK  	SCK  // GPIO5 SCK printed on Feather board
#define PIN_SPI_DC   	32   // GPIO32
#define PIN_SPI_CS   	14   // GPIO14
#define PIN_SPI_RES  	15   // GPIO15

#include "IBoard.h"

class ESP32Board : public IBoard {
	private:

	public:
	ESP32Board() {}

	/* The Feather ESP32-S3 has a I2C_POWER pin that must be pulled HIGH
 	 * to enable power to the STEMMA QT port. Without it, the QT port will not work! 
	*/
	// Declaration of board-specific setup function
	void setup(void) override {
   		// Turn Stemma connector on by pulling pin HIGH.
  		pinMode(PIN_I2C_POWER, OUTPUT);
  		digitalWrite(PIN_I2C_POWER, HIGH);

  		// Setup the optional buttons on the appropriate pins
		#if defined(PIN_BUTTON_1) && defined(PIN_BUTTON_2)
  		pinMode(PIN_BUTTON_1, INPUT_PULLUP);
  		pinMode(PIN_BUTTON_2, INPUT_PULLUP);
		#endif

  		// Setup Actuator control pins
  		pinMode(PIN_ACTUATOR_1, OUTPUT);
  		pinMode(PIN_ACTUATOR_2, OUTPUT);
		// Set Actuator default to brake
  		digitalWrite(PIN_ACTUATOR_1, LOW);
  		digitalWrite(PIN_ACTUATOR_2, LOW);
	}
};

#endif // ADAFRUITFEATHERESP32S3_H_
