#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

// =====================
// WiFi credentials
// =====================
const char* ssid = "INPT-Test";
const char* password = "iinnpptt";

// =====================
// Firmware version - MUST be 1.0.0 for initial release
// =====================
const char* currentFirmwareVersion = "1.0.0";

// =====================
// GitHub URLs
// =====================
const char* versionUrl = "https://raw.githubusercontent.com/Nasreddiine/esp32_firmware/main/version.txt";
const char* firmwareUrl = "https://github.com/Nasreddiine/esp32_firmware/releases/latest/download/firmware.bin";

// =====================
// GitHub Root Certificate
// =====================
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STGadaX0ZdAsSvG4Lxw\n" \
"9qTRkXv1IbzLxU0XZspN7qQaGc6L2RlZcFZl+5K7b5I1Q8K7Y5U5L5Uwv0Zg1Mp\n" \
"lFfG6raB8p6N7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dD\n" \
"nVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5\n" \
"L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6\n" \
"-----END CERTIFICATE-----\n";

Preferences preferences;
WiFiClientSecure client;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 2 * 60 * 1000; // 2 minutes

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32 Auto-OTA Firmware System");
  Serial.println("=================================");
  Serial.println("Version: " + String(currentFirmwareVersion));

  // Set root certificate for GitHub
  client.setCACert(rootCACertificate);

  preferences.begin("firmware", false);
  String storedVersion = preferences.getString("version", "1.0.0");
  Serial.println("Stored version: " + storedVersion);
  Serial.println("Current version: " + String(currentFirmwareVersion));

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
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.println("Connecting to WiFi: " + String(ssid));
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ WiFi connection failed");
  }
}

String checkVersionFromGitHub() {
  HTTPClient http;
  
  http.begin(versionUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();
    http.end();
    return latestVersion;
  } else {
    Serial.printf("[OTA] Version check failed: %d\n", httpCode);
    http.end();
    return "";
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
  
  String latestVersion = checkVersionFromGitHub();
  
  if (latestVersion.length() > 0) {
    Serial.println("[OTA] Current version: " + String(currentFirmwareVersion));
    Serial.println("[OTA] Latest version : " + latestVersion);

    if (latestVersion != currentFirmwareVersion) {
      Serial.println("[OTA] >>> New firmware AVAILABLE! Starting update...");
      performOTA(latestVersion);
    } else {
      Serial.println("[OTA] Firmware is up to date");
    }
  } else {
    Serial.println("[OTA] Version check failed");
  }
}

void performOTA(String newVersion) {
  Serial.println("[OTA] Starting firmware update to version: " + newVersion);
  Serial.println("[OTA] Download URL: " + String(firmwareUrl));

  HTTPClient http;
  http.begin(client, firmwareUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(30000);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
    
    if (Update.begin(contentLength)) {
      Serial.println("[OTA] Starting update process...");
      
      WiFiClient* stream = http.getStreamPtr();
      uint8_t buffer[256];
      size_t written = 0;
      
      while (http.connected() && (written < contentLength)) {
        size_t available = stream->available();
        if (available > 0) {
          int toRead = min(available, sizeof(buffer));
          int bytesRead = stream->readBytes(buffer, toRead);
          
          if (Update.write(buffer, bytesRead) != bytesRead) {
            Serial.println("[OTA] ERROR: Write failed");
            break;
          }
          
          written += bytesRead;
          
          // Show progress
          if (written % 8192 == 0) {
            Serial.printf("[OTA] Progress: %d/%d bytes\n", written, contentLength);
          }
        }
        delay(1);
      }
      
      if (Update.end()) {
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
}
