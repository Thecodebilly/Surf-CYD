#ifndef NETWORK_H
#define NETWORK_H

#include "Types.h"
#include <vector>

// URL encoding utility
String urlEncode(const String &value);

// WiFi connection
bool connectWifi(const WifiCredentials &creds);

// Location API
std::vector<LocationInfo> fetchLocationMatches(const String &location, int maxResults = 10);
LocationInfo fetchLocation(const String &location);

// Marine forecast API
SurfForecast fetchSurfForecast(float latitude, float longitude);

// NOAA Tide functions
String findNearestTideStation(float latitude, float longitude);
float fetchNOAATideHeight(const String &stationId, float &minTide, float &maxTide);
void clearTideStationCache();

#endif // NETWORK_H
