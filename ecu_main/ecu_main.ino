// ecu_main.ino
// PSU OSSEM Capstone Team: Hussein Ashkanani, Ethan Budde, David Mazhnikov, Roman Sulza, Clarker Ren
// Faculty Advisors: Dr. Joshua Méndez & Dr. Elliott Gall
//
// brief:
//  initializes ECU environment
//  sets up network connections, I2C connections to sensors, fan connections
//  regularly detects sensor values, updates if novel
//  drives interrupts for buttons to control data marking and fan speed
//  outputs sensor data serially as a stream 
// rev 1.1

// TOP
// libraries
// required libraries
#include "Arduino.h"            // base Arduino library
#include <WiFi.h>               // for WiFi shield or ESP32
#include <Wire.h>               // I2C comms library
#include <Adafruit_Sensor.h>    // arbitrary sensor driver library
#include <ezTime.h>             // WiFi timesync, for timestamps
#include <SPI.h>                // Serial peripheral library

// specific sensor libraries
#include <Adafruit_SCD30.h>     // SCD30 interfacing
#include <Adafruit_SGP30.h>     // SGP30 interfacing
#include <Adafruit_BME680.h>    // BME680 interfacing
//#include <Other_Sensor.h>     // additional sensor libraries go here...

// constants and variables
// sensor LED pin constants
// see ~/KiCad/Integrated_System for value coherence
#define BMEPIN 33
#define SCDPIN 27
#define SGPPIN 12

// barometric pressure constant
#define SEALEVELPRESSURE_HPA (1013.25)

// setup pre-run cycles constant
#define PRERUN_CYCLES 15

// loop delay value
#define LOOP_DELAY 50

// fan, button pin constants
// see ~/KiCad/Integrated_System for value coherence
const int FAN1_PIN = 26;
const int FAN2_PIN = 25;
const int BUTTON1_PIN = 13;
const int BUTTON2_PIN = 21;

// fan-speed PWM constants
const int PWM_FREQ = 25000;
const int PWM_RESOLUTION = 8;

// data highlighting boolean
bool trigger = false;

// fan, button state variables
int fanMode = 0;                // initializes fan to medium speed on startup
int reading[3];                 // +1 indexed 

// fan-speed presets
int fanSpeed[] = {              // 3 modes
  135,                          // medium speed - starting value
  0,                            // off
  255                           // full speed
};

// button debouncing state mechanism variables
int lastButtonReading[3];               // active button  
int stableButtonState[3] = {0, 1, 1};   // active low 0s for both buttons
unsigned long lastDebounceTime = 0;     // time since we last bounced
const unsigned long debounceDelay = 50; // delay (ms) ~= 2x expected bounce time

// sensor Objects
Adafruit_SCD30 scd30;
Adafruit_SGP30 sgp;
Adafruit_BME680 bme(&Wire);

// timezone Object
Timezone myTZ;

// network ssid and password
// credentials of hotspot, router connection, or external device's netword
// Rev2 - needs to be pulled from a file, this current design is "unsafe"
char ssid[]     = "SomeNetwork";        // your SSID goes here...
char password[] = "ThePasscode";        // your SSID's pass goes here...

// counter for fallback timestamping check
int count = 0;
int t_dataBegin = 0;

// fallback timestamping enable/disable
bool dumbTimestamps = false;            // disabled by default

// coherence structs, last/current holders
// BME values struct
struct bmeValues{
  float temp;
  float pres;
  float humid;
  float gasr;
};

// BME current, last state holders
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

/*
struct newSensorValues{
    int value1;
    float value2;
    long float value3;
};
newSensorValues newCurrent, newLast;
*/

// worker functions
// getPinMode
//   checks pin is valid port, returns pin type
uint32_t getPinMode(uint32_t pin) {
  uint32_t bit = digitalPinToBitMask(pin);      // absolute bitvalue
  uint32_t port = digitalPinToPort(pin);        // port mask

  // if this isn't a pin, we don't care about it
  if (port == NOT_A_PIN) return 0xFF;

  volatile uint32_t *reg = portModeRegister(port);
  volatile uint32_t *out = portOutputRegister(port);

  if (*reg & bit) {
    return OUTPUT;                  // pin is configured as an output
  } else {
    return (*out & bit) ? INPUT_PULLUP : INPUT; // checks if pull-up is active
  }
}

