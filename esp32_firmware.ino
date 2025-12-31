#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

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
// GitHub URLs - Raw.githubusercontent.com supports HTTPS with regular HTTPClient
// =====================
const char* versionUrl = "https://raw.githubusercontent.com/Nasreddiine/esp32_firmware/main/version.txt";
const char* firmwareBaseUrl = "https://github.com/Nasreddiine/esp32_firmware/releases/download/v";

// =====================
// Timing
// =====================
const unsigned long checkInterval = 2 * 60 * 1000; // 2 minutes
unsigned long lastCheckTime = 0;
bool updateInProgress = false;

Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32 Auto-OTA Firmware System");
  Serial.println("=================================");

  preferences.begin("firmware", false);
  String storedVersion = preferences.getString("version", "1.0.0");
  Serial.println("Stored version: " + storedVersion);
  Serial.println("Current version: " + String(currentFirmwareVersion));

  connectToWiFi();
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
    delay(500);
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
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected, skipping version check");
      return;
    }
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
  
  // Use the releases API to get direct download link
  String firmwareUrl = String(firmwareBaseUrl) + newVersion + "/firmware.bin";
  Serial.println("[OTA] Download URL: " + firmwareUrl);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000); // Longer timeout for firmware download
  
  Serial.println("[OTA] Downloading firmware...");
  http.begin(firmwareUrl);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      contentLength = http.getSize(); // Try again
    }
    
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
    
    if (contentLength <= 0) {
      Serial.println("[OTA] ERROR: Could not determine file size");
      updateInProgress = false;
      return;
    }
    
    // Begin OTA update
    if (Update.begin(contentLength, U_FLASH)) {
      Serial.println("[OTA] Starting update process...");
      
      // Get the stream
      WiFiClient* stream = http.getStreamPtr();
      
      // Create buffer
      uint8_t buff[256] = { 0 };
      size_t written = 0;
      int progress = 0;
      
      // Read all data
      while (http.connected() && (written < contentLength)) {
        int available = stream->available();
        if (available > 0) {
          int toRead = min(available, (int)sizeof(buff));
          int c = stream->readBytes(buff, toRead);
          
          if (Update.write(buff, c) != c) {
            Serial.println("[OTA] ERROR: Write failed");
            break;
          }
          
          written += c;
          
          // Show progress every 10%
          int newProgress = (written * 100) / contentLength;
          if (newProgress >= progress + 10) {
            progress = newProgress;
            Serial.printf("[OTA] Progress: %d%%\n", progress);
          }
        }
        delay(1);
      }
      
      if (Update.end(true)) { // true to set the update as valid
        Serial.println("[OTA] Update completed successfully");
        
        // Store new version
        preferences.putString("version", newVersion);
        preferences.end();
        
        Serial.println("[OTA] Firmware updated successfully!");
        Serial.println("[OTA] Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
      } else {
        Serial.print("[OTA] ERROR: Update failed: ");
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
  Serial.println("[OTA] Update process completed");
}
