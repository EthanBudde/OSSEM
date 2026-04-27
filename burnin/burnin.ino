// includes
#include <Adafruit_Sensor.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_BME680.h>
#include "Arduino.h"
#include <SPI.h>
#include <Wire.h>

#define SEALEVELPRESSURE_HPA (1013.25)
#define BMEPIN 33
#define SCDPIN 27
#define SGPPIN 12

// sensor Objects
Adafruit_SCD30 scd30;
Adafruit_SGP30 sgp;
Adafruit_BME680 bme(&Wire);

//blinkpin command
  //checks pin valid output
  //blinks selected pattern at selected pin

uint32_t getPinMode(uint32_t pin) {
  uint32_t bit = digitalPinToBitMask(pin);
  uint32_t port = digitalPinToPort(pin);
  if (port == NOT_A_PIN) return 0xFF;

  volatile uint32_t *reg = portModeRegister(port);
  volatile uint32_t *out = portOutputRegister(port);

  if (*reg & bit) {
    return OUTPUT; // Pin is configured as an output
  } else {
    return (*out & bit) ? INPUT_PULLUP : INPUT; // Checks if pull-up is active
  }
}

void blinkPin(int pin, int pattern){
  if(getPinMode(pin) != OUTPUT) {
    // some error notation
    return;
  }

  switch(pattern){
    case 0:
      digitalWrite(pin, HIGH);
      delay(375);
      digitalWrite(pin, LOW);
      delay(375);
      digitalWrite(pin, HIGH);
      delay(375);
      digitalWrite(pin, LOW);
      delay(375);
      break;
    case 1:
      digitalWrite(pin, HIGH);
      break;
    case 2:
      digitalWrite(pin, LOW);
      delay(15);
      break;
    default:
      return;
  }
}

void setup() {
  Serial.begin(115200);
  // pinmode
  pinMode(BMEPIN, OUTPUT);
  pinMode(SCDPIN, OUTPUT);
  pinMode(SGPPIN, OUTPUT);
  
  // bme wakeup
  if (!bme.begin()) {
		Serial.println("Could not find a valid BME680 sensor, check wiring!");
		while (1);
	}

	// bme constants init 
	bme.setTemperatureOversampling(BME680_OS_8X);
	bme.setHumidityOversampling(BME680_OS_2X);
	bme.setPressureOversampling(BME680_OS_4X);	
	bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
	bme.setGasHeater(320, 150); // 320*C for 150 ms

  // bme blink confirm
  blinkPin(BMEPIN, 0);

  // sgp wakeup
  if (! sgp.begin()){
    Serial.println("SGP sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  //sgp blink confirm
  blinkPin(SGPPIN, 0);

  // scd wakeup
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");

  // scd measurement interval
  if (!scd30.setMeasurementInterval(2)) {
    Serial.println("Failed to set measurement interval");
    while(1){ delay(10);}
  }
  Serial.print("Measurement interval: "); Serial.print(scd30.getMeasurementInterval());
  Serial.println(" seconds");

  // scd offset check
  Serial.print("Ambient pressure offset: "); Serial.print(scd30.getAmbientPressureOffset());
  Serial.println(" mBar");
  Serial.print("Altitude offset: ");Serial.print(scd30.getAltitudeOffset());
  Serial.println(" meters");
  Serial.print("Temperature offset: ");Serial.print((float)scd30.getTemperatureOffset()/100.0);
  Serial.println(" degrees C");
  Serial.print("Forced Recalibration reference: ");Serial.print(scd30.getForcedCalibrationReference());
  Serial.println(" ppm");

  // scd self calibration check
  if (!scd30.selfCalibrationEnabled(true)){
    Serial.println("Failed to enable or disable self calibration");
    while(1) { delay(10); }
  } if (scd30.selfCalibrationEnabled()) {
    Serial.print("Self calibration enabled");
  } else {
    Serial.print("Self calibration disabled");
  }
  Serial.println("\n\n");

  // scd blink confirm
  blinkPin(SCDPIN, 0);

}

void loop() {  
  // bme check valid
  if (! bme.performReading()) {
		return;
	}

  // bme oplight on
  blinkPin(BMEPIN, 1);

  // bme print block
  Serial.println("[BME BLOCK]");
  Serial.print(bme.temperature); Serial.println("[degC]");
  Serial.print(bme.pressure / 100.0); Serial.println("[hPaPres]");
  Serial.print(bme.humidity); Serial.println("[%humid]");
  Serial.print(bme.gas_resistance / 1000.0); Serial.println("[KOhmsGasR]");
  Serial.println();

  // bme oplight off
  blinkPin(BMEPIN, 2);
  
  //ping scd for valid data reading
  if (scd30.dataReady()) {
    if (!scd30.read()){ 
      return; 
    }
    // scd oplight on
    blinkPin(SCDPIN, 1);

    // scd print block
    Serial.println("[SCD BLOCK]");
    Serial.print(scd30.temperature); Serial.println("[degC]");
    Serial.print(scd30.relative_humidity); Serial.println("[%humid]");
	  Serial.print(scd30.CO2, 3); Serial.println("[ppmCO2]");
    Serial.println();
  
    //sgp oplight off
    blinkPin(SCDPIN, 2);
  }

  // sgp data valid
  if (! sgp.IAQmeasure()) {
    return;
  } else {
    // sgp oplight on
    blinkPin(SGPPIN, 1);

	  // sgp print block  
    Serial.println("[SGP BLOCK]");
    Serial.print(sgp.TVOC); Serial.println("[ppbTVOC]");
    Serial.print(sgp.eCO2); Serial.println("[ppmCO2]");
  
    //sgp oplight off
    blinkPin(SGPPIN, 2);
  }
  Serial.println();  Serial.println();  
  delay(900);
}