/*
  This section connects the webpage to the ESP32 using WebSocket.
  It follows the class example and asks the ESP32 for its settings when
  the connection first opens.
*/

// Set up the address used to connect to the ESP32
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

// Remember when the user is moving a slider so live updates do not interrupt it
var movingMovementSlider = false;
var movingLightSlider = false;
var movingVolumeSlider = false;

// Wait for the webpage to load before connecting
window.addEventListener("load", onload);

// Start the WebSocket connection when the page loads
function onload(event) {
    initWebSocket();
}

// Set up the WebSocket connection
function initWebSocket() {
    console.log("Trying to open a WebSocket connection...");

    websocket = new WebSocket(gateway);

    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When connected, update the message and ask the ESP32 for its states
function onOpen(event) {
    console.log("WebSocket opened");

    document.getElementById("connectionStatus").innerHTML =
        "ESP connected";

    // Ask the ESP32 to send its current states
    getStates();
}

// If disconnected, try connecting again after 2 seconds
function onClose(event) {
    console.log("WebSocket closed");

    document.getElementById("connectionStatus").innerHTML =
        "ESP disconnected";

    setTimeout(initWebSocket, 2000);
}

/*
  Every message from the ESP32 contains the current sensor settings and
  readings in JSON. The webpage uses this information to keep all buttons,
  sliders and the bag status matching the actual ESP32 settings.
*/

// Read the JSON message sent from the ESP32
function onMessage(event) {
    var myObj = JSON.parse(event.data);

    // Print the received message for testing
    console.log(myObj);

    var values = myObj.values;

    // Read the sensor settings sent from the ESP32
    movementOn = values[0].movement == "1";
    lightOn = values[1].light == "1";
    alarmOn = values[2].alarm == "1";

    // Update the movement button using the ESP32 setting
    document.getElementById("movementButton").innerHTML =
        movementOn ? "ON" : "OFF";

    // Update the light button using the ESP32 setting
    document.getElementById("lightButton").innerHTML =
        lightOn ? "ON" : "OFF";

    document.getElementById("alarmButton").innerHTML =
        alarmOn ? "ON" : "OFF";

    // Show the saved sensitivity and volume values
    if (!movingMovementSlider) {
        document.getElementById("movementSensitivity").value =
            values[3].movementSensitivity;

        document.getElementById("movementValue").innerHTML =
            values[3].movementSensitivity;
    }

    if (!movingLightSlider) {
        document.getElementById("lightSensitivity").value =
            values[4].lightSensitivity;

        document.getElementById("lightValue").innerHTML =
            values[4].lightSensitivity;
    }

    if (!movingVolumeSlider) {
        document.getElementById("volumeSlider").value =
            values[5].volume;

        document.getElementById("volumeValue").innerHTML =
            values[5].volume;
    }

    // Show which sensors are selected as alarm triggers
    document.getElementById("movementTrigger").checked =
        values[6].movementTrigger == "1";

    document.getElementById("lightTrigger").checked =
        values[7].lightTrigger == "1";

    // Change the bag status when either selected sensor detects tampering
    var bagStatus = document.getElementById("bagStatus");

    if (values[8].tamper == "1") {
        bagStatus.innerHTML = "Tampering Detected";
        bagStatus.classList.add("warning");
    } else {
        bagStatus.innerHTML = "Bag Secure";
        bagStatus.classList.remove("warning");
    }

    updateAlarmStatus();

    document.getElementById("lightReading").innerHTML =
        values[9].ldr;

    document.getElementById("movementReading").innerHTML =
        Number(values[10].movementValue).toFixed(1);
}

/*
  The controls below send simple text commands to the ESP32.
  Instead of only changing the webpage, the ESP32 changes the real setting
  and sends a message back to confirm it.
*/

// Only send a message when the ESP32 is connected
function sendMessage(message) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(message);
    } else {
        console.log("ESP32 not connected");
    }
}

var movementOn = true;

// Change the movement sensor button between ON and OFF
function toggleMovement() {
    sendMessage(movementOn ? "movementOff" : "movementOn");
}

// Show the selected movement sensitivity
function updateMovementSensitivity(element) {
    movingMovementSlider = true;

    var value =
        document.getElementById("movementSensitivity").value;

    document.getElementById("movementValue").innerHTML = value;

    console.log("Movement sensitivity:", value);
}

// Send the slider value when the user finishes moving it
function saveMovementSensitivity() {
    sendMessage(
        "movementSensitivity:" +
        document.getElementById("movementSensitivity").value
    );

    movingMovementSlider = false;
}

var lightOn = true;

// Change the light sensor button between ON and OFF
function toggleLight() {
    sendMessage(lightOn ? "lightOff" : "lightOn");
}

// Show the selected light sensitivity
function updateLightSensitivity(element) {
    movingLightSlider = true;

    var value =
        document.getElementById("lightSensitivity").value;

    document.getElementById("lightValue").innerHTML = value;

    console.log("Light sensitivity:", value);
}

// Send the slider value when the user finishes moving it
function saveLightSensitivity() {
    sendMessage(
        "lightSensitivity:" +
        document.getElementById("lightSensitivity").value
    );

    movingLightSlider = false;
}

var alarmOn = true;

// Change the alarm button between ON and OFF
function toggleAlarm() {
    sendMessage(alarmOn ? "alarmOff" : "alarmOn");
}

// Show the selected volume value
function updateVolume(element) {
    movingVolumeSlider = true;

    var value =
        document.getElementById("volumeSlider").value;

    document.getElementById("volumeValue").innerHTML = value;

    console.log("Alarm volume:", value);
}

// Send the volume setting to the ESP32
function saveVolume() {
    sendMessage(
        "volume:" +
        document.getElementById("volumeSlider").value
    );

    movingVolumeSlider = false;
}

// Update the alarm status shown on the webpage
function updateAlarmStatus() {
    var status = document.getElementById("alarmStatus");

    if (alarmOn) {
        status.innerHTML = "Alarm Ready";
    } else {
        status.innerHTML = "Alarm Off";
    }
}

// Check if movement is selected as an alarm trigger
document.getElementById("movementTrigger")
    .addEventListener("change", function() {

    console.log("Movement trigger:", this.checked);

    sendMessage(
        this.checked ? "movementTriggerOn" : "movementTriggerOff"
    );
});

// Check if light is selected as an alarm trigger
document.getElementById("lightTrigger")
    .addEventListener("change", function() {

    console.log("Light trigger:", this.checked);

    sendMessage(
        this.checked ? "lightTriggerOn" : "lightTriggerOff"
    );
});

// Ask the ESP32 for its current settings
function getStates() {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("getStates");
    }
}