// blinkpin
  //checks pin valid output
  //blinks selected pattern at selected pin
void blinkPin(int pin, int pattern){
  if(getPinMode(pin) != OUTPUT) {
    // todo - add error messaging, logging
    return;
  }
  // blinking patterns for sensor leds
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

// setup
void setup() {
  // serial comms
  Serial.begin(115200);
  
  // sensor operation LED enabling
  pinMode(BMEPIN, OUTPUT);
  pinMode(SCDPIN, OUTPUT);
  pinMode(SGPPIN, OUTPUT);

  // pushbutton enabling
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  // fan1, fan2 value setting, enabling
  ledcAttach(FAN1_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN1_PIN, fanSpeed[fanMode]);

  ledcAttach(FAN2_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN2_PIN, fanSpeed[fanMode]);

  // deprecated - test messaging  
  //Serial.println("ESP32 Feather HUZZAH32 fan control started.");
  //Serial.println("Mode 0: OFF");
  
  // hold sensor pins high for Wi-Fi connection period
  blinkPin(BMEPIN, 1);
  blinkPin(SCDPIN, 1);
  blinkPin(SGPPIN, 1);

  //while (!Serial) delay(10);	// will pause until serial terminal open
	
  // wifi signal chain enable
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
	  Serial.println("Connecting ...");
    
    // 30s timeout counter    
    if(count == 120){
      Serial.println("cannot connect to wifi, no timestamps");
      break;
    }
    count++;
	delay(250);
  }
	
  // if WiFi connected, print basic network details to serial log
  if(WiFi.status() == WL_CONNECTED){
    count = 0;
    Serial.print("connected to network -> ");
    Serial.println(ssid);  
    waitForSync();
    myTZ.setLocation(F("America/Los_Angeles"));
  }
  
  // disable LEDs, network connection period over
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
  for(int i=0;i<PRERUN_CYCLES;i++){
      scdCurrent.temp = scd30.temperature;
      scdCurrent.humid = scd30.relative_humidity;
      scdCurrent.co2 = scd30.CO2;
  }
}

// loop
void loop() {
  // button check
  // see button routines below
  readButton(BUTTON1_PIN, 1);
  readButton(BUTTON2_PIN, 2);
  
  // timestamp marking to serial stream
  if(trigger == true) {
	  Serial.print("!");
    trigger = false;
  }

  // timestamping
  if(dumbTimestamps == false){          // smart, Wi-Fi timestamps
	  Serial.println(myTZ.dateTime("H:i:s.v"));
	}else if(dumbTimestamps == true){   // dumb, time-since-startup timestamps
    char buffer[10];
    int time = (millis() - t_dataBegin) / 1000; 
    sprintf(buffer, "%03d", ((millis() - t_dataBegin) % 1000));
    Serial.print(time); Serial.print("."); Serial.println(buffer);
  }
 

  // refactor for controlability, testing
  // try/except with a nonblocking timer, scheduling
  scdRead();
  sgpRead();
  bmeRead();

  // small wait, loop
  Serial.println();  
  delay(LOOP_DELAY);
}

