#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* ssid = "Busther_Network";
const char* password = "Pbcsummit2020!";

// Web server running on port 80
WebServer server(80);

// Relay pins
#define RELAY_PIN1  25
#define RELAY_PIN2  26

// Turbidity sensor setup
const int turbiditySensorPin = 34;  // ADC pin for the sensor
const float highVoltage = 1.50;     // Voltage threshold for clear water
const float midVoltage = 1.06;      // Voltage threshold for cloudy water
const float lowVoltage = 0.50;      // Voltage threshold for dark water
const float highNTU = 50.0;         // NTU value for clear water
const float midNTU = 40.0;          // NTU value for cloudy water
const float lowNTU = 30.0;          // NTU value for dark water
const float turbidityThreshold = 40.0;  // NTU threshold for relay activation

// Relay and control timing
unsigned long lastActivationTime = 0;
unsigned long relayOnTime = 0;
const long relayDuration = 10000; // Time relays are on, in milliseconds
const long activationDelay = 30000; // Delay before reactivating relays, in milliseconds

// Relay states
bool relayState25 = false;
bool relayState26 = false;
bool manualRelay1 = false;
bool manualRelay2 = false;
float currentTurbidity = 0.0;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  digitalWrite(RELAY_PIN1, LOW);
  digitalWrite(RELAY_PIN2, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Define routes
  server.on("/toggleRelay1", []() {
    relayState25 = !relayState25;
    digitalWrite(RELAY_PIN1, relayState25 ? HIGH : LOW);
    Serial.print("Relay 25 state: ");
    Serial.println(relayState25 ? "ON" : "OFF");
    server.send(200, "text/plain", relayState25 ? "Relay 1 is ON" : "Relay 1 is OFF");
  });

  server.on("/toggleRelay2", []() {
    relayState26 = !relayState26;
    digitalWrite(RELAY_PIN2, relayState26 ? HIGH : LOW);
    Serial.print("Relay 26 state: ");
    Serial.println(relayState26 ? "ON" : "OFF");
    server.send(200, "text/plain", relayState26 ? "Relay 2 is ON" : "Relay 2 is OFF");
  });

  server.on("/turnOnRelay1", []() {
    manualRelay1 = true;
    relayState25 = true;
    digitalWrite(RELAY_PIN1, HIGH);
    Serial.println("Relay 25 turned ON");
    server.send(200, "text/plain", "Relay 1 is ON");
  });

  server.on("/turnOffRelay1", []() {
    manualRelay1 = false;
    relayState25 = false;
    digitalWrite(RELAY_PIN1, LOW);
    Serial.println("Relay 25 turned OFF");
    server.send(200, "text/plain", "Relay 1 is OFF");
  });

  server.on("/turnOnRelay2", []() {
    manualRelay2 = true;
    relayState26 = true;
    digitalWrite(RELAY_PIN2, HIGH);
    Serial.println("Relay 26 turned ON");
    server.send(200, "text/plain", "Relay 2 is ON");
  });

  server.on("/turnOffRelay2", []() {
    manualRelay2 = false;
    relayState26 = false;
    digitalWrite(RELAY_PIN2, LOW);
    Serial.println("Relay 26 turned OFF");
    server.send(200, "text/plain", "Relay 2 is OFF");
  });

  // Add endpoint to return turbidity data
  server.on("/getTurbidity", []() {
    String turbidityData = "{\"turbidity\":" + String(currentTurbidity) + "}";
    server.send(200, "application/json", turbidityData);
  });

  // Start server
  server.begin();
  Serial.println("HTTP server started");

  // Initial turbidity data send
  sendTurbidityData();
}

// Function to determine NTU based on voltage
float getNTU(float voltage) {
  if (voltage < midVoltage) {
    return lowNTU;  // NTU value for dark water
  } else if (voltage < highVoltage) {
    return midNTU;  // NTU value for cloudy water
  } else {
    return highNTU;  // NTU value for clear water
  }
}

// Reads and sends turbidity data and manages relay automation
void sendTurbidityData() {
  int sensorValue = analogRead(turbiditySensorPin);
  float voltage = sensorValue * (3.3 / 4095.0);
  float ntu = getNTU(voltage);
  currentTurbidity = ntu; // Update current turbidity value

  Serial.print("Turbidity (NTU): ");
  Serial.println(ntu);
  Serial.print("Voltage (V): ");
  Serial.println(voltage);

  // Manage relay activation based on NTU readings and timing constraints
  unsigned long currentTime = millis();
  if (ntu < turbidityThreshold && currentTime - lastActivationTime > activationDelay) {
    if (!manualRelay1) {
      digitalWrite(RELAY_PIN1, HIGH);
      relayState25 = true;
    }
    if (!manualRelay2) {
      digitalWrite(RELAY_PIN2, HIGH);
      relayState26 = true;
    }
    relayOnTime = currentTime;
    lastActivationTime = currentTime;
  }

  if (currentTime - relayOnTime >= relayDuration) {
    if (!manualRelay1) {
      digitalWrite(RELAY_PIN1, LOW);
      relayState25 = false;
    }
    if (!manualRelay2) {
      digitalWrite(RELAY_PIN2, LOW);
      relayState26 = false;
    }
  }
}

void loop() {
  server.handleClient();
  sendTurbidityData();

  delay(1000);  // Read and send turbidity data every second
}
