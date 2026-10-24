#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <esp_arduino_version.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

/*
  These settings control how the bag sensors and alarm work.
  The webpage can now change them, instead of only changing the button text.
*/

// Sensor settings
bool USE_LDR = true;
bool USE_ACCEL = true;
bool ALARM_ON = true;

// false = either sensor, true = both sensors
bool REQUIRE_BOTH = false;

// Choose which sensors are allowed to trigger the alarm
bool MOVEMENT_TRIGGER = true;
bool LIGHT_TRIGGER = true;

// Sensitivity values are percentages shown on the webpage
int movementSensitivity = 50;
int lightSensitivity = 50;
int alarmVolume = 50;

// Light and movement thresholds
int LDR_THRESHOLD = 2800;
float MOVEMENT_THRESHOLD = 9;

// Pins for the LDR and buzzer
#define LDR_PIN A0
#define BUZZER_PIN 13

// Set up the small screen on the ESP32
Adafruit_ST7789 tftScreen = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Set up the accelerometer
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Starting acceleration values used to detect movement
float armX, armY, armZ;

// Values sent to the webpage so it can show the actual bag status
bool tamperDetected = false;

// Remember which sensors are currently triggered
bool ldrTriggered = false;
bool accelTriggered = false;

int currentLDR = 0;
float currentMovement = 0;
unsigned long lastSensorCheck = 0;
unsigned long lastWebUpdate = 0;
unsigned long lastBuzzerChange = 0;
bool buzzerPlaying = false;

/*
  This is the WiFi, LittleFS and WebSocket code adapted from the class example.
  The ESP32 hosts the webpage and exchanges messages with JavaScript.
*/

// WiFi network details
const char SSID[] = "Winwifi";
const char PASSWORD[] = "YOUR_WIFI_PASSWORD";

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

/*
  This makes one JSON message containing the current ESP32 settings and readings.
  It uses the same values array idea as the teacher's example, so the webpage
  can read each value in the onMessage() function.
*/

// Make a JSON message containing the current sensor settings
String getOutputStates() {
  JSONVar myArray;

  myArray["values"][0]["movement"] = USE_ACCEL ? "1" : "0";
  myArray["values"][1]["light"] = USE_LDR ? "1" : "0";
  myArray["values"][2]["alarm"] = ALARM_ON ? "1" : "0";
  myArray["values"][3]["movementSensitivity"] = movementSensitivity;
  myArray["values"][4]["lightSensitivity"] = lightSensitivity;
  myArray["values"][5]["volume"] = alarmVolume;
  myArray["values"][6]["movementTrigger"] = MOVEMENT_TRIGGER ? "1" : "0";
  myArray["values"][7]["lightTrigger"] = LIGHT_TRIGGER ? "1" : "0";
  myArray["values"][8]["tamper"] = tamperDetected ? "1" : "0";
  myArray["values"][9]["ldr"] = currentLDR;
  myArray["values"][10]["movementValue"] = currentMovement;

  return JSON.stringify(myArray);
}

// Send a message to all connected webpages
void notifyClients(String state) {
  ws.textAll(state);
}

/*
  The webpage sends simple text commands, such as movementOff or volume:50.
  This function reads each message, changes the right ESP32 variable, and sends
  the new settings back so the controls stay in sync after a refresh.
*/

// Read messages sent from the webpage
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 &&
      info->len == len && info->opcode == WS_TEXT) {

    // Read the message received from the webpage
    String message = "";

    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    Serial.println(message);

    // Send the current sensor settings when requested
    if (message == "getStates") {
      notifyClients(getOutputStates());
      return;
    }

    // Turn the movement sensor on or off
    if (message == "movementOn") {
      USE_ACCEL = true;
    }
    else if (message == "movementOff") {
      USE_ACCEL = false;
    }

    // Turn the light sensor on or off
    else if (message == "lightOn") {
      USE_LDR = true;
    }
    else if (message == "lightOff") {
      USE_LDR = false;
    }

    // Turn the alarm on or off
    else if (message == "alarmOn") {
      ALARM_ON = true;
    }
    else if (message == "alarmOff") {
      ALARM_ON = false;
    }

    // Choose which sensors can trigger the alarm
    else if (message == "movementTriggerOn") {
      MOVEMENT_TRIGGER = true;
    }
    else if (message == "movementTriggerOff") {
      MOVEMENT_TRIGGER = false;
    }
    else if (message == "lightTriggerOn") {
      LIGHT_TRIGGER = true;
    }
    else if (message == "lightTriggerOff") {
      LIGHT_TRIGGER = false;
    }

    // Read sensitivity and volume numbers from the sliders
    else if (message.startsWith("movementSensitivity:")) {
      movementSensitivity =
        constrain(message.substring(20).toInt(), 0, 100);
    }
    else if (message.startsWith("lightSensitivity:")) {
      lightSensitivity =
        constrain(message.substring(17).toInt(), 0, 100);
    }
    else if (message.startsWith("volume:")) {
      alarmVolume =
        constrain(message.substring(7).toInt(), 0, 100);
    }
    else {
      return;
    }

    // Higher sensitivity means a smaller change can trigger detection
    MOVEMENT_THRESHOLD =
      16.0 - (14.0 * movementSensitivity / 100.0);

    LDR_THRESHOLD =
      map(lightSensitivity, 0, 100, 4000, 1600);

    notifyClients(getOutputStates());
  }
}

