#include <HTTPClient.h>
#include <TinyGPS++.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoJson.h>
//#include <SoftwareSerial.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Firebase URLs
String firebaseURL = "https://glass-f7e3b-default-rtdb.firebaseio.com/gps.json";
String emergencyStatusURL = "https://glass-f7e3b-default-rtdb.firebaseio.com/emergency_status.json";

// WiFi Credentials
const char *ssid = "Vivo";
const char *password = "jemsheeee567";

// Twilio Credentials
const char* accountSid = "ACc35e6be0e8e1459683d1412b5d2a93cb";  // Twilio Account SID
const char* authToken = "bbb4dd56553e69193813614d14da85a0";    // Twilio Auth Token
const char* fromPhone = "+12184605045";        // Your Twilio phone number
const char* toPhone = "+917592894755";          // Recipient phone number (change as needed)

String lastTimestamp = ""; // Variable to store the last timestamp from Firebase

// Initialize TinyGPS++
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // Use UART1 for GPS (TX=3, RX=1)

// Camera setup function
void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  gpsSerial.begin(9600, SERIAL_8N1, 3, 1); // RX=3, TX=1
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Camera initialization
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_240X240;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  WiFi.setSleep(false);
  startCameraServer();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  // Initial timestamp check
  checkForEmergencyStatus();
}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());

    if (gps.location.isUpdated()) {
      float latitude = gps.location.lat();
      float longitude = gps.location.lng();
      Serial.print("Latitude: ");
      Serial.print(latitude, 6);
      Serial.print(" Longitude: ");
      Serial.println(longitude, 6);

      sendToFirebase(latitude, longitude);
    }
  }

  // Check for emergency status every 5 seconds
  checkForEmergencyStatus();
}

// Send GPS coordinates to Firebase
void sendToFirebase(float lat, float lng) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"latitude\": " + String(lat, 6) + ", \"longitude\": " + String(lng, 6) + "}";

    int httpResponseCode = http.PUT(jsonData);

    Serial.print("Firebase Response: ");
    Serial.println(httpResponseCode);
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }
  delay(5000);
}

// Function to check Firebase for emergency status
void checkForEmergencyStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(emergencyStatusURL);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      String timestamp = extractTimestamp(payload);

      // If the timestamp from Firebase is different from lastTimestamp, send the SMS
      if (timestamp != lastTimestamp && timestamp != "") {
        Serial.println("Emergency timestamp has changed!");
        
        // Get the current GPS location
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();
        
        // Send the SMS with updated timestamp and location
        sendEmergencyAlert(timestamp, latitude, longitude);
        
        // Update lastTimestamp to the new timestamp from Firebase
        lastTimestamp = timestamp;
      }
    } else {
      Serial.println("Error: Could not fetch emergency status.");
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }

  delay(5000);  // Check every 5 seconds
}

// Extract timestamp from the Firebase response (JSON format)
String extractTimestamp(String payload) {
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.f_str());
    return "";
  }
  const char* timestamp = doc["timestamp"];
  return timestamp ? String(timestamp) : "";
}

// Send emergency alert via SMS using Twilio API
void sendEmergencyAlert(String timestamp, float latitude, float longitude) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://api.twilio.com/2010-04-01/Accounts/" + String(accountSid) + "/Messages.json");
    http.setAuthorization(accountSid, authToken);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Construct the message with updated timestamp and GPS location
    String message = "Emergency Alert: Timestamp: " + timestamp + 
                     " Latitude: " + String(latitude, 6) + 
                     " Longitude: " + String(longitude, 6);

    // Payload to send to Twilio API
    String payload = "From=" + String(fromPhone) + "&To=" + String(toPhone) + "&Body=" + message;

    // Send the POST request
    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
      Serial.println("Emergency SMS sent!");
    } else {
      Serial.println("Failed to send SMS. Response code: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }
  delay(2000);  // Small delay before next action
}