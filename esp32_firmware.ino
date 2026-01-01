#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

// =====================
// WiFi credentials - UPDATED
// =====================
const char* ssid = "INPT-Test";
const char* password = "iinnpptt";

// =====================
// Firmware version
// =====================
const char* currentFirmwareVersion = "1.0.0";

// =====================
// GitHub URLs
// =====================
const char* versionUrl = "https://raw.githubusercontent.com/Nasreddiine/esp32_firmware/main/version.txt";
const char* firmwareUrl = "https://github.com/Nasreddiine/esp32_firmware/releases/download/latest/firmware.bin";

// =====================
// GitHub Root Certificate (valid until 2031)
// =====================
const char* githubRootCACertificate = R"(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STGadaX0ZdAsSvG4Lxw
9qTRkXv1IbzLxU0XZspN7qQaGc6L2RlZcFZl+5K7b5I1Q8K7Y5U5L5Uwv0Zg1Mp
lFfG6raB8p6N7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dD
nVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnV
k5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5
L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2
k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6K7Q+1dDnVk5L2k6
-----END CERTIFICATE-----
)";

// =====================
// Timing
// =====================
const unsigned long checkInterval = 2 * 60 * 1000; // 2 minutes
unsigned long lastCheckTime = 0;
bool updateInProgress = false;

Preferences preferences;
WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32 Auto-OTA Firmware System");
  Serial.println("=================================");

  // Set root certificate for GitHub
  client.setCACert(githubRootCACertificate);

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
  
  Serial.println("Connecting to WiFi: INPT-Test");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected to INPT-Test");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ WiFi connection failed to INPT-Test");
  }
}

String checkVersionFromGitHub() {
  if (!client.connect("raw.githubusercontent.com", 443)) {
    Serial.println("[OTA] Connection to version server failed");
    return "";
  }

  String request = String("GET ") + "/Nasreddiine/esp32_firmware/main/version.txt HTTP/1.1\r\n" +
                  "Host: raw.githubusercontent.com\r\n" +
                  "Connection: close\r\n\r\n";
  
  client.print(request);

  // Wait for response
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[OTA] Version check timeout");
      client.stop();
      return "";
    }
  }

  // Read response
  String response = "";
  while (client.available()) {
    response += client.readStringUntil('\n');
  }

  client.stop();

  // Extract version from response
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart == -1) return "";
  
  String version = response.substring(bodyStart + 4);
  version.trim();
  
  return version;
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
  updateInProgress = true;
  
  Serial.println("[OTA] Starting firmware update to version: " + newVersion);
  Serial.println("[OTA] Download URL: " + String(firmwareUrl));

  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);
  
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Update failed. Error (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[OTA] No updates available");
      break;
      
    case HTTP_UPDATE_OK:
      Serial.println("[OTA] Update completed successfully");
      
      // Store new version before restart
      preferences.putString("version", newVersion);
      preferences.end();
      
      Serial.println("[OTA] Firmware updated successfully!");
      Serial.println("[OTA] Restarting...");
      ESP.restart();
      break;
  }
  
  updateInProgress = false;
}
