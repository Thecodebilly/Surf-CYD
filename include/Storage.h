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

// Wave height preference storage
bool saveWaveHeightPreference(float threshold);
float loadWaveHeightPreference();
void deleteWaveHeightPreference();

// Surf location storage
bool saveSurfLocation(const LocationInfo &locInfo);
LocationInfo loadSurfLocationInfo();
void deleteSurfLocation();

#endif // STORAGE_H
