#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

// ---------- User config ----------
const char *WIFI_SSID = "The neighbors wifi";
const char *WIFI_PASS = "Theneighborspassword";
const char *SURF_LOCATION = "Huntington Beach, CA";
// ----------------------------------

// ---------- Pin definitions for ESP32 + ILI9341 ----------
// ESP32-2432S028R (CYD 2.8") common pin configuration
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   -1  // Use -1 if tied to ESP32 EN/reset
#define TFT_BL    27  // Backlight pin — try 21, 27, or -1
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_SCLK  14
// ---------------------------------------------------------

static const uint32_t REFRESH_INTERVAL_MS = 1800000; // 30 minutes
static const char *GEOCODE_URL =
    "https://geocoding-api.open-meteo.com/v1/search";
static const char *MARINE_URL = "https://marine-api.open-meteo.com/v1/marine";

// Create display bus and GFX objects
// 3.5" 480x320 display - using ILI9488 driver (common for 3.5" CYD)
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI,
                                            TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9488_18bit(bus, TFT_RST, 0 /* rotation */,
                                             false /* IPS */);

struct LocationInfo {
  float latitude = 0.0f;
  float longitude = 0.0f;
  String displayName = "";
  bool valid = false;
};

struct SurfForecast {
  float waveHeight = 0.0f;
  float wavePeriod = 0.0f;
  float waveDirection = 0.0f;
  String timeLabel = "";
  bool valid = false;
};

LocationInfo cachedLocation;
String wifiSsidInput;
String wifiPassInput;

void logInfo(const String &message) {
  Serial.printf("[INFO  %10lu ms] %s\n", millis(), message.c_str());
}

void logError(const String &message) {
  Serial.printf("[ERROR %10lu ms] %s\n", millis(), message.c_str());
}

void showStatus(const String &line1, const String &line2 = "",
                uint16_t color = WHITE) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(color);
  gfx->setTextSize(3);
  gfx->setCursor(20, 40);
  gfx->println(line1);
  if (!line2.isEmpty()) {
    gfx->setCursor(20, 90);
    gfx->println(line2);
  }
}

String readLineFromSerial(uint32_t timeoutMs) {
  String input;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (Serial.available() > 0) {
      char c = static_cast<char>(Serial.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        input.trim();
        return input;
      }
      input += c;
    }
    delay(10);
  }
  input.trim();
  return input;
}

void promptForWifi() {
  Serial.println();
  Serial.println("Enter Wi-Fi SSID (press Enter to keep default):");
  showStatus("Wi-Fi setup", "Enter SSID", CYAN);
  String ssid = readLineFromSerial(20000);
  if (!ssid.isEmpty()) {
    wifiSsidInput = ssid;
  }

  Serial.println("Enter Wi-Fi password (press Enter to keep default):");
  showStatus("Wi-Fi setup", "Enter password", CYAN);
  String pass = readLineFromSerial(20000);
  if (!pass.isEmpty()) {
    wifiPassInput = pass;
  }
}

void connectWifi() {
  const char *ssid = wifiSsidInput.isEmpty() ? WIFI_SSID : wifiSsidInput.c_str();
  const char *pass = wifiPassInput.isEmpty() ? WIFI_PASS : wifiPassInput.c_str();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  showStatus("Connecting to Wi-Fi", ssid, CYAN);
  logInfo("Connecting to Wi-Fi SSID: " + String(ssid));
  uint8_t retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    ++retries;
    if (retries % 10 == 0) {
      logInfo("Still connecting... attempt " + String(retries));
      showStatus("Connecting...", "Attempt " + String(retries), CYAN);
    }
    if (retries >= 60) {
      logError("Wi-Fi connection failed after 60 attempts.");
      showStatus("Wi-Fi connection failed", "Check credentials", RED);
      return;
    }
  }
  logInfo("Wi-Fi connected. IP: " + WiFi.localIP().toString());
  showStatus("Wi-Fi connected", WiFi.localIP().toString(), GREEN);
}

String urlEncode(const String &value) {
  String encoded;
  encoded.reserve(value.length());
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ||
        c == '.' || c == '~') {
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

LocationInfo fetchLocation(const String &location) {
  LocationInfo info;
  if (WiFi.status() != WL_CONNECTED) {
    logError("Wi-Fi not connected.");
    return info;
  }

  HTTPClient http;
  String url = String(GEOCODE_URL) + "?name=" + urlEncode(location) +
               "&count=1&language=en&format=json";
  logInfo("Geocoding location " + location + " via " + url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "DadSurfChecker/1.0");
  http.addHeader("Accept-Encoding", "identity");
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    logError("HTTP error code: " + String(code));
    http.end();
    return info;
  }

  const size_t capacity = 12 * 1024;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  DynamicJsonDocument doc(capacity);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  String payload = http.getString();
  http.end();
  if (payload.isEmpty()) {
    logError("Empty response payload.");
    return info;
  }

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    String preview = payload.substring(0, 120);
    logError("JSON parse failed: " + String(err.c_str()));
    logError("Response preview: " + preview);
    return info;
  }

  JsonObject first = doc["results"][0];
  if (first.isNull()) {
    logError("No geocoding results.");
    return info;
  }

  info.latitude = first["latitude"] | 0.0f;
  info.longitude = first["longitude"] | 0.0f;
  String name = first["name"] | "";
  String admin1 = first["admin1"] | "";
  String country = first["country"] | "";
  info.displayName = name;
  if (!admin1.isEmpty()) {
    info.displayName += ", " + admin1;
  }
  if (!country.isEmpty()) {
    info.displayName += ", " + country;
  }
  info.valid = true;
  logInfo("Geocoded location to " + info.displayName + " (" +
          String(info.latitude, 4) + ", " + String(info.longitude, 4) +
          ")");
  return info;
}

