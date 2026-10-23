// Set up the address used to connect to the ESP32
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

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

// Read the JSON message sent from the ESP32
function onMessage(event) {
    var myObj = JSON.parse(event.data);

    // Print the received message for testing
    console.log(myObj);

    // Read the sensor settings sent from the ESP32
    movementOn = myObj.values[0].movement == "1";
    lightOn = myObj.values[1].light == "1";

    // Update the movement button using the ESP32 setting
    if (movementOn) {
        document.getElementById("movementButton").innerHTML = "ON";
    } else {
        document.getElementById("movementButton").innerHTML = "OFF";
    }

    // Update the light button using the ESP32 setting
    if (lightOn) {
        document.getElementById("lightButton").innerHTML = "ON";
    } else {
        document.getElementById("lightButton").innerHTML = "OFF";
    }

    console.log(event.data);
}

var movementOn = true;

// Change the movement sensor button between ON and OFF
function toggleMovement() {
    // Only send a message if the ESP32 is connected
    if (websocket && websocket.readyState === WebSocket.OPEN) {

        if (movementOn) {
            websocket.send("movementOff");
        } else {
            websocket.send("movementOn");
        }
    }
}

// Show the selected movement sensitivity
function updateMovementSensitivity(element) {
    var value = document.getElementById("movementSensitivity").value;

    document.getElementById("movementValue").innerHTML = value;

    console.log("Movement sensitivity:", value);
}

var lightOn = true;

// Change the light sensor button between ON and OFF
function toggleLight() {
    // Only send a message if the ESP32 is connected
    if (websocket && websocket.readyState === WebSocket.OPEN) {

        if (lightOn) {
            websocket.send("lightOff");
        } else {
            websocket.send("lightOn");
        }
    }
}

// Show the selected light sensitivity
function updateLightSensitivity(element) {
    var value = document.getElementById("lightSensitivity").value;

    document.getElementById("lightValue").innerHTML = value;

    console.log("Light sensitivity:", value);
}

var alarmOn = true;

// Change the alarm button between ON and OFF
function toggleAlarm() {
    alarmOn = !alarmOn;

    var button = document.getElementById("alarmButton");

    if (alarmOn) {
        button.innerHTML = "ON";
    } else {
        button.innerHTML = "OFF";
    }

    updateAlarmStatus();

    console.log("Alarm:", alarmOn);
}

// Show the selected volume value
function updateVolume(element) {
    var value = document.getElementById("volumeSlider").value;

    document.getElementById("volumeValue").innerHTML = value;

    console.log("Alarm volume:", value);
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
document.getElementById("movementTrigger").addEventListener("change", function() {
    console.log("Movement trigger:", this.checked);
});

// Check if light is selected as an alarm trigger
document.getElementById("lightTrigger").addEventListener("change", function() {
    console.log("Light trigger:", this.checked);
});

// Ask the ESP32 for its current settings
function getStates() {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("getStates");
    }
}
