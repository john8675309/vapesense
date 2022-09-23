/*

*/

void setup() {
    Serial.begin(9600);
    pinMode(53, OUTPUT);
}

void loop() {
    int sensorValue=analogRead(A0);
    Serial.println(sensorValue);
    //float R0=(1023.0/sensorValue)-1;
    float R0=(sensorValue * 4.95 / 1023);
    if (sensorValue > 51 ) {
      digitalWrite(53, HIGH);
    } else {
      digitalWrite(53,LOW);
    }
    Serial.print("R0 = ");
    Serial.println(R0);
    delay(500);
}
