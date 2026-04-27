// includes
#include <Adafruit_Sensor.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_BME680.h>
#include "Arduino.h"
#include <ezTime.h>
#include <SPI.h>
#include <WiFi.h> // for WiFi shield or ESP32
#include <Wire.h>

#define SEALEVELPRESSURE_HPA (1013.25)

// sensor Objects
Adafruit_SCD30 scd30;
Adafruit_SGP30 sgp;
Adafruit_BME680 bme(&Wire);

// timezone Object
Timezone myTZ;

// network ssid and pass
char ssid[]     = "HOWLING-ABYSS";
char password[] = "bigswag!";

int count = 0;

void setup() {
  // serial comms
  Serial.begin(115200);
	while (!Serial) delay(10);	// will pause until serial terminal open
	
  // wifi signal chain
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		Serial.println("Connecting ...");
    
    if(count == 120){
      Serial.println("cannot connect to wifi, no timestamps");
      break;
    }
    count++;
		delay(250);
  }
	
  if(WiFi.status() == WL_CONNECTED){
    count = 0;
    Serial.print("connected to network -> ");
    Serial.println(ssid);  
    waitForSync();
    myTZ.setLocation(F("America/Los_Angeles"));
  }
  // timezone sync and location
  
  
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
  
  // sgp wakeup
  if (! sgp.begin()){
    Serial.println("SGP sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

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
}

void loop() {
  // timestamp
  if(count == 0){Serial.println(myTZ.dateTime("H:i:s.v"));}
  
  // bme print block
  // we assume the BME is always gonna give us something, so its at the top
  Serial.println("[BME BLOCK]");
  Serial.print(bme.temperature); Serial.println("[degC]");
  Serial.print(bme.pressure / 100.0); Serial.println("[hPaPres]");
  Serial.print(bme.humidity); Serial.println("[%humid]");
  Serial.print(bme.gas_resistance / 1000.0); Serial.println("[KOhmsGasR]");
  Serial.println();

  //ping scd for valid data reading
  if (scd30.dataReady()) {
    if (!scd30.read()){ 
      return; 
    } if (! bme.performReading()) {
		return;
	  }
    // scd print block
    Serial.println("[SCD BLOCK]");
    Serial.print(scd30.temperature); Serial.println("[degC]");
    Serial.print(scd30.relative_humidity); Serial.println("[%humid]");
	  Serial.print(scd30.CO2, 3); Serial.println("[ppmCO2]");
    Serial.println();
  }

  
  if (! sgp.IAQmeasure()) {
    return;
  } else {
	// sgp print block  
    Serial.println("[SGP BLOCK]");
    Serial.print(sgp.TVOC); Serial.println("[ppbTVOC]");
    Serial.print(sgp.eCO2); Serial.println("[ppmCO2]");
  }
  Serial.println();  Serial.println();  
  delay(900);
}