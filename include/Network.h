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

#endif // NETWORK_H
