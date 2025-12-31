#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

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
// GitHub URLs
// =====================
const char* versionUrl = "https://raw.githubusercontent.com/Nasreddiine/esp32_firmware/main/version.txt";
const char* firmwareBaseUrl = "https://github.com/Nasreddiine/esp32_firmware/releases/download/v";

// =====================
// Timing
// =====================
const unsigned long checkInterval = 2 * 60 * 1000; // 2 minutes
unsigned long lastCheckTime = 0;
bool updateInProgress = false;

// =====================
// Preferences for storing version
// =====================
#include <Preferences.h>
Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32 Auto-OTA Firmware System");
  Serial.println("=================================");

  // Initialize preferences
  preferences.begin("firmware", false);
  
  // Read stored version (if any)
  String storedVersion = preferences.getString("version", "1.0.0");
  Serial.println("Stored version: " + storedVersion);
  Serial.println("Current version: " + String(currentFirmwareVersion));

  connectToWiFi();
  
  // Check for updates on startup
  checkForNewFirmware();
}

void loop() {
  if (!updateInProgress && (millis() - lastCheckTime > checkInterval)) {
    lastCheckTime = millis();
    checkForNewFirmware();
  }
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void checkForNewFirmware() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping version check");
    connectToWiFi();
    return;
  }

  Serial.println("\n[OTA] Checking for new firmware...");

  HTTPClient http;
  http.begin(versionUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();

    Serial.println("[OTA] Current version: " + String(currentFirmwareVersion));
    Serial.println("[OTA] Latest version : " + latestVersion);

    if (latestVersion != currentFirmwareVersion) {
      Serial.println("[OTA] >>> New firmware AVAILABLE! Starting update...");
      performOTA(latestVersion);
    } else {
      Serial.println("[OTA] Firmware is up to date");
    }
  } else {
    Serial.printf("[OTA] Version check failed (HTTP %d)\n", httpCode);
  }

  http.end();
}

void performOTA(String newVersion) {
  updateInProgress = true;
  
  Serial.println("[OTA] Starting firmware update to version: " + newVersion);
  
  // Construct firmware download URL
  String firmwareUrl = String(firmwareBaseUrl) + newVersion + "/firmware.bin";
  Serial.println("[OTA] Download URL: " + firmwareUrl);

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation (for simplicity)
  
  http.begin(client, firmwareUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(30000);
  
  Serial.println("[OTA] Downloading firmware...");
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
    
    if (contentLength <= 0) {
      Serial.println("[OTA] ERROR: Invalid content length");
      updateInProgress = false;
      return;
    }
    
    // Begin OTA update
    if (Update.begin(contentLength)) {
      Serial.println("[OTA] Starting update process...");
      
      // Create buffer for reading
      uint8_t buff[128] = { 0 };
      
      // Get TCP stream
      WiFiClient* stream = http.getStreamPtr();
      
      // Read all data from server
      size_t written = 0;
      while (http.connected() && (written < contentLength)) {
        // Read up to 128 bytes
        size_t size = stream->available();
        
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          
          // Write to OTA
          Update.write(buff, c);
          written += c;
          
          // Progress indicator
          if (written % 4096 == 0) {
            Serial.printf("[OTA] Progress: %d%%\n", (written * 100) / contentLength);
          }
        }
        delay(1);
      }
      
      if (Update.end()) {
        Serial.println("[OTA] Update completed successfully");
        
        if (Update.isFinished()) {
          // Store new version in preferences
          preferences.putString("version", newVersion);
          preferences.end();
          
          Serial.println("[OTA] Restarting ESP32 with new firmware...");
          delay(1000);
          ESP.restart();
        } else {
          Serial.println("[OTA] ERROR: Update not finished");
        }
      } else {
        Serial.printf("[OTA] ERROR: Update failed: %s\n", Update.errorString());
        Update.printError(Serial);
      }
    } else {
      Serial.println("[OTA] ERROR: Not enough space for OTA");
    }
  } else {
    Serial.printf("[OTA] ERROR: Download failed (HTTP %d)\n", httpCode);
  }
  
  http.end();
  updateInProgress = false;
  Serial.println("[OTA] Update process ended");
}
