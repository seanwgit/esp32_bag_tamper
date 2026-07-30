#include <Adafruit_ADXL345_U.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>



// ================= SETTINGS =================

// Enables or disables the light sensor.
bool USE_LDR = true;
// Enables or disables the acceleromter sensor.
bool USE_ACCEL = true;

// Determines how the alarm is triggered.
// false = either sensor triggers alarm.
// true = both sensors must trigger alarm.
bool REQUIRE_BOTH = false;

// Light level required before the LDR detects a change.
int LDR_THRESHOLD = 2800;

// Minimum change in acceleration required to detect movement. (m/s²)
float MOVEMENT_THRESHOLD = 9;

// Pins for each component
#define LDR_PIN A0
#define BUZZER_PIN 13

// ============================================

// Create an ADXL345 sensor object with a unique sensor ID.
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Variables used to store the acceleration values when the system is armed.
// Future movement is compared against these values.
float armX, armY, armZ;

//===================start of paste====================================
/**
This code is pasted directly from previous example code we have had in class
where further ajustments will be made.
*/

//------------- WiFi setup --------------
//the Wifi status
//int status = WL_IDLE_STATUS;

//Your network name and password are hidden in the secrets.h file
const char SSID[] = SECRET_SSID;
const char PASSWORD[] = SECRET_PASS;

//------------- WebServer setup --------------
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");


/**
LittleFS setup - wants to be first method for some reason?
*/
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS failed to setup");
  } else {
    Serial.println("LittleFS should be going now");
  }
}

/**
LittleFS reading a file 
*/
String readFile(fs::FS& fs, const char* path) {
  Serial.print("Reading file: ");
  Serial.println(path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("readFile failed to open file");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

/**
LittleFS writing a file
*/
void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.print("Writing file: ");
  Serial.println(path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("writeFile failed to write to file");
    return;
  }
  if (file.print(message)) {
    Serial.println("- writeFile success");
  } else {
    Serial.println("- writeFile failed");
  }
}

/**
set up a JSON array and hold the states of all values we care about
pins and their current states variables and their current values
*/
String getOutputStates() {
  JSONVar myArray;

  myArray["values"][0]["pinNum"] = String(2);
  myArray["values"][0]["state"] = String(digitalRead(2));

  myArray["values"][1]["pinNum"] = String(13);
  myArray["values"][1]["state"] = String(digitalRead(13));

  myArray["values"][2]["r"] = String(red);
  myArray["values"][2]["g"] = String(green);
  myArray["values"][2]["b"] = String(blue);

  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

/**
Sends a message to all clients connected to ip address with a JSON message
*/
void notifyClients(String state) {
  ws.textAll(state);
}

/**
Handles messages from clients in the if statements below
*/
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    //if message is states send the state of all gpio pins
    if (strcmp((char*)data, "getStates") == 0) {
      notifyClients(getOutputStates());
    } else {
      //(char*)data should be in this format "rgb(255,87,51)" when it isn't 'getStates'
      std::string rgb = (char*)data;
      byte firstComma = rgb.find(',');
      byte secondComma = rgb.find(',', firstComma + 1);
      byte lastBracket = rgb.find(')');

      red = stoi(rgb.substr(4, firstComma));
      green = stoi(rgb.substr(firstComma + 1, secondComma - firstComma - 1));
      blue = stoi(rgb.substr(secondComma + 1, lastBracket));
    }
    //Just to alternate the led on and off to show we can digitalRead and digitalWrite here
    //digitalWrite(LEDPIN, !digitalRead(LEDPIN)); //AM I AN ISSUE NEED TO TEST--------------------------------------
  }
}

/**
Event listener handles different asyncronous steps from the web socket
*/
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      //Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.print(" connected from ");
      Serial.println(client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      //Serial.printf("WebSocket client #%u disconnected\n", client->id());
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.println(" disconnected\n");
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


/**
WebSocket setup
What to do when an event happens on the ws socket and adding a handler to the server
*/
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  Serial.println("websocket should be running now");
}

//==================End of Paste=============================


void setup() {
  // Start serial communication for debugging.
  Serial.begin(115200);
  // Wait for the serial monitor to connect.
  delay(2000);

  // Configure the LDR as an input and the buzzer as an output.
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start I2C communication with connected sensors.
  Wire.begin();

  // Check that the ADXL345 is connected before continuing.
  if (!accel.begin()) {
    Serial.println("ADXL345 NOT FOUND");
    while (1);
  }
  
  // Stop the program if the sensor cannot be found.
  accel.setRange(ADXL345_RANGE_2_G);

  // Set the accelerometer to its most sensitive measurement range.
  sensors_event_t event;
  accel.getEvent(&event);

  // Store the starting orientation of the bag.
  armX = event.acceleration.x;
  armY = event.acceleration.y;
  armZ = event.acceleration.z;

  Serial.println("SYSTEM ARMED");
}

void loop() {

  // Keep track of whether each sensor has detected tampering.
  bool ldrTriggered = false;
  bool accelTriggered = false;

  // LDR 

  // Read the current light level from the LDR.
  int ldr = analogRead(LDR_PIN);

  // If the LDR is enabled and the threshold is exceeded,
  // mark the sensor as triggered.
  if (USE_LDR && ldr > LDR_THRESHOLD) {
    ldrTriggered = true;

    Serial.print("LDR TRIGGERED: ");
    Serial.println(ldr);
  }

  // ADXL345 
  // Read the latest acceleration values.
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate the difference between the current position
  // and the original armed position on each axis.
  float dx = abs(event.acceleration.x - armX);
  float dy = abs(event.acceleration.y - armY);
  float dz = abs(event.acceleration.z - armZ);

  // Use the largest change across all three axes as the movement value.
  float movement = max(dx, max(dy, dz));

  // If movement exceeds the threshold, trigger the accelerometer.
  if (USE_ACCEL && movement > MOVEMENT_THRESHOLD) {
    accelTriggered = true;

    Serial.print("MOVEMENT TRIGGERED: ");
    Serial.println(movement);
  }

  // ALARM LOGIC 

  // Determine whether the alarm should sound.
  bool alarm = false;

  // If REQUIRE_BOTH is true, every enabled sensor must trigger.
  if (REQUIRE_BOTH) {

    alarm =
      (!USE_LDR || ldrTriggered) &&
      (!USE_ACCEL || accelTriggered);

  } else {
    
    // Else, any enabled sensor can trigger the alarm.
    alarm =
      (USE_LDR && ldrTriggered) ||
      (USE_ACCEL && accelTriggered);
  }

  // BUZZER 
  // Sound the buzzer if tampering has been detected.
  if (alarm) {

    Serial.println("!!! BAG TAMPER DETECTED !!!");

    // Generate a 2000 Hz tone.
    tone(BUZZER_PIN, 2000);

    // Keep the tone on briefly.   
    delay(250);

    // Stop the buzzer.
    noTone(BUZZER_PIN);

    // Pause before checking the sensors again.
    delay(250);
  }

  // DEBUG 
  // Output sensor readings to the Serial Monitor for testing and debugging.
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

  // Small delay to reduce the sensor reading rate.  
  delay(100);
}