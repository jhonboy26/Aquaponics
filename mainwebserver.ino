#include <Arduino.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* ssid = "Busther_Network";
const char* pass = "Pbcsummit2020!";

// IP address of ESP32 B (relay controller)
const char* relayControllerIP = "192.168.0.116"; 

// Web server running on port 80
WebServer server(80);

// Google Sheets Script Web App URL
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbzcjab2h43jT1d7cd8GMXg8-f5x4jPyaLlCoScdLY1Aju33PEnUW5OasqlsBlydn7C5aQ/exec";

// Sensor and pin configuration
#define pH_SENSOR_PIN 36
#define relayPinHigh 25
#define relayPinLow 26
#define ONE_WIRE_BUS 4
#define SERVO_PIN 5
#define TRIG_PIN 15
#define ECHO_PIN 16

// Calibration constants
float voltageAtPH4 = 0.085;
float voltageAtPH7 = 1.045;
float voltageAtPH10 = 2.005;
float neutralVoltage = voltageAtPH7;
float slope = (10.0 - 4.0) / (voltageAtPH10 - voltageAtPH4);

// Turbidity constants
float currentTurbidity = 0.0;

// Timers and control flags
unsigned long lastRelayToggleTime = 0;
const long relayToggleInterval = 30000;
unsigned long previousMillis = 0;
const long interval = 1000;
unsigned long servoInterval = 1200000; // Default servo interval of 20 minutes (1200000 ms)
unsigned long previousServoMillis = 0;
const long googleUploadInterval = 60000;
unsigned long previousGoogleUploadMillis = 0;
bool manualServoControl = false;  // Manual control flag for servo

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Servo myServo;

float currentPH = 0.0;
float currentTemperature = 0.0;
float currentDistance = 0.0;

void calibrateSensor() {
  neutralVoltage = voltageAtPH7;
  slope = (10.0 - 4.0) / (voltageAtPH10 - voltageAtPH4);
}

