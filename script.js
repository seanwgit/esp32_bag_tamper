// Set up the address used to connect to the ESP32
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

// Wait for the webpage to finish loading before connecting
window.addEventListener("load", onload);

// Start the WebSocket connection when the page loads
function onload(event) {
    initWebSocket();
}

// Create the connection and set up what happens when it opens,
// closes or receives a message
function initWebSocket() {
    console.log("Trying to open a WebSocket connection...");
    websocket = new WebSocket(gateway);

    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// Show that the ESP32 has connected
function onOpen(event) {
    console.log("WebSocket opened");

    document.getElementById("connectionStatus").innerHTML =
        "ESP connected";
}

// Show that the ESP32 has disconnected and try to reconnect
// after 2 seconds
function onClose(event) {
    console.log("WebSocket closed");

    document.getElementById("connectionStatus").innerHTML =
        "ESP disconnected";

    setTimeout(initWebSocket, 2000);
}

// Run this function when a message comes from the ESP32
function onMessage(event) {
    // Convert the message from JSON text into a JavaScript object
    var myObj = JSON.parse(event.data);

    // Display the received data in the browser console for testing
    console.log(myObj);
    console.log(event.data);

    // This will be changed later so the webpage can update
    // its buttons and status using information from the ESP32
}

var movementOn = true;

// Change the movement sensor between ON and OFF
function toggleMovement() {
    // Reverse the current setting
    movementOn = !movementOn;

    var button = document.getElementById("movementButton");

    // Change the button text to match the setting
    if (movementOn) {
        button.innerHTML = "ON";
    } else {
        button.innerHTML = "OFF";
    }

    // Print the setting to the console to check that it works
    console.log("Movement sensor:", movementOn);

    // A message could be sent to the ESP32 here once the
    // WebSocket commands are connected to the C++ code
    // websocket.send("movementOn");
    // websocket.send("movementOff");
}

// Update the number shown beside the movement sensitivity slider
function updateMovementSensitivity(element) {
    var value = document.getElementById("movementSensitivity").value;

    document.getElementById("movementValue").innerHTML = value;

    // Check the selected value while testing
    console.log("Movement sensitivity:", value);
}

var lightOn = true;

// Change the light sensor between ON and OFF
function toggleLight() {
    lightOn = !lightOn;

    var button = document.getElementById("lightButton");

    if (lightOn) {
        button.innerHTML = "ON";
    } else {
        button.innerHTML = "OFF";
    }

    console.log("Light sensor:", lightOn);
}

// Update the number shown beside the light sensitivity slider
function updateLightSensitivity(element) {
    var value = document.getElementById("lightSensitivity").value;

    document.getElementById("lightValue").innerHTML = value;

    console.log("Light sensitivity:", value);
}

var alarmOn = true;

// Turn the alarm setting on or off
function toggleAlarm() {
    alarmOn = !alarmOn;

    var button = document.getElementById("alarmButton");

    if (alarmOn) {
        button.innerHTML = "ON";
    } else {
        button.innerHTML = "OFF";
    }

    // Update the alarm status text after changing the setting
    updateAlarmStatus();

    console.log("Alarm:", alarmOn);
}

// Show the selected volume value beside the slider
function updateVolume(element) {
    var value = document.getElementById("volumeSlider").value;

    document.getElementById("volumeValue").innerHTML = value;

    console.log("Alarm volume:", value);
}

// Change the status text depending on whether the alarm is on
function updateAlarmStatus() {
    var status = document.getElementById("alarmStatus");

    if (alarmOn) {
        status.innerHTML = "Alarm Ready";
    } else {
        status.innerHTML = "Alarm Off";
    }
}

// Check whether movement is selected as an alarm trigger
document.getElementById("movementTrigger").addEventListener("change", function() {
    console.log("Movement trigger:", this.checked);
});

// Check whether light changes are selected as an alarm trigger
document.getElementById("lightTrigger").addEventListener("change", function() {
    console.log("Light trigger:", this.checked);
});

// Ask the ESP32 for its current settings
function getStates() {
    // Only send the request if the WebSocket is connected
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("getStates");
    }
}
