#ifndef STORAGE_H
#define STORAGE_H

#include "Types.h"

// Logging functions
void logInfo(const String &message);
void logError(const String &message);

// WiFi credentials storage
bool saveWifiCredentials(const WifiCredentials &creds);
WifiCredentials loadWifiCredentials();
void deleteWifiCredentials();

// Theme preference storage
bool saveThemePreference(bool isDark);
bool loadThemePreference();
void deleteThemePreference();

// Wave height preference storage
bool saveWaveHeightPreference(float threshold);
float loadWaveHeightPreference();
void deleteWaveHeightPreference();

// Surf location storage
bool saveSurfLocation(const LocationInfo &locInfo);
LocationInfo loadSurfLocationInfo();
void deleteSurfLocation();

// Tide range storage
bool saveTideRange(float minTide, float maxTide, unsigned long timestamp, const String &locationKey, bool isCalibrating);
void loadTideRange(float &minTide, float &maxTide, unsigned long &timestamp, String &locationKey, bool &isCalibrating);
void deleteTideRange();

// Tide direction storage
bool saveTideDirection(float tideHeightOneHourAgo, unsigned long tideDirectionTimestamp, int currentTideDirection);
void loadTideDirection(float &tideHeightOneHourAgo, unsigned long &tideDirectionTimestamp, int &currentTideDirection);
void deleteTideDirection();

#endif // STORAGE_H
