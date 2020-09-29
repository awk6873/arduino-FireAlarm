int sensorAnalog = A0;
int sensorDigital = A1;


void setup() {
  pinMode(sensorAnalog, INPUT);
  pinMode(sensorDigital, INPUT_PULLUP);

  Serial.begin(57600);
}

void loop() {

  Serial.print(analogRead(sensorAnalog));
  Serial.print('\t');
  Serial.println(digitalRead(sensorDigital));

  delay(500);

}