void setup() {
  Serial.begin(115200);
  pinMode(relayPinHigh, OUTPUT);
  pinMode(relayPinLow, OUTPUT);
  digitalWrite(relayPinHigh, LOW);
  digitalWrite(relayPinLow, LOW);

  sensors.begin();
  myServo.attach(SERVO_PIN);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, pass);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize web server routes
  server.on("/", handleRoot);
  server.on("/updateTurbidity", handleUpdateTurbidity);
  server.on("/sensorData", handleSensorData);
  server.on("/toggleRelay", handleToggleRelay);
  server.on("/toggleRemoteRelay1", handleToggleRemoteRelay1);
  server.on("/toggleRemoteRelay2", handleToggleRemoteRelay2);
  server.on("/turnOnRelay1", handleTurnOnRelay1);   // New endpoint for turning on relay 1
  server.on("/turnOffRelay1", handleTurnOffRelay1); // New endpoint for turning off relay 1
  server.on("/turnOnRelay2", handleTurnOnRelay2);   // New endpoint for turning on relay 2
  server.on("/turnOffRelay2", handleTurnOffRelay2); // New endpoint for turning off relay 2
  server.on("/moveServo", handleMoveServo);         // New endpoint for manual servo control
  server.on("/setServoInterval", handleSetServoInterval); // New endpoint to set servo interval

  server.begin();
  Serial.println("HTTP server started");

  calibrateSensor();
  readSensors();
}

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "    <meta charset='UTF-8'>";
  html += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "    <title>Aquaponics Control Panel</title>";
  html += "    <link href='https://fonts.googleapis.com/css2?family=Roboto:wght@400;500&family=Poppins:wght@700&display=swap' rel='stylesheet'>";
  html += "    <style>";
  html += "        body {";
  html += "            font-family: 'Roboto', sans-serif;";
  html += "            background-color: #D3D9D4;";
  html += "            color: #212A31;";
  html += "            margin: 0;";
  html += "            padding: 0;";
  html += "            display: flex;";
  html += "            justify-content: center;";
  html += "            align-items: center;";
  html += "            height: 100vh;";
  html += "            flex-direction: column;";
  html += "        }";
  html += "        .container {";
  html += "            background-color: #ffffff;";
  html += "            border-radius: 10px;";
  html += "            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);";
  html += "            width: 90%;";
  html += "            max-width: 600px;";
  html += "            padding: 20px;";
  html += "            text-align: center;";
  html += "            border: 1px solid #DEE2E6;";
  html += "        }";
  html += "        h1 {";
  html += "            font-family: 'Poppins', sans-serif;";
  html += "            font-size: 2em;";
  html += "            color: #ffffff;";
  html += "            background: linear-gradient(90deg, #124E66, #2E3944);";
  html += "            padding: 10px;";
  html += "            border-radius: 8px;";
  html += "            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);";
  html += "            margin-bottom: 20px;";
  html += "            text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.1);";
  html += "        }";
  html += "        .sensor-data {";
  html += "            display: grid;";
  html += "            grid-template-columns: 1fr 1fr;";
  html += "            gap: 20px;";
  html += "            margin-bottom: 20px;";
  html += "        }";
  html += "        .sensor-card {";
  html += "            background-color: #F8F9FA;";
  html += "            padding: 20px;";
  html += "            border-radius: 10px;";
  html += "            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);";
  html += "            text-align: center;";
  html += "        }";
  html += "        .sensor-card h2 {";
  html += "            font-size: 1.2em;";
  html += "            margin-bottom: 10px;";
  html += "            color: #2E3944;";
  html += "        }";
  html += "        .sensor-card p {";
  html += "            font-size: 1.5em;";
  html += "            margin: 0;";
  html += "            color: #212A31;";
  html += "        }";
  html += "        .sensor-icon {";
  html += "            width: 30px;";
  html += "            height: 30px;";
  html += "            margin-bottom: 10px;";
  html += "        }";
  html += "        .controls {";
  html += "            display: flex;";
  html += "            flex-direction: column;";
  html += "            gap: 20px;";
  html += "        }";
  html += "        .control-section {";
  html += "            background-color: #F8F9FA;";
  html += "            padding: 15px;";
  html += "            border-radius: 10px;";
  html += "            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);";
  html += "        }";
  html += "        .control-section h2 {";
  html += "            font-size: 1.5em;";
  html += "            color: #2E3944;";
  html += "            margin-bottom: 15px;";
  html += "        }";
  html += "        .control-group {";
  html += "            display: flex;";
  html += "            flex-wrap: wrap;";
  html += "            gap: 10px;";
  html += "            justify-content: center;";
  html += "        }";
  html += "        button {";
  html += "            padding: 10px 20px;";
  html += "            font-size: 1em;";
  html += "            color: #fff;";
  html += "            background: linear-gradient(145deg, #124E66, #2E3944);";
  html += "            border: none;";
  html += "            border-radius: 5px;";
  html += "            cursor: pointer;";
  html += "            transition: background 0.3s, transform 0.2s;";
  html += "            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);";
  html += "            flex: 1 1 calc(50% - 20px);";
  html += "        }";
  html += "        button:hover {";
  html += "            background: linear-gradient(145deg, #2E3944, #124E66);";
  html += "            transform: translateY(-2px);";
  html += "        }";
  html += "        button:active {";
  html += "            background: #212A31;";
  html += "            transform: translateY(0);";
  html += "        }";
  html += "        button:focus {";
  html += "            outline: none;";
  html += "            box-shadow: 0 0 0 3px rgba(18, 78, 102, 0.25);";
  html += "        }";
  html += "        .servo-control {";
  html += "            display: flex;";
  html += "            justify-content: space-between;";
  html += "            align-items: center;";
  html += "            padding: 10px 0;";
  html += "            gap: 10px;";
  html += "            flex-wrap: wrap;";
  html += "        }";
  html += "        .servo-control input {";
  html += "            padding: 10px;";
  html += "            font-size: 1em;";
  html += "            flex: 1;";
  html += "            border: 1px solid #ddd;";
  html += "            border-radius: 5px;";
  html += "        }";
  html += "        @media (max-width: 600px) {";
  html += "            h1 {";
  html += "                font-size: 1.5em;";
  html += "            }";
  html += "            .sensor-data {";
  html += "                grid-template-columns: 1fr;";
  html += "                gap: 10px;";
  html += "            }";
  html += "            button {";
  html += "                padding: 10px;";
  html += "                flex: 1 1 100%;";
  html += "            }";
  html += "        }";
  html += "    </style>";
  html += "    <script>";
  html += "        function sendRequest(endpoint) {";
  html += "            var xhr = new XMLHttpRequest();";
  html += "            xhr.open('GET', endpoint, true);";
  html += "            xhr.send();";
  html += "        }";
  html += "        function updateSensorData() {";
  html += "            var xhr = new XMLHttpRequest();";
  html += "            xhr.open('GET', '/sensorData', true);";
  html += "            xhr.onload = function() {";
  html += "                if (xhr.status == 200) {";
  html += "                    var data = JSON.parse(xhr.responseText);";
  html += "                    document.getElementById('pHValue').innerText = data.pH.toFixed(2);";
  html += "                    document.getElementById('waterTemp').innerText = data.temperature.toFixed(2) + ' °C';";
  html += "                    document.getElementById('distance').innerText = data.distance.toFixed(2) + ' cm';";
  html += "                    document.getElementById('turbidity').innerText = data.turbidity.toFixed(2);";
  html += "                }";
  html += "            };";
  html += "            xhr.send();";
  html += "        }";
  html += "        function setServoInterval() {";
  html += "            var interval = document.getElementById('servoInterval').value;";
  html += "            var xhr = new XMLHttpRequest();";
  html += "            xhr.open('GET', '/setServoInterval?interval=' + interval, true);";
  html += "            xhr.onload = function() {";
  html += "                if (xhr.status == 200) {";
  html += "                    alert('Servo interval set to ' + interval + ' milliseconds');";
  html += "                }";
  html += "            };";
  html += "            xhr.send();";
  html += "        }";
  html += "        setInterval(updateSensorData, 1000);";
  html += "    </script>";
  html += "</head>";
  html += "<body>";
  html += "    <div class='container'>";
  html += "        <h1>Aquaponics Control Panel</h1>";
  html += "        <div class='sensor-data'>";
  html += "            <div class='sensor-card'>";
  html += "                <img src='https://img.icons8.com/fluency/48/ph-meter.png' alt='pH Icon' class='sensor-icon'>";
  html += "                <h2>pH Value</h2>";
  html += "                <p id='pHValue'>0.0</p>";
  html += "            </div>";
  html += "            <div class='sensor-card'>";
  html += "                <img src='https://img.icons8.com/fluency/48/temperature.png' alt='Temperature Icon' class='sensor-icon'>";
  html += "                <h2>Water Temperature</h2>";
  html += "                <p id='waterTemp'>0.0 °C</p>";
  html += "            </div>";
  html += "            <div class='sensor-card'>";
  html += "                <img src='https://img.icons8.com/fluency/48/ruler.png' alt='Distance Icon' class='sensor-icon'>";
  html += "                <h2>Distance</h2>";
  html += "                <p id='distance'>0.0 cm</p>";
  html += "            </div>";
  html += "            <div class='sensor-card'>";
  html += "                <img src='https://img.icons8.com/fluency/48/water.png' alt='Turbidity Icon' class='sensor-icon'>";
  html += "                <h2>Turbidity</h2>";
  html += "                <p id='turbidity'>0.0</p>";
  html += "            </div>";
  html += "        </div>";
  html += "        <div class='controls'>";
  html += "            <div class='control-section'>";
  html += "                <h2>pH Solutions</h2>";
  html += "                <div class='control-group'>";
  html += "                    <button onclick='sendRequest(\"/toggleRelay?pin=25\")'>pH Down</button>";
  html += "                    <button onclick='sendRequest(\"/toggleRelay?pin=26\")'>pH Up</button>";
  html += "                </div>";
  html += "            </div>";
  html += "            <div class='control-section'>";
  html += "                <h2>Turbidity</h2>";
  html += "                <div class='control-group'>";
  html += "                    <button onclick='sendRequest(\"/turnOnRelay1\")'>Turn ON Drain Pump</button>";
  html += "                    <button onclick='sendRequest(\"/turnOffRelay1\")'>Turn OFF Drain Pump</button>";
  html += "                    <button onclick='sendRequest(\"/turnOnRelay2\")'>Turn ON Water In Pump</button>";
  html += "                    <button onclick='sendRequest(\"/turnOffRelay2\")'>Turn OFF Water In Pump</button>";
  html += "                </div>";
  html += "            </div>";
  html += "            <button onclick='sendRequest(\"/moveServo\")'>Move Servo</button>";
  html += "            <div class='control-section servo-control'>";
  html += "                <input type='number' id='servoInterval' placeholder='Enter milliseconds' value='10000'>";
  html += "                <button onclick='setServoInterval()'>Set Servo Interval</button>";
  html += "            </div>";
  html += "        </div>";
  html += "    </div>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleUpdateTurbidity() {
  if (server.hasArg("value")) {
    currentTurbidity = server.arg("value").toFloat();
    Serial.print("Updated Turbidity: ");
    Serial.println(currentTurbidity);
  }
  server.send(200, "text/plain", "Turbidity updated.");
}

