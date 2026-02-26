#include "Network.h"
#include "Config.h"
#include "Storage.h"
#include "Theme.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx;
extern Theme currentTheme;

// Forward declaration
void showStatus(const String &line1, const String &line2, uint16_t color);

String urlEncode(const String &value) {
  String encoded;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

bool connectWifi(const WifiCredentials &creds) {
  if (!creds.valid) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
  showStatus("Connecting Wi-Fi", creds.ssid, currentTheme.textSecondary);

  for (uint8_t attempt = 0; attempt < 60; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      showStatus("Wi-Fi connected", WiFi.localIP().toString(), currentTheme.success);
      logInfo("Connected to Wi-Fi " + creds.ssid);
      delay(1000);
      return true;
    }
    delay(500);
  }

  logError("Wi-Fi connection failed for SSID: " + creds.ssid);
  showStatus("Wi-Fi failed", "Tap to re-enter", currentTheme.error);
  return false;
}

std::vector<LocationInfo> fetchLocationMatches(const String &location, int maxResults) {
  std::vector<LocationInfo> matches;
  if (WiFi.status() != WL_CONNECTED) return matches;

  HTTPClient http;
  String url = String(GEOCODE_URL) + "?name=" + urlEncode(location) + "&count=" + String(maxResults) + "&language=en&format=json";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return matches;
  }

  DynamicJsonDocument doc(12 * 1024);
  String payload = http.getString();
  http.end();
  if (deserializeJson(doc, payload)) return matches;

  JsonArray results = doc["results"];
  if (results.isNull()) return matches;

  for (JsonObject result : results) {
    LocationInfo info;
    info.latitude = result["latitude"] | 0.0f;
    info.longitude = result["longitude"] | 0.0f;
    info.displayName = String((const char *)result["name"]);
    String admin1 = result["admin1"] | "";
    String country = result["country"] | "";
    if (!admin1.isEmpty()) info.displayName += ", " + admin1;
    if (!country.isEmpty()) info.displayName += ", " + country;
    info.valid = true;
    matches.push_back(info);
  }
  return matches;
}

LocationInfo fetchLocation(const String &location) {
  LocationInfo info;
  auto matches = fetchLocationMatches(location, 1);
  if (!matches.empty()) return matches[0];
  return info;
}

