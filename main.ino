#include <WiFi.h>
#include <HTTPClient.h>

// =====================
// WiFi credentials
// =====================
const char* ssid = "Residence1-Etage1";
const char* password = "iinnpptt";

// =====================
// Firmware version
// =====================
const char* currentFirmwareVersion = "1.0.0";

// =====================
// GitHub version file
// =====================
const char* versionUrl =
  "https://raw.githubusercontent.com/Nasreddiine/esp32_firmware/main/version.txt";

// =====================
// Timing
// =====================
const unsigned long checkInterval = 2 * 60 * 1000; // 2 minutes
unsigned long lastCheckTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================");
  Serial.println("ESP32 Firmware v1.0.0 (Manual)");
  Serial.println("OTA system initialized (check only)");
  Serial.println("=================================");

  connectToWiFi();
  checkForNewFirmware();
}

void loop() {
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    checkForNewFirmware();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void checkForNewFirmware() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping version check");
    return;
  }

  Serial.println("\nChecking for new firmware...");

  HTTPClient http;
  http.begin(versionUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();

    Serial.println("Current version : " + String(currentFirmwareVersion));
    Serial.println("Latest  version : " + latestVersion);

    if (latestVersion != currentFirmwareVersion) {
      Serial.println(">>> New firmware AVAILABLE (OTA ready)");
    } else {
      Serial.println("Firmware is up to date");
    }
  } else {
    Serial.printf("Version check failed (HTTP %d)\n", httpCode);
  }

  http.end();
}
