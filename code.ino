#include <Adafruit_ADXL345_U.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// SETTINGS

// Enable sensors
bool USE_LDR = true;
bool USE_ACCEL = true;

// false = either sensor triggers alarm
// true = both sensors must trigger alarm
bool REQUIRE_BOTH = false;

// LDR threshold
int LDR_THRESHOLD = 2800;

// Movement threshold (m/s²)
float MOVEMENT_THRESHOLD = 9;

// Pins
#define LDR_PIN A0
#define BUZZER_PIN 13

//CODE

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Armed position
float armX, armY, armZ;

void setup() {

  Serial.begin(115200);
  delay(2000);

  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin();

  if (!accel.begin()) {
    Serial.println("ADXL345 NOT FOUND");
    while (1);
  }

  accel.setRange(ADXL345_RANGE_2_G);

  // Store starting position
  sensors_event_t event;
  accel.getEvent(&event);

  armX = event.acceleration.x;
  armY = event.acceleration.y;
  armZ = event.acceleration.z;

  Serial.println("SYSTEM ARMED");
}

void loop() {

  bool ldrTriggered = false;
  bool accelTriggered = false;

  // LDR 

  int ldr = analogRead(LDR_PIN);

  if (USE_LDR && ldr > LDR_THRESHOLD) {
    ldrTriggered = true;

    Serial.print("LDR TRIGGERED: ");
    Serial.println(ldr);
  }

  // ADXL345 

  sensors_event_t event;
  accel.getEvent(&event);

  float dx = abs(event.acceleration.x - armX);
  float dy = abs(event.acceleration.y - armY);
  float dz = abs(event.acceleration.z - armZ);

  float movement = max(dx, max(dy, dz));

  if (USE_ACCEL && movement > MOVEMENT_THRESHOLD) {
    accelTriggered = true;

    Serial.print("MOVEMENT TRIGGERED: ");
    Serial.println(movement);
  }

  // ALARM LOGIC 

  bool alarm = false;

  if (REQUIRE_BOTH) {

    alarm =
      (!USE_LDR || ldrTriggered) &&
      (!USE_ACCEL || accelTriggered);

  } else {

    alarm =
      (USE_LDR && ldrTriggered) ||
      (USE_ACCEL && accelTriggered);
  }

  // BUZZER 

  if (alarm) {

    Serial.println("!!! BAG TAMPER DETECTED !!!");

    tone(BUZZER_PIN, 2000);
    delay(250);
    noTone(BUZZER_PIN);

    delay(250);
  }

  // DEBUG 

  Serial.print("LDR=");
  Serial.print(ldr);

  Serial.print(" | Move=");
  Serial.print(movement);

  Serial.print(" | X=");
  Serial.print(event.acceleration.x);

  Serial.print(" Y=");
  Serial.print(event.acceleration.y);

  Serial.print(" Z=");
  Serial.println(event.acceleration.z);

  delay(100);
}