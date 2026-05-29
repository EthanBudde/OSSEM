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
#define BMEPIN 33
#define SCDPIN 27
#define SGPPIN 12

const int FAN1_PIN = 26;
const int FAN2_PIN = 25;
const int BUTTON1_PIN = 13;
const int BUTTON2_PIN = 21;

const int PWM_FREQ = 25000;
const int PWM_RESOLUTION = 8;

bool trigger = false;

int fanMode = 1;
int reading[2];

int fanSpeed[] = {
  0,
  90,
  160,
  255
};

int lastButtonReading[2];
int stableButtonState[2] = {1, 1};
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// sensor Objects
Adafruit_SCD30 scd30;
Adafruit_SGP30 sgp;
Adafruit_BME680 bme(&Wire);

// timezone Object
Timezone myTZ;

// network ssid and pass
char ssid[]     = "Dav";
char password[] = "00000000";

int count = 0;
int t_dataBegin = 0;

bool dumbTimestamps = false;

// coherence structs, last/current holders
struct bmeValues{
  float temp;
  float pres;
  float humid;
  float gasr;
};

bmeValues bmeCurrent, bmeLast;

struct scdValues{
  float temp;
  float humid;
  float co2;  
};

scdValues scdCurrent, scdLast;

struct sgpValues{
  int tvoc;
  int co2;
};

sgpValues sgpCurrent, sgpLast;

//getPinMode
//  checks pin is valid port, returns pin type
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

//blinkpin
  //checks pin valid output
  //blinks selected pattern at selected pin
void blinkPin(int pin, int pattern){
  if(getPinMode(pin) != OUTPUT) {
    // some error notation
    return;
  }

  switch(pattern){
    case 0:                   // blink confirm - 2 long blinks, for setup
      digitalWrite(pin, HIGH);
      delay(500);
      digitalWrite(pin, LOW);
      delay(500);
      digitalWrite(pin, HIGH);
      delay(500);
      digitalWrite(pin, LOW);
      delay(500);
      break;
    case 1:                   // enable LED
      digitalWrite(pin, HIGH);
      break;
    case 2:                   // disable LED
      digitalWrite(pin, LOW);
      delay(20);
      break;
    default:
      return;
  }
}

void setup() {
  // serial comms
    // serial 
  Serial.begin(115200);
  
  // pinmode
  pinMode(BMEPIN, OUTPUT);
  pinMode(SCDPIN, OUTPUT);
  pinMode(SGPPIN, OUTPUT);
  
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  ledcAttach(FAN1_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN1_PIN, fanSpeed[fanMode]);

  ledcAttach(FAN2_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN2_PIN, fanSpeed[fanMode]);

  Serial.println("ESP32 Feather HUZZAH32 fan control started.");
  Serial.println("Mode 0: OFF");
  
  blinkPin(BMEPIN, 1);
  blinkPin(SCDPIN, 1);
  blinkPin(SGPPIN, 1);

  //while (!Serial) delay(10);	// will pause until serial terminal open
	
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
  
  blinkPin(BMEPIN, 2);
  blinkPin(SCDPIN, 2);
  blinkPin(SGPPIN, 2);

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

  // change this
  for(int i=0;i<11;i++){
      scdCurrent.temp = scd30.temperature;
      scdCurrent.humid = scd30.relative_humidity;
      scdCurrent.co2 = scd30.CO2;
  }
}

void loop() {
  //button check
  readButton(BUTTON1_PIN, 1);
  readButton(BUTTON2_PIN, 2);
  
  // timestamp
  if(trigger) {
	  Serial.print("!");
    trigger = false;
  }
  if(dumbTimestamps == false){
	  Serial.println(myTZ.dateTime("H:i:s.v"));
	}else if(dumbTimestamps == true){
    char buffer[10];
    int time = (millis() - t_dataBegin) / 1000; 
    sprintf(buffer, "%03d", ((millis() - t_dataBegin) % 1000));
    Serial.print(time); Serial.print("."); Serial.println(buffer);
  }
 

  // refactor for controlability, testing
  // try/except with a nonblocking timer?
  scdRead();
  sgpRead();
  bmeRead();

  Serial.println();  
  delay(50);
}

