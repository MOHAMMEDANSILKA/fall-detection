#include <Wire.h>
#include <MPU6050.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define ONE_WIRE_BUS 4 // GPIO4 for the temperature sensor

// WiFi credentials
const char* ssid = "Your_SSID";           // <-- Replace with your WiFi name
const char* password = "Your_PASSWORD";   // <-- Replace with your WiFi password

// Server URL
const char* serverName = "http://<PHONE_IP>:<PORT>/receive"; // <-- Replace with your phone IP and server port

MPU6050 mpu;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Fall detection parameters
const float fallThreshold = 20000; // Adjusted based on request
const int sampleInterval = 1000;    // Sampling interval in milliseconds (made slower to avoid flooding)
const unsigned long fallDelay = 2000; // Prevent duplicate detections

float prevAccX = 0.0, prevAccY = 0.0, prevAccZ = 0.0;
unsigned long lastFallTime = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    mpu.initialize();

    if (!mpu.testConnection()) {
        Serial.println("MPU6050 connection failed!");
        while (1); // Stop execution if MPU6050 is not detected
    }
    
    Serial.println("MPU6050 connected. Starting fall detection...");
    
    // Initialize temperature sensor
    sensors.begin();

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi.");
}

// Function to send data to server
void sendToServer(String message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName); // Connect to server
        http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Set header

        String httpRequestData = "data=" + message; // Prepare data
        int httpResponseCode = http.POST(httpRequestData); // Send POST request

        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }
        http.end(); // Close connection
    } else {
        Serial.println("WiFi Disconnected!");
    }
}

void detectFall() {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    // Compute acceleration change (jerk)
    float deltaX = abs(ax - prevAccX);
    float deltaY = abs(ay - prevAccY);
    float deltaZ = abs(az - prevAccZ);
    float accelerationChange = deltaX + deltaY + deltaZ;

    // Store previous acceleration values
    prevAccX = ax;
    prevAccY = ay;
    prevAccZ = az;

    // Check if acceleration change exceeds threshold
    if (accelerationChange > fallThreshold) {
        unsigned long currentTime = millis();
        if (currentTime - lastFallTime > fallDelay) { // Prevent duplicate detections
            Serial.println("🚨 Fall detected! 🚨");
            sendToServer("Fall detected!"); // Send to server
            lastFallTime = currentTime;
        }
    }
}

void readTemperature() {
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);

    if (temperatureC != DEVICE_DISCONNECTED_C) {
        Serial.print("Temperature: ");
        Serial.print(temperatureC);
        Serial.println(" °C");

        String tempString = "Temperature: " + String(temperatureC) + " C";
        sendToServer(tempString); // Send to server
    } else {
        Serial.println("Error: Temperature sensor not found!");
    }
}

void loop() {
    detectFall();
    readTemperature();
    delay(sampleInterval);
}
