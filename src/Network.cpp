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

SurfForecast fetchSurfForecast(float latitude, float longitude) {
  SurfForecast forecast;
  if (WiFi.status() != WL_CONNECTED) return forecast;

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
  forecast.valid = true;
  return forecast;
}
