#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TinyGPS++.h>
#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include <Base64.h>


const char *ssid = "Vivo";
const char *password = "jemsheeee567";

// Firebase URL
String firebaseURL = "https://glass-f7e3b-default-rtdb.firebaseio.com/emergency_status.json";

// Twilio credentials
const char* accountSid = "ACc35e6be0e8e1459683d1412b5d2a93cb";  // Twilio Account SID
const char* authToken = "bbb4dd56553e69193813614d14da85a0";    // Twilio Auth Token
const char* fromPhone = "+12184605045";    // Your Twilio phone number
const char* toPhone = "+917592894755";    // The recipient phone number

// Firebase emergency check node
String emergencyCheckURL = "https://glass-f7e3b-default-rtdb.firebaseio.com/emergency_status.json";

// Initialize TinyGPS++ for GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // Use UART1 for GPS (TX=3, RX=1)

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  gpsSerial.begin(9600, SERIAL_8N1, 3, 1); // RX=3, TX=1

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Initialize camera
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

  // Set pixel format to RGB565
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_240X240;  
  config.fb_count = 1;  
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // Camera initialization
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

  // Ensure WiFi stays on
  WiFi.setSleep(false);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void loop() {
  // Check Firebase for emergency status
  checkEmergencyStatus();

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
}

// Send location data to Firebase
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

// Check Firebase for emergency status
void checkEmergencyStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(emergencyCheckURL);  // Firebase URL

    int httpResponseCode = http.GET();
    String payload = http.getString();

    if (httpResponseCode == 200) {
      Serial.println("Emergency Status Check Success");
      Serial.println(payload); // Print the response

      // Check if "help" status is detected
      if (payload.indexOf("\"status\":\"help\"") > 0) {
        String timestamp = extractTimestamp(payload);
        sendEmergencySMS(timestamp);
      }
    } else {
      Serial.println("Error in GET request. Response code: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.begin(ssid, password);
  }
}

// Extract timestamp from Firebase payload
String extractTimestamp(String payload) {
  int startIdx = payload.indexOf("\"timestamp\":\"") + 13;
  int endIdx = payload.indexOf("\"", startIdx);
  return payload.substring(startIdx, endIdx);
}

// Send Emergency SMS using Twilio
void sendEmergencySMS(String timestamp) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    HTTPClient http;

    String credentials = String(accountSid) + ":" + String(authToken);
    String auth = "Basic " + base64::encode(credentials);

    String postData = "From=" + String(fromPhone) + "&To=" + String(toPhone) + 
                      "&Body=Emergency! Help required! Timestamp: " + timestamp + 
                      " Current Location: Latitude: " + String(gps.location.lat(), 6) + 
                      " Longitude: " + String(gps.location.lng(), 6);

    http.begin(client, "https://api.twilio.com/2010-04-01/Accounts/" + String(accountSid) + "/Messages.json");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", auth);

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode == 200) {
      Serial.println("Emergency SMS Sent Successfully!");
    } else {
      Serial.println("Error in sending SMS. Response code: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected. Cannot send SMS.");
  }
}