void handleSensorData() {
  // Fetch turbidity data from ESP32 B
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/getTurbidity";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("Turbidity data from ESP32 B: " + payload);
      int turbidityIndex = payload.indexOf(":") + 1;
      int turbidityEnd = payload.indexOf("}");
      currentTurbidity = payload.substring(turbidityIndex, turbidityEnd).toFloat();
    } else {
      Serial.print("Error getting turbidity data: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }

  String json = "{";
  json += "\"pH\":" + String(currentPH) + ",";
  json += "\"temperature\":" + String(currentTemperature) + ",";
  json += "\"distance\":" + String(currentDistance) + ",";
  json += "\"turbidity\":" + String(currentTurbidity);
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggleRelay() {
  if (server.hasArg("pin")) {
    int pin = server.arg("pin").toInt();
    static bool relayState25 = false;
    static bool relayState26 = false;

    if (pin == 25) {
      relayState25 = !relayState25;
      digitalWrite(relayPinHigh, relayState25 ? HIGH : LOW);
      Serial.print("Relay 25 state: ");
      Serial.println(relayState25 ? "ON" : "OFF");
      server.send(200, "text/plain", relayState25 ? "Relay 25 is ON" : "Relay 25 is OFF");
    } else if (pin == 26) {
      relayState26 = !relayState26;
      digitalWrite(relayPinLow, relayState26 ? HIGH : LOW);
      Serial.print("Relay 26 state: ");
      Serial.println(relayState26 ? "ON" : "OFF");
      server.send(200, "text/plain", relayState26 ? "Relay 26 is ON" : "Relay 26 is OFF");
    } else {
      server.send(400, "text/plain", "Invalid pin");
    }
  } else {
    server.send(400, "text/plain", "Pin not specified");
  }
}

void handleToggleRemoteRelay1() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/toggleRelay1";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 1 toggle response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to toggle remote relay 1");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleToggleRemoteRelay2() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/toggleRelay2";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 2 toggle response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to toggle remote relay 2");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleTurnOnRelay1() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/turnOnRelay1";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 1 ON response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to turn ON remote relay 1");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleTurnOffRelay1() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/turnOffRelay1";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 1 OFF response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to turn OFF remote relay 1");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleTurnOnRelay2() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/turnOnRelay2";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 2 ON response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to turn ON remote relay 2");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleTurnOffRelay2() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + relayControllerIP + "/turnOffRelay2";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Remote relay 2 OFF response: ");
      Serial.println(response);
      server.send(200, "text/plain", response);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      server.send(500, "text/plain", "Failed to turn OFF remote relay 2");
    }
    http.end();
  } else {
    server.send(500, "text/plain", "WiFi Disconnected");
  }
}

