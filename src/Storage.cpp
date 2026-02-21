#include "Storage.h"
#include "Config.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

void logInfo(const String &message) { 
  Serial.printf("[INFO  %10lu ms] %s\n", millis(), message.c_str()); 
}

void logError(const String &message) { 
  Serial.printf("[ERROR %10lu ms] %s\n", millis(), message.c_str()); 
}

bool saveWifiCredentials(const WifiCredentials &creds) {
  DynamicJsonDocument doc(512);
  doc["ssid"] = creds.ssid;
  doc["password"] = creds.password;

  File f = SPIFFS.open(WIFI_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open wifi file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write wifi file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved Wi-Fi credentials to SPIFFS.");
  return true;
}

WifiCredentials loadWifiCredentials() {
  WifiCredentials creds;
  if (!SPIFFS.exists(WIFI_FILE)) {
    logInfo("No saved Wi-Fi credentials file.");
    return creds;
  }

  File f = SPIFFS.open(WIFI_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open wifi file for read.");
    return creds;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse wifi file.");
    return creds;
  }

  creds.ssid = doc["ssid"] | "";
  creds.password = doc["password"] | "";
  creds.valid = !creds.ssid.isEmpty();
  if (creds.valid) logInfo("Loaded saved Wi-Fi credentials.");
  return creds;
}

void deleteWifiCredentials() {
  if (SPIFFS.exists(WIFI_FILE)) {
    SPIFFS.remove(WIFI_FILE);
    logInfo("Deleted saved Wi-Fi credentials.");
  }
}

bool saveThemePreference(bool isDark) {
  DynamicJsonDocument doc(256);
  doc["darkMode"] = isDark;

  File f = SPIFFS.open(THEME_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open theme file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write theme file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved theme preference to SPIFFS.");
  return true;
}

bool loadThemePreference() {
  if (!SPIFFS.exists(THEME_FILE)) {
    logInfo("No saved theme preference.");
    return true;  // Default to dark mode
  }

  File f = SPIFFS.open(THEME_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open theme file for read.");
    return true;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse theme file.");
    return true;
  }

  bool isDark = doc["darkMode"] | true;
  logInfo(String("Loaded theme: ") + (isDark ? "dark" : "light"));
  return isDark;
}

bool saveWaveHeightPreference(float threshold) {
  DynamicJsonDocument doc(256);
  doc["threshold"] = threshold;

  File f = SPIFFS.open(WAVE_PREF_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open wave pref file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write wave pref file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved wave height preference to SPIFFS.");
  return true;
}

float loadWaveHeightPreference() {
  if (!SPIFFS.exists(WAVE_PREF_FILE)) {
    logInfo("No saved wave height preference.");
    return 1.0f;  // Default
  }

  File f = SPIFFS.open(WAVE_PREF_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open wave pref file for read.");
    return 1.0f;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse wave pref file.");
    return 1.0f;
  }

  float threshold = doc["threshold"] | 1.0f;
  logInfo(String("Loaded wave threshold: ") + String(threshold, 1));
  return threshold;
}

void deleteWaveHeightPreference() {
  if (SPIFFS.exists(WAVE_PREF_FILE)) {
    SPIFFS.remove(WAVE_PREF_FILE);
    logInfo("Deleted saved wave height preference.");
  }
}

bool saveSurfLocation(const LocationInfo &locInfo) {
  if (!locInfo.valid) return false;
  
  DynamicJsonDocument doc(512);
  doc["location"] = locInfo.displayName;
  doc["latitude"] = locInfo.latitude;
  doc["longitude"] = locInfo.longitude;

  File f = SPIFFS.open(LOCATION_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open location file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write location file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved surf location to SPIFFS.");
  return true;
}

LocationInfo loadSurfLocationInfo() {
  LocationInfo info;
  if (!SPIFFS.exists(LOCATION_FILE)) {
    logInfo("No saved location file.");
    return info;
  }

  File f = SPIFFS.open(LOCATION_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open location file for read.");
    return info;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse location file.");
    return info;
  }

  info.displayName = doc["location"] | "";
  info.latitude = doc["latitude"] | 0.0f;
  info.longitude = doc["longitude"] | 0.0f;
  info.valid = !info.displayName.isEmpty() && (info.latitude != 0.0f || info.longitude != 0.0f);
  
  if (info.valid) {
    logInfo("Loaded location: " + info.displayName);
  }
  return info;
}

void deleteSurfLocation() {
  if (SPIFFS.exists(LOCATION_FILE)) {
    SPIFFS.remove(LOCATION_FILE);
    logInfo("Deleted saved surf location.");
  }
}