// Find the nearest NOAA tide station to given coordinates
String findNearestTideStation(float latitude, float longitude) {
  if (WiFi.status() != WL_CONNECTED) return "";
  
  logInfo("Finding NOAA station for coordinates: lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6));
  
  // Hardcoded stations for common locations (avoids large API response)
  // Mayport, FL area (30.397, -81.4276)
  if (abs(latitude - 30.397) < 0.1 && abs(longitude - (-81.428)) < 0.1) {
    logInfo("Using hardcoded Mayport station: 8720218");
    return "8720218"; // MAYPORT (BAR PILOT DOCK)
  }
  
  // Jacksonville Beach area (30.3268, -81.3836)
  if (abs(latitude - 30.327) < 0.1 && abs(longitude - (-81.384)) < 0.1) {
    logInfo("Using hardcoded Jacksonville Beach station: 8720218");
    return "8720218"; // MAYPORT (BAR PILOT DOCK) - closest to Jax Beach
  }
  
  // For other locations, we could implement a region-based lookup
  // For now, return a default Florida East Coast station if in that region
  if (latitude > 25.0 && latitude < 31.0 && longitude > -82.0 && longitude < -80.0) {
    logInfo("Using default Florida East Coast station: 8720218");
    return "8720218";
  }
  
  logError("No NOAA station mapped for this location");
  return "";
}

// Fetch current tide height from NOAA station
float fetchNOAATideHeight(const String &stationId, float &minTide, float &maxTide) {
  if (WiFi.status() != WL_CONNECTED || stationId.isEmpty()) {
    logError("fetchNOAATideHeight: WiFi not connected or stationId empty");
    minTide = 0.0f;
    maxTide = 0.0f;
    return 0.0f;
  }
  
  // Check if NTP time sync has completed, retry if not
  time_t now = time(nullptr);
  if (now < 1000000000) {
    logError("fetchNOAATideHeight: NTP time not synced (time=" + String(now) + "), retrying...");
    // Try syncing again
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    for (int i = 0; i < 30; i++) {
      delay(100);
      now = time(nullptr);
      if (now >= 1000000000) {
        logInfo("NTP sync succeeded on retry: " + String(now));
        break;
      }
    }
    
    // If still not synced, fail
    if (now < 1000000000) {
      logError("fetchNOAATideHeight: NTP sync failed even after retry (time=" + String(now) + ")");
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
  }
  
  // Get current date in YYYYMMDD format
  struct tm *timeinfo = localtime(&now);
  char dateStr[16];
  strftime(dateStr, sizeof(dateStr), "%Y%m%d", timeinfo);
  String currentDate = String(dateStr);
  
  // Check if we have cached tide bounds for today
  String cachedDate = "";
  bool hasCachedBounds = loadTideBounds(minTide, maxTide, cachedDate);
  
  // Invalidate cached bounds if they're invalid (both 0) or from a different day
  if (hasCachedBounds && (cachedDate != currentDate || (minTide == 0.0f && maxTide == 0.0f))) {
    logInfo("Cached tide bounds invalid or from different day, refetching");
    hasCachedBounds = false;
  }
  
  // If we don't have today's valid bounds cached, fetch from API
  if (!hasCachedBounds) {
    logInfo("Fetching NOAA tides for station " + stationId + " for today: " + currentDate);
    
    HTTPClient http;
    // Fetch only today's predictions (begin_date = end_date = today)
    String url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&station=" + stationId + 
                 "&datum=MLLW&time_zone=lst_ldt&units=english&interval=h&begin_date=" + currentDate + 
                 "&end_date=" + currentDate + "&format=json";
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      logError("Failed to fetch NOAA tide data: HTTP " + String(code) + " for URL: " + url);
      http.end();
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    String payload = http.getString();
    http.end();
    
    logInfo("NOAA API response (first 200 chars): " + payload.substring(0, min(200, (int)payload.length())));
    
    DynamicJsonDocument doc(8 * 1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      logError("Failed to parse NOAA tide JSON: " + String(error.c_str()));
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    JsonArray predictions = doc["predictions"];
    if (predictions.isNull() || predictions.size() == 0) {
      // Check if there's an error message in the response
      if (doc.containsKey("error")) {
        logError("NOAA API error: " + String((const char*)doc["error"]["message"]));
      } else {
        logError("No tide predictions in NOAA response (array size: " + String(predictions.size()) + ")");
      }
      minTide = 0.0f;
      maxTide = 0.0f;
      return 0.0f;
    }
    
    logInfo("NOAA returned " + String(predictions.size()) + " tide predictions for today");
    
    // Get the first prediction (most recent/current)
    // "v" is a string in the JSON, need to convert to float
    String currentTideStr = predictions[0]["v"] | "0.0";
    float currentTideHeight = currentTideStr.toFloat();
    
    // Find min and max from all predictions for today
    float minTideFeet = currentTideHeight;
    float maxTideFeet = currentTideHeight;
    
    for (JsonVariant pred : predictions) {
      String heightStr = pred["v"] | "0.0";
      float height = heightStr.toFloat();
      if (height < minTideFeet) minTideFeet = height;
      if (height > maxTideFeet) maxTideFeet = height;
    }
    
    // Convert from feet to meters for consistency with rest of app
    float currentTideMeters = currentTideHeight * 0.3048f;
    minTide = minTideFeet * 0.3048f;
    maxTide = maxTideFeet * 0.3048f;
    
    // Save today's bounds to file for reuse
    saveTideBounds(minTide, maxTide, currentDate);
    
    logInfo("NOAA tide - Current: " + String(currentTideHeight, 2) + " ft (" + String(currentTideMeters, 2) + " m), " +
            "Daily Range: " + String(minTideFeet, 2) + " to " + String(maxTideFeet, 2) + " ft (" + 
            String(minTide, 2) + " to " + String(maxTide, 2) + " m)");
    
    return currentTideMeters;
  } else {
    // We have today's bounds cached, just fetch current tide height
    logInfo("Using cached tide bounds for " + currentDate + ": " + String(minTide, 2) + "m to " + String(maxTide, 2) + "m");
    
    HTTPClient http;
    // Fetch current tide prediction only (for the next hour)
    String url = "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&station=" + stationId + 
                 "&datum=MLLW&time_zone=lst_ldt&units=english&interval=h&begin_date=" + currentDate + 
                 "&end_date=" + currentDate + "&format=json&range=1";
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      logError("Failed to fetch current NOAA tide: HTTP " + String(code));
      http.end();
      return 0.0f;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      logError("Failed to parse NOAA tide JSON: " + String(error.c_str()));
      return 0.0f;
    }
    
    JsonArray predictions = doc["predictions"];
    if (predictions.isNull() || predictions.size() == 0) {
      logError("No current tide prediction in NOAA response");
      return 0.0f;
    }
    
    String currentTideStr = predictions[0]["v"] | "0.0";
    float currentTideHeight = currentTideStr.toFloat();
    float currentTideMeters = currentTideHeight * 0.3048f;
    
    logInfo("Current tide: " + String(currentTideHeight, 2) + " ft (" + String(currentTideMeters, 2) + " m)");
    
    return currentTideMeters;
  }
}

SurfForecast fetchSurfForecast(float latitude, float longitude) {
  SurfForecast forecast;
  if (WiFi.status() != WL_CONNECTED) return forecast;

  // Fetch wave data from Marine API
  HTTPClient http;
  String url = String(MARINE_URL) + "?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) + "&hourly=wave_height,wave_period,wave_direction&timezone=auto";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return forecast;
  }

  DynamicJsonDocument doc(24 * 1024);
  String payload = http.getString();
  http.end();
  if (deserializeJson(doc, payload)) return forecast;

  JsonArray times = doc["hourly"]["time"];
  JsonArray heights = doc["hourly"]["wave_height"];
  JsonArray periods = doc["hourly"]["wave_period"];
  JsonArray directions = doc["hourly"]["wave_direction"];
  if (times.isNull() || heights.isNull() || periods.isNull() || directions.isNull() || times.size() == 0) {
    return forecast;
  }

  forecast.timeLabel = String(times[0].as<const char *>());
  forecast.waveHeight = heights[0] | 0.0f;
  forecast.wavePeriod = periods[0] | 0.0f;
  forecast.waveDirection = directions[0] | 0.0f;

  // Fetch wind data from Weather Forecast API
  url = String(WEATHER_URL) + "?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) + "&hourly=wind_speed_10m,wind_direction_10m&wind_speed_unit=mph&timezone=auto";
  http.begin(url);
  code = http.GET();
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
    http.end();
    DynamicJsonDocument windDoc(12 * 1024);
    if (deserializeJson(windDoc, payload) == DeserializationError::Ok) {
      JsonArray windSpeeds = windDoc["hourly"]["wind_speed_10m"];
      JsonArray windDirections = windDoc["hourly"]["wind_direction_10m"];
      if (!windSpeeds.isNull() && !windDirections.isNull() && windSpeeds.size() > 0) {
        forecast.windSpeed = windSpeeds[0] | 0.0f;
        forecast.windDirection = windDirections[0] | 0.0f;
      }
    }
  } else {
    http.end();
  }
  
  // Fetch tide data from NOAA
  // Use cached station ID if available, otherwise find nearest
  static String cachedStationId = "";
  static float cachedLat = 0.0f;
  static float cachedLon = 0.0f;
  
  // Check if we need to find a new station (location changed significantly)
  if (cachedStationId.isEmpty() || abs(latitude - cachedLat) > 0.5f || abs(longitude - cachedLon) > 0.5f) {
    logInfo("Finding nearest NOAA tide station for lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6));
    cachedStationId = findNearestTideStation(latitude, longitude);
    cachedLat = latitude;
    cachedLon = longitude;
  } else {
    logInfo("Using cached NOAA station: " + cachedStationId + " (lat=" + String(latitude, 6) + ", lon=" + String(longitude, 6) + ")");
  }
  
  // Fetch tide height from NOAA
  if (!cachedStationId.isEmpty()) {
    forecast.tideHeight = fetchNOAATideHeight(cachedStationId, forecast.minTide, forecast.maxTide);
  } else {
    logError("No NOAA tide station found, using default tide height");
    forecast.tideHeight = 0.0f;
    forecast.minTide = 0.0f;
    forecast.maxTide = 0.0f;
  }

  forecast.valid = true;
  return forecast;
}