void handleMoveServo() {
  Serial.println("Manual servo control activated.");
  manualServoControl = true;
  moveServo();
  server.send(200, "text/plain", "Servo moved manually.");
}

void handleSetServoInterval() {
  if (server.hasArg("interval")) {
    servoInterval = server.arg("interval").toInt();
    Serial.print("Updated Servo Interval: ");
    Serial.println(servoInterval);
    server.send(200, "text/plain", "Servo interval set to " + String(servoInterval) + " milliseconds.");
  } else {
    server.send(400, "text/plain", "Interval not specified");
  }
}

void readSensors() {
  currentPH = readPHValue();
  readWaterTemperature();
  readDistance();

  controlRelays(currentPH);
}

float readPHValue() {
  int sensorValue = analogRead(pH_SENSOR_PIN);
  float sensorVoltage = sensorValue * (3.3 / 4095.0);
  float pH = 7 + (sensorVoltage - neutralVoltage) * slope;

  Serial.print("pH Value: ");
  Serial.println(pH);
  Serial.print("Sensor Voltage: ");
  Serial.println(sensorVoltage);

  return pH;
}

void readWaterTemperature() {
  sensors.requestTemperatures();
  currentTemperature = sensors.getTempCByIndex(0);

  Serial.print("Water Temperature: ");
  Serial.println(currentTemperature);
}

void moveServo() {
  myServo.write(90);
  delay(2000);
  myServo.write(0);
  manualServoControl = false;  // Reset manual control flag after movement
}

void readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  currentDistance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(currentDistance);
  Serial.println(" cm");
}

void controlRelays(float pH) {
  unsigned long currentTime = millis();

  if (currentTime - lastRelayToggleTime > relayToggleInterval) {
    if (pH > 10.0) {
      digitalWrite(relayPinHigh, HIGH);
      Serial.println("High pH Relay ON");
    } else {
      digitalWrite(relayPinHigh, LOW);
      Serial.println("High pH Relay OFF");
    }

    if (pH < 2.0) {
      digitalWrite(relayPinLow, HIGH);
      Serial.println("Low pH Relay ON");
    } else {
      digitalWrite(relayPinLow, LOW);
      Serial.println("Low pH Relay OFF");
    }
    lastRelayToggleTime = currentTime;
  }
}

void sendDataToGoogleSheets() {
  HTTPClient http;
  String jsonData = "{";
  jsonData += "\"pH\":" + String(currentPH) + ",";
  jsonData += "\"temperature\":" + String(currentTemperature) + ",";
  jsonData += "\"distance\":" + String(currentDistance) + ",";
  jsonData += "\"turbidity\":" + String(currentTurbidity);
  jsonData += "}";

  http.begin(googleScriptURL);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Payload: " + payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readSensors();
  }

  if (!manualServoControl && currentMillis - previousServoMillis >= servoInterval) {
    previousServoMillis = currentMillis;
    moveServo();
  }

  if (currentMillis - previousGoogleUploadMillis >= googleUploadInterval) {
    previousGoogleUploadMillis = currentMillis;
    sendDataToGoogleSheets();
  }

  server.handleClient();
}