// bmeRead
// BME688 value requestor, validator 
void bmeRead(){
  // bme check valid
  if (! bme.performReading()) {
		return;
	} else {
    
    // bme oplight on
    blinkPin(BMEPIN, 1);

    // grab values to current holder
    bmeCurrent.temp = bme.temperature;
    bmeCurrent.pres = bme.pressure / 100.0;
    bmeCurrent.humid = bme.humidity;
    bmeCurrent.gasr = bme.gas_resistance / 1000.0;
  }
  
  // if all values same, sensor has no new values, return
  if((bmeCurrent.temp == bmeLast.temp) && (bmeCurrent.pres == bmeLast.pres) && (bmeCurrent.humid == bmeLast.humid) && (bmeCurrent.gasr == bmeLast.gasr)){
    // bme oplight off
    blinkPin(BMEPIN, 2);
    return;

  // otherwise, we have a new value
  }else{ 
    // bme print block
    Serial.println("[BME BLOCK]");
    Serial.print(bme.temperature); Serial.println("[degC]");
    Serial.print(bme.pressure / 100.0); Serial.println("[hPaPres]");
    Serial.print(bme.humidity); Serial.println("[%humid]");
    Serial.print(bme.gas_resistance / 1000.0); Serial.println("[KOhmsGasR]");
    Serial.println();

    // update state values
    bmeLast = bmeCurrent;
  }
  // bme oplight off
  blinkPin(BMEPIN, 2);
}

// scdRead
// SCD30 value requestor, validator
void scdRead(){
  //ping scd for valid data reading
  if (scd30.dataReady()) {
    if (!scd30.read()){ 
      return; 
    } else {
      // scd oplight on
      blinkPin(SCDPIN, 1);
      
      // values to current state holder
      scdCurrent.temp = scd30.temperature;
      scdCurrent.humid = scd30.relative_humidity;
      scdCurrent.co2 = scd30.CO2;

      // if none are new, sensor has no new data, return
      if((scdCurrent.temp == scdLast.temp)&&(scdCurrent.humid == scdLast.humid)&&(scdCurrent.co2 == scdLast.co2)){
        //sgp oplight off
        blinkPin(SCDPIN, 2);
        return;
      
      // new data case
      } else {
        // scd print block
        Serial.println("[SCD BLOCK]");
        Serial.print(scd30.temperature); Serial.println("[degC]");
        Serial.print(scd30.relative_humidity); Serial.println("[%humid]");
	    Serial.print(scd30.CO2, 3); Serial.println("[ppmCO2]");
        Serial.println();

        //update last state holder
        scdLast = scdCurrent;
      }
    //sgp oplight off
    blinkPin(SCDPIN, 2);
    }
  }  
}

// sgpRead
// SGP30 value requester, validator
void sgpRead(){
  // sgp data valid?
  if (! sgp.IAQmeasure()) {
    return;
  } else {
    // sgp oplight on
    blinkPin(SGPPIN, 1);

    // values to current state holder
    sgpCurrent.tvoc = sgp.TVOC;
    sgpCurrent.co2 = sgp.eCO2;
  }

  // if current == last, we have no new values, return
  if((sgpCurrent.tvoc == sgpLast.tvoc)&&(sgpCurrent.tvoc == sgpLast.tvoc)){
    //sgp oplight off
    blinkPin(SGPPIN, 2);
    return;

  // new value case  
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

// fanControl
// cycles fan speed mode on call
void fanControl(){
  // increment fanMode
  fanMode++;

  // wrap if value > 2 
  if (fanMode > 2) {		
    fanMode = 0;
  }

  // update fan states
  ledcWrite(FAN1_PIN, fanSpeed[fanMode]);
  ledcWrite(FAN2_PIN, fanSpeed[fanMode]);
}

// shadeToggle
// marks current timestamp with a '!' prefix for shading graph
void shadeToggle(){
  trigger = true;
}

// button reading service
void readButton(int pin, int button){
  // current button value   
  reading[button] = digitalRead(pin);
  
  // check debounce period
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading[button] != stableButtonState[button]) {
      // debounce period over, button stable
      stableButtonState[button] = reading[button];

      // stable press switch
      if (stableButtonState[button] == LOW) {
        switch(pin){
          case(BUTTON2_PIN):
            shadeToggle();          // button1 routine
            break;
          case(BUTTON1_PIN):        // button2 routine
            fanControl();
            break;
          default:
            Serial.println("undefined button input detected.");
            break;
        }
      } 
    }
  }
  // update button state variables
  lastButtonReading[button] = reading[button];	
}