#ifndef STORAGE_H
#define STORAGE_H

#include "Types.h"
#include <time.h>

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

// Tide direction storage
bool saveTideDirection(float tideHeightOneHourAgo, time_t tideDirectionTimestamp, int currentTideDirection);
void loadTideDirection(float &tideHeightOneHourAgo, time_t &tideDirectionTimestamp, int &currentTideDirection);
void deleteTideDirection();

// Tide bounds storage (daily min/max)
bool saveTideBounds(float minTide, float maxTide, const String &date);
bool loadTideBounds(float &minTide, float &maxTide, String &date);
void deleteTideBounds();

// Hourly tide tracking storage
bool saveTideHourlyCheck(float startHeight, time_t startTime, int hour);
bool loadTideHourlyCheck(float &startHeight, time_t &startTime, int &hour);
void deleteTideHourlyCheck();

// Player name storage
bool savePlayerName(const String &name);
String loadPlayerName();

#endif // STORAGE_H