SurfForecast fetchSurfForecast(float latitude, float longitude) {
  SurfForecast forecast;
  if (WiFi.status() != WL_CONNECTED) {
    logError("Wi-Fi not connected.");
    return forecast;
  }

  HTTPClient http;
  String url = String(MARINE_URL) + "?latitude=" + String(latitude, 4) +
               "&longitude=" + String(longitude, 4) +
               "&hourly=wave_height,wave_period,wave_direction&timezone=auto";
  logInfo("Requesting surf forecast via " + url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "DadSurfChecker/1.0");
  http.addHeader("Accept-Encoding", "identity");
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    logError("HTTP error code: " + String(code));
    http.end();
    return forecast;
  }

  const size_t capacity = 24 * 1024;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  DynamicJsonDocument doc(capacity);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  String payload = http.getString();
  http.end();
  if (payload.isEmpty()) {
    logError("Empty response payload.");
    return forecast;
  }

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    String preview = payload.substring(0, 120);
    logError("JSON parse failed: " + String(err.c_str()));
    logError("Response preview: " + preview);
    return forecast;
  }

  JsonArray times = doc["hourly"]["time"];
  JsonArray heights = doc["hourly"]["wave_height"];
  JsonArray periods = doc["hourly"]["wave_period"];
  JsonArray directions = doc["hourly"]["wave_direction"];
  if (times.isNull() || heights.isNull() || periods.isNull() ||
      directions.isNull() || times.size() == 0) {
    logError("Missing hourly forecast data.");
    return forecast;
  }

  forecast.timeLabel = String(times[0].as<const char *>());
  forecast.waveHeight = heights[0] | 0.0f;
  forecast.wavePeriod = periods[0] | 0.0f;
  forecast.waveDirection = directions[0] | 0.0f;
  forecast.valid = true;
  logInfo("Forecast waveHeight=" + String(forecast.waveHeight, 2) +
          "m period=" + String(forecast.wavePeriod, 1) +
          "s direction=" + String(forecast.waveDirection, 0));
  return forecast;
}

void drawForecast(const LocationInfo &location, const SurfForecast &forecast) {
  gfx->fillScreen(BLACK);

  int16_t w = gfx->width();   // 480 in landscape
  int16_t h = gfx->height();  // 320 in landscape

  // Location (top left)
  gfx->setTextColor(CYAN);
  gfx->setTextSize(3);
  gfx->setCursor(20, 20);
  gfx->println("Surf spot");
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 55);
  gfx->println(location.displayName);

  // Wave height (center, large)
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(3);
  gfx->setCursor(20, 130);
  gfx->println("Wave height");
  gfx->setTextColor(WHITE);
  gfx->setTextSize(5);
  gfx->setCursor(20, 170);
  String heightStr = String(forecast.waveHeight, 2) + " m";
  gfx->println(heightStr);

  // Details (bottom)
  bool happy = forecast.waveHeight >= 1.0f;
  uint16_t accent = happy ? GREEN : RED;
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 250);
  gfx->println("Period / Direction");
  gfx->setTextColor(accent);
  gfx->setTextSize(4);
  gfx->setCursor(20, 285);
  String detailStr = String(forecast.wavePeriod, 1) + "s ";
  detailStr += String(forecast.waveDirection, 0) + (char)248 + " ";
  detailStr += forecast.timeLabel.substring(11, 16);
  gfx->println(detailStr);

  // Large smiley (right side)
  gfx->setTextSize(8);
  gfx->setCursor(w - 120, 100);
  gfx->println(happy ? ":)" : ":(");
}

void showError(const String &message) {
  logError(message);
  showStatus("Error:", message, RED);
}

void setupDisplay() {
  logInfo("Display init starting...");

  // Initialize backlight
#if TFT_BL >= 0
  logInfo("Backlight pin TFT_BL=" + String(TFT_BL));
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  logInfo("Backlight set HIGH");
#endif

  if (!gfx->begin()) {
    logError("gfx->begin() failed!");
    return;
  }
  logInfo("Display init done. Width=" + String(gfx->width()) +
          " Height=" + String(gfx->height()));

  gfx->setRotation(1); // landscape
  logInfo("Rotation set to 1 (landscape). Width=" + String(gfx->width()) +
          " Height=" + String(gfx->height()));

  // ---------- Diagnostic color cycle ----------
  logInfo("Running display color diagnostics...");
  const uint16_t testColors[] = {RED, GREEN, BLUE, WHITE, BLACK};
  const char *colorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
  for (int i = 0; i < 5; ++i) {
    gfx->fillScreen(testColors[i]);
    logInfo(String("Screen filled: ") + colorNames[i]);
    delay(400);
  }
  logInfo("Color diagnostics done.");
  // --------------------------------------------

  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->println("Booting...");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  setupDisplay();
  promptForWifi();
  connectWifi();
}

void loop() {
  showStatus("Finding spot", SURF_LOCATION, CYAN);
  if (!cachedLocation.valid) {
    cachedLocation = fetchLocation(SURF_LOCATION);
  }
  if (!cachedLocation.valid) {
    showError("Location lookup failed.");
    delay(REFRESH_INTERVAL_MS);
    return;
  }

  showStatus("Fetching surf", cachedLocation.displayName, CYAN);
  SurfForecast forecast =
      fetchSurfForecast(cachedLocation.latitude, cachedLocation.longitude);
  if (forecast.valid) {
    drawForecast(cachedLocation, forecast);
  } else {
    showError("Fetch failed; retrying.");
  }
  delay(REFRESH_INTERVAL_MS);
}