void bmeRead(){
  // bme check valid
  if (! bme.performReading()) {
		return;
	} else {
    // bme oplight on
    blinkPin(BMEPIN, 1);

    bmeCurrent.temp = bme.temperature;
    bmeCurrent.pres = bme.pressure / 100.0;
    bmeCurrent.humid = bme.humidity;
    bmeCurrent.gasr = bme.gas_resistance / 1000.0;
  }

  if((bmeCurrent.temp == bmeLast.temp) && (bmeCurrent.pres == bmeLast.pres) && (bmeCurrent.humid == bmeLast.humid) && (bmeCurrent.gasr == bmeLast.gasr)){
    // bme oplight off
    blinkPin(BMEPIN, 2);
    return; 
  }else{ 
    
    // bme print block
    Serial.println("[BME BLOCK]");
    Serial.print(bme.temperature); Serial.println("[degC]");
    Serial.print(bme.pressure / 100.0); Serial.println("[hPaPres]");
    Serial.print(bme.humidity); Serial.println("[%humid]");
    Serial.print(bme.gas_resistance / 1000.0); Serial.println("[KOhmsGasR]");
    Serial.println();

    bmeLast = bmeCurrent;
  }
  // bme oplight off
    blinkPin(BMEPIN, 2);
}

void scdRead(){
  //ping scd for valid data reading
  if (scd30.dataReady()) {
    if (!scd30.read()){ 
      return; 
    } else {
      // scd oplight on
      blinkPin(SCDPIN, 1);

      scdCurrent.temp = scd30.temperature;
      scdCurrent.humid = scd30.relative_humidity;
      scdCurrent.co2 = scd30.CO2;

      if((scdCurrent.temp == scdLast.temp)&&(scdCurrent.humid == scdLast.humid)&&(scdCurrent.co2 == scdLast.co2)){
        //sgp oplight off
        blinkPin(SCDPIN, 2);
        return;
      } else {
        // scd print block
        Serial.println("[SCD BLOCK]");
        Serial.print(scd30.temperature); Serial.println("[degC]");
        Serial.print(scd30.relative_humidity); Serial.println("[%humid]");
	      Serial.print(scd30.CO2, 3); Serial.println("[ppmCO2]");
        Serial.println();
        scdLast = scdCurrent;
      }
    //sgp oplight off
    blinkPin(SCDPIN, 2);
    }
  }  
}

void sgpRead(){
  // sgp data valid
  if (! sgp.IAQmeasure()) {
    return;
  } else {
    // sgp oplight on
    blinkPin(SGPPIN, 1);

    sgpCurrent.tvoc = sgp.TVOC;
    sgpCurrent.co2 = sgp.eCO2;
  }

  if((sgpCurrent.tvoc == sgpLast.tvoc)&&(sgpCurrent.tvoc == sgpLast.tvoc)){
    //sgp oplight off
    blinkPin(SGPPIN, 2);
    return;
  } else {
    // sgp print block  
    Serial.println("[SGP BLOCK]");
    Serial.print(sgp.TVOC); Serial.println("[ppbTVOC]");
    Serial.print(sgp.eCO2); Serial.println("[ppmCO2]");
    sgpLast = sgpCurrent;
  }
  //sgp oplight off
  blinkPin(SGPPIN, 2);
}

void fanControl(){
  fanMode++;
  if (fanMode > 3) {		
    fanMode = 0;
  }
 
  ledcWrite(FAN1_PIN, fanSpeed[fanMode]);
  ledcWrite(FAN2_PIN, fanSpeed[fanMode]);

  //Serial.print("Fan mode: ");
  Serial.print(fanMode);
  //Serial.print(" | PWM: ");
  Serial.println(fanSpeed[fanMode]);
}

void shadeToggle(){
	if((t_dataBegin == 0)&&(dumbTimestamps)){
    t_dataBegin = millis();
  } else { 
    trigger = true;
  }
}


void readButton(int pin, int work){
  reading[work] = digitalRead(pin);
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading[work] != stableButtonState[work]) {
      stableButtonState[work] = reading[work];

      if (stableButtonState[work] == LOW) {
        switch(pin){
          case(BUTTON2_PIN):
            fanControl();
            break;
          case(BUTTON1_PIN):
            //fanControl();
            break;
          default:
            Serial.println("undefined button input detected.");
            break;
        }
      } 
    }
  }

  lastButtonReading[work] = reading[work];	
}