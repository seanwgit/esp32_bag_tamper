#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// Sensor settings
bool USE_LDR = true;
bool USE_ACCEL = true;

// false = either sensor, true = both sensors
bool REQUIRE_BOTH = false;

// Light and movement thresholds
int LDR_THRESHOLD = 2800;
float MOVEMENT_THRESHOLD = 9;

// Pins for the LDR and buzzer
#define LDR_PIN A0
#define BUZZER_PIN 13

// Set up the accelerometer
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Starting acceleration values used to detect movement
float armX, armY, armZ;


// WebSocket code adapted from the class example

// WiFi network details
const char SSID[] = "Winwifi";
const char PASSWORD[] = "giNk0b!lBO4";

// Create the web server on port 80
AsyncWebServer server(80);

// Create the WebSocket
AsyncWebSocket ws("/ws");

// Start LittleFS so the ESP32 can read the webpage files
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS failed to setup");
  } else {
    Serial.println("LittleFS should be going now");
  }
}

// Read a file from LittleFS
void readFile(fs::FS& fs, const char* path) {
  Serial.print("Reading file: ");
  Serial.println(path);

  File file = fs.open(path);

  if (!file || file.isDirectory()) {
    Serial.println("readFile failed to open file");
    return;
  }

  while (file.available()) {
    Serial.write(file.read());
  }

  file.close();
}

// Write text to a file in LittleFS
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

  file.close();
}

// Make a JSON message to test communication with the webpage
String getOutputStates() {
  JSONVar myArray;

  // This is just a test value for now
  myArray["values"][0]["status"] = "connected";

  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

// Send a message to all connected webpages
void notifyClients(String state) {
  ws.textAll(state);
}

// Read messages sent from the webpage
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

    // Read the message received from the webpage
    String message = "";

    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    Serial.println(message);

    // If the webpage requests the states, send a test message
    if (message == "getStates") {
      notifyClients(getOutputStates());
    }
  }
}

// Handles WebSocket connections, disconnections and messages
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.print(" connected from ");
      Serial.println(client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.println(" disconnected");
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;

    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Start the WebSocket
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  Serial.println("WebSocket should be running now");
}


void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);
  delay(2000);

  // Connect the ESP32 to WiFi
  WiFi.begin(SSID, PASSWORD);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");

  // Print the IP address used to open the webpage
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Start LittleFS and WebSocket
  initLittleFS();
  initWebSocket();

  // Load the webpage files stored on the ESP32
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Start the web server
  server.begin();

  Serial.println("Web server started");

  // Set up the LDR and buzzer
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start communication with the accelerometer
  Wire.begin();

  // Check whether the ADXL345 is connected
  if (!accel.begin()) {
    Serial.println("ADXL345 NOT FOUND");
    while (1);
  }

  // Set the accelerometer range
  accel.setRange(ADXL345_RANGE_2_G);

  sensors_event_t event;
  accel.getEvent(&event);

  // Save the starting position of the bag
  armX = event.acceleration.x;
  armY = event.acceleration.y;
  armZ = event.acceleration.z;

  Serial.println("SYSTEM ARMED");
}


void loop() {

  // Remove disconnected WebSocket clients
  ws.cleanupClients();

  // Track whether each sensor has detected tampering
  bool ldrTriggered = false;
  bool accelTriggered = false;

  // Read the light sensor
  int ldr = analogRead(LDR_PIN);

  // Check whether the light level has passed the threshold
  if (USE_LDR && ldr > LDR_THRESHOLD) {
    ldrTriggered = true;

    Serial.print("LDR TRIGGERED: ");
    Serial.println(ldr);
  }

  // Read the accelerometer
  sensors_event_t event;
  accel.getEvent(&event);

  // Calculate the change from the starting position
  float dx = abs(event.acceleration.x - armX);
  float dy = abs(event.acceleration.y - armY);
  float dz = abs(event.acceleration.z - armZ);

  // Use the largest change to measure movement
  float movement = max(dx, max(dy, dz));

  // Check whether enough movement has happened
  if (USE_ACCEL && movement > MOVEMENT_THRESHOLD) {
    accelTriggered = true;

    Serial.print("MOVEMENT TRIGGERED: ");
    Serial.println(movement);
  }

  // Decide whether the alarm should go off
  bool alarm = false;

  if (REQUIRE_BOTH) {

    // Both enabled sensors must trigger
    alarm =
      (!USE_LDR || ldrTriggered) &&
      (!USE_ACCEL || accelTriggered);

  } else {

    // Either enabled sensor can trigger
    alarm =
      (USE_LDR && ldrTriggered) ||
      (USE_ACCEL && accelTriggered);
  }

  // Sound the buzzer if tampering is detected
  if (alarm) {

    Serial.println("!!! BAG TAMPER DETECTED !!!");

    tone(BUZZER_PIN, 2000);
    delay(250);

    noTone(BUZZER_PIN);
    delay(250);
  }

  // Print sensor readings for testing
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
