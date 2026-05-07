//Three modes to control fan speed
// Board: Adafruit ESP32 Feather / HUZZAH32


const int FAN_PIN = 26;
const int BUTTON_PIN = 13;

const int PWM_FREQ = 25000;
const int PWM_RESOLUTION = 8;

int fanMode = 0;

int fanSpeed[] = {
  0,
  90,
  160,
  255
};

int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  delay(2000);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  ledcAttach(FAN_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(FAN_PIN, fanSpeed[fanMode]);

  Serial.println("ESP32 Feather HUZZAH32 fan control started.");
  Serial.println("Mode 0: OFF");
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        fanMode++;

        if (fanMode > 3) {
          fanMode = 0;
        }

        ledcWrite(FAN_PIN, fanSpeed[fanMode]);

        Serial.print("Fan mode: ");
        Serial.print(fanMode);
        Serial.print(" | PWM: ");
        Serial.println(fanSpeed[fanMode]);
      }
    }
  }

  lastButtonReading = reading;
}
