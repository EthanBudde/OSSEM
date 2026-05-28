const int BUTTON_PIN = ;   // A12 / IO13 / GPIO13

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Button test started.");
  Serial.println("Not pressed should be HIGH.");
  Serial.println("Pressed should be LOW.");
}

void loop() {
  int buttonState = digitalRead(BUTTON_PIN);

  Serial.print("GPIO13 button state: ");
  Serial.println(buttonState);

  delay(300);
}