// Handles WebSocket connections, disconnections and messages
void onEvent(AsyncWebSocket* server,
             AsyncWebSocketClient* client,
             AwsEventType type,
             void* arg,
             uint8_t* data,
             size_t len) {

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

/*
  The buzzer uses PWM to try to change its loudness. This works for my
  passive piezo buzzer; an active buzzer may only sound on or off. The code
  supports both older and newer ESP32 Arduino board packages.
*/

void setupBuzzer() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWrite(BUZZER_PIN, 0);
#else
  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER_PIN, 0);
  ledcWrite(0, 0);
#endif
}

void setBuzzer(bool playSound) {
  // Higher slider values use a larger PWM duty cycle
  int duty = playSound ?
    map(alarmVolume, 0, 100, 0, 128) : 0;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BUZZER_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

/*
  Setup connects the ESP32 to WiFi, loads the webpage files, and starts the
  WebSocket server. It then checks the accelerometer and remembers the bag's
  starting position so it can detect movement later. I also added a screen 
  that prints the ip adress based on the teacher example.
*/

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

  // Turn on the small TFT screen
  pinMode(TFT_BACKLITE, OUTPUT);
  pinMode(TFT_I2C_POWER, OUTPUT);

  digitalWrite(TFT_BACKLITE, HIGH);
  digitalWrite(TFT_I2C_POWER, HIGH);

  // Set up the screen using the class example
  tftScreen.init(135, 240);
  tftScreen.setRotation(3);
  tftScreen.fillScreen(ST77XX_WHITE);

  // Set the text colour and size
  tftScreen.setTextColor(ST77XX_BLACK);
  tftScreen.setTextSize(2);
  tftScreen.setTextWrap(true);

  // Show the IP address needed to open the webpage
  tftScreen.setCursor(10, 20);
  tftScreen.println("Bag Detection");

  tftScreen.setCursor(10, 60);
  tftScreen.println("IP Address:");

  tftScreen.setCursor(10, 90);
  tftScreen.println(WiFi.localIP());

  Serial.println("Screen should now show the IP address");

  // Start LittleFS and WebSocket
  initLittleFS();
  initWebSocket();

  // Load the webpage files stored on the ESP32
  server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

  // Start the web server
  server.begin();

  Serial.println("Web server started");

  // Set up the LDR and buzzer
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setupBuzzer();

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

/*
  Loop checks the light and movement sensors and decides whether the bag has
  been disturbed. It also updates the buzzer and sends readings to the webpage.
  millis() is used instead of long delays so the webpage stays responsive.
*/

void loop() {
  // Remove disconnected WebSocket clients
  ws.cleanupClients();

  unsigned long now = millis();

  // Read the sensors about every 100 milliseconds
  if (now - lastSensorCheck >= 100) {
    lastSensorCheck = now;

    // Track whether each sensor has detected tampering
    // Reset the sensor results before checking them again
    ldrTriggered = false;
    accelTriggered = false;

    // Read the light sensor
    int ldr = analogRead(LDR_PIN);
    currentLDR = ldr;

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
    currentMovement = movement;

    // Check whether enough movement has happened
    if (USE_ACCEL && movement > MOVEMENT_THRESHOLD) {
      accelTriggered = true;

      Serial.print("MOVEMENT TRIGGERED: ");
      Serial.println(movement);
    }

    /*
      The sensor buttons control whether tampering can be detected.
      The alarm checkboxes are separate and only control the buzzer.
    */

    // Decide whether the bag has been tampered with
    if (REQUIRE_BOTH) {
      tamperDetected =
        USE_ACCEL && USE_LDR &&
        accelTriggered && ldrTriggered;
    } else {
      tamperDetected = accelTriggered || ldrTriggered;
    }
  }

  /*
    The alarm plays a short beep, then pauses before the next beep.
    If the alarm is off or the bag is secure, the buzzer stops immediately.
  */

  // Check whether a selected sensor should activate the buzzer
  bool alarmTriggered =
    (MOVEMENT_TRIGGER && accelTriggered) ||
    (LIGHT_TRIGGER && ldrTriggered);

  // Sound the buzzer only when the alarm is enabled
  bool shouldSound =
    alarmTriggered && ALARM_ON && alarmVolume > 0;

  if (!shouldSound) {
    buzzerPlaying = false;
    setBuzzer(false);
    lastBuzzerChange = now;
  }
  else if (now - lastBuzzerChange >= 250) {
    buzzerPlaying = !buzzerPlaying;
    setBuzzer(buzzerPlaying);
    lastBuzzerChange = now;

    if (buzzerPlaying) {
      Serial.println("!!! BAG TAMPER DETECTED !!!");
    }
  }

  /*
    The ESP32 regularly sends sensor readings and the tamper result to the
    webpage. This is how Bag Secure changes to Tampering Detected without
    needing to refresh the browser.
  */

  // Send live states to the webpage twice per second
  if (now - lastWebUpdate >= 500) {
    lastWebUpdate = now;

    if (ws.count() > 0) {
      notifyClients(getOutputStates());
    }

    // Print sensor readings for testing
    Serial.print("LDR=");
    Serial.print(currentLDR);

    Serial.print(" | Move=");
    Serial.println(currentMovement);
  }
}
