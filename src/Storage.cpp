#include "Storage.h"
#include "Config.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>

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

void deleteThemePreference() {
  if (SPIFFS.exists(THEME_FILE)) {
    SPIFFS.remove(THEME_FILE);
    logInfo("Deleted saved theme preference.");
  }
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

bool saveTideDirection(float tideHeightOneHourAgo, time_t tideDirectionTimestamp, int currentTideDirection) {
  DynamicJsonDocument doc(512);
  doc["tideHeightOneHourAgo"] = tideHeightOneHourAgo;
  doc["tideDirectionTimestamp"] = (long)tideDirectionTimestamp;
  
  // Add human-readable timestamp
  char timeStr[64];
  struct tm *timeinfo = localtime(&tideDirectionTimestamp);
  if (timeinfo != nullptr) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    doc["timestampReadable"] = String(timeStr);
  }
  
  doc["currentTideDirection"] = currentTideDirection;

  File f = SPIFFS.open(TIDE_DIRECTION_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open tide direction file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write tide direction file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved tide direction to SPIFFS.");
  return true;
}

void loadTideDirection(float &tideHeightOneHourAgo, time_t &tideDirectionTimestamp, int &currentTideDirection) {
  tideHeightOneHourAgo = 0.0f;
  tideDirectionTimestamp = 0;
  currentTideDirection = 0;

  if (!SPIFFS.exists(TIDE_DIRECTION_FILE)) {
    logInfo("No saved tide direction.");
    return;
  }

  File f = SPIFFS.open(TIDE_DIRECTION_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open tide direction file for read.");
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse tide direction file.");
    return;
  }

  tideHeightOneHourAgo = doc["tideHeightOneHourAgo"] | 0.0f;
  tideDirectionTimestamp = doc["tideDirectionTimestamp"] | 0L;
  currentTideDirection = doc["currentTideDirection"] | 0;
  
  // Log with human-readable timestamp if available
  String readableTime = doc["timestampReadable"] | String("unknown");
  logInfo(String("Loaded tide direction: ") + String(currentTideDirection) + " (last update: " + readableTime + ")");
}

void deleteTideDirection() {
  if (SPIFFS.exists(TIDE_DIRECTION_FILE)) {
    SPIFFS.remove(TIDE_DIRECTION_FILE);
    logInfo("Deleted saved tide direction.");
  }
}

// Tide bounds storage (daily min/max)
bool saveTideBounds(float minTide, float maxTide, const String &date) {
  DynamicJsonDocument doc(512);
  doc["minTide"] = minTide;
  doc["maxTide"] = maxTide;
  doc["date"] = date;

  File f = SPIFFS.open(TIDE_BOUNDS_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open tide bounds file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write tide bounds file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved tide bounds: min=" + String(minTide, 2) + "m, max=" + String(maxTide, 2) + "m, date=" + date);
  return true;
}

bool loadTideBounds(float &minTide, float &maxTide, String &date) {
  minTide = 0.0f;
  maxTide = 0.0f;
  date = "";

  if (!SPIFFS.exists(TIDE_BOUNDS_FILE)) {
    logInfo("No saved tide bounds.");
    return false;
  }

  File f = SPIFFS.open(TIDE_BOUNDS_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open tide bounds file for read.");
    return false;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse tide bounds file.");
    return false;
  }

  minTide = doc["minTide"] | 0.0f;
  maxTide = doc["maxTide"] | 0.0f;
  date = doc["date"] | String("");
  
  logInfo("Loaded tide bounds: min=" + String(minTide, 2) + "m, max=" + String(maxTide, 2) + "m, date=" + date);
  return true;
}

void deleteTideBounds() {
  if (SPIFFS.exists(TIDE_BOUNDS_FILE)) {
    SPIFFS.remove(TIDE_BOUNDS_FILE);
    logInfo("Deleted saved tide bounds.");
  }
}

// Hourly tide tracking storage
bool saveTideHourlyCheck(float startHeight, time_t startTime, int hour) {
  DynamicJsonDocument doc(512);
  doc["startHeight"] = startHeight;
  doc["startTime"] = (long)startTime;
  doc["hour"] = hour;

  char timeStr[64];
  struct tm *timeinfo = localtime(&startTime);
  if (timeinfo != nullptr) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    doc["timeReadable"] = String(timeStr);
  }

  File f = SPIFFS.open(TIDE_HOURLY_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open tide hourly file for write.");
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    logError("Failed to write tide hourly file.");
    f.close();
    return false;
  }
  f.close();
  logInfo("Saved tide hourly check to SPIFFS.");
  return true;
}

bool loadTideHourlyCheck(float &startHeight, time_t &startTime, int &hour) {
  startHeight = 0.0f;
  startTime = 0;
  hour = -1;

  if (!SPIFFS.exists(TIDE_HOURLY_FILE)) {
    logInfo("No saved tide hourly check.");
    return false;
  }

  File f = SPIFFS.open(TIDE_HOURLY_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open tide hourly file for read.");
    return false;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse tide hourly file.");
    return false;
  }

  startHeight = doc["startHeight"] | 0.0f;
  startTime = doc["startTime"] | 0L;
  hour = doc["hour"] | -1;

  String readableTime = doc["timeReadable"] | String("unknown");
  logInfo("Loaded tide hourly check: hour=" + String(hour) + ", height=" + String(startHeight, 2) + " (at: " + readableTime + ")");
  return true;
}

void deleteTideHourlyCheck() {
  if (SPIFFS.exists(TIDE_HOURLY_FILE)) {
    SPIFFS.remove(TIDE_HOURLY_FILE);
    logInfo("Deleted saved tide hourly check.");
  }
}

bool savePlayerName(const String &name) {
  DynamicJsonDocument doc(128);
  doc["name"] = name;

  File f = SPIFFS.open(PLAYER_NAME_FILE, FILE_WRITE);
  if (!f) {
    logError("Failed to open player name file for write.");
    return false;
  }
  serializeJson(doc, f);
  f.close();
  logInfo("Saved player name: " + name);
  return true;
}

String loadPlayerName() {
  if (!SPIFFS.exists(PLAYER_NAME_FILE)) return "";

  File f = SPIFFS.open(PLAYER_NAME_FILE, FILE_READ);
  if (!f) return "";

  DynamicJsonDocument doc(128);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return "";

  String name = doc["name"] | "";
  logInfo("Loaded player name: " + name);
  return name;
}
