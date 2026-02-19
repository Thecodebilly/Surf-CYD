#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

const char *DEFAULT_SURF_LOCATION = "Huntington Beach, CA";
const char *WIFI_FILE = "/wifi.json";
const char *LOCATION_FILE = "/location.json";

#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14

#define TOUCH_CS 33
#define TOUCH_IRQ 36

// Touch calibration values for CYD displays (may need minor tuning per panel)
#define TOUCH_MIN_X 240
#define TOUCH_MAX_X 3860
#define TOUCH_MIN_Y 280
#define TOUCH_MAX_Y 3860

static const uint32_t REFRESH_INTERVAL_MS = 1800000; // 30 minutes
static const char *GEOCODE_URL = "https://geocoding-api.open-meteo.com/v1/search";
static const char *MARINE_URL = "https://marine-api.open-meteo.com/v1/marine";

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ST7796(bus, TFT_RST, 1 /* rotation */, true /* IPS */, 320, 480, 0, 0, 0, 0);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

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

struct WifiCredentials {
  String ssid;
  String password;
  bool valid = false;
};

struct TouchPoint {
  int16_t x = -1;
  int16_t y = -1;
  bool pressed = false;
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

LocationInfo cachedLocation;
WifiCredentials wifiCredentials;
Rect forgetButton = {0, 0, 0, 0};
Rect forgetLocationButton = {0, 0, 0, 0};
String surfLocation = DEFAULT_SURF_LOCATION;

void logInfo(const String &message) { Serial.printf("[INFO  %10lu ms] %s\n", millis(), message.c_str()); }
void logError(const String &message) { Serial.printf("[ERROR %10lu ms] %s\n", millis(), message.c_str()); }

bool pointInRect(int16_t x, int16_t y, const Rect &r) {
  return x >= r.x && y >= r.y && x < (r.x + r.w) && y < (r.y + r.h);
}

TouchPoint getTouchPoint() {
  TouchPoint p;
  if (!touch.touched()) return p;

  TS_Point raw = touch.getPoint();
  p.x = map(raw.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, gfx->width());
  p.y = map(raw.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, gfx->height());
  p.x = constrain(p.x, 0, gfx->width() - 1);
  p.y = constrain(p.y, 0, gfx->height() - 1);
  p.pressed = true;
  return p;
}

void drawButton(const Rect &r, const String &label, uint16_t bg, uint16_t fg = BLACK, uint8_t textSize = 2) {
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
  gfx->drawRoundRect(r.x, r.y, r.w, r.h, 6, WHITE);
  gfx->setTextColor(fg);
  gfx->setTextSize(textSize);
  int16_t tx = r.x + 8;
  int16_t ty = r.y + (r.h / 2) - 8;
  gfx->setCursor(tx, ty);
  gfx->println(label);
}

void showStatus(const String &line1, const String &line2 = "", uint16_t color = WHITE) {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(12, 24);
  gfx->println(line1);
  if (!line2.isEmpty()) {
    gfx->setTextColor(WHITE);
    gfx->setCursor(12, 56);
    gfx->println(line2);
  }
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
  wifiCredentials = WifiCredentials();
}

bool saveSurfLocation(const String &location) {
  DynamicJsonDocument doc(256);
  doc["location"] = location;

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

String loadSurfLocation() {
  if (!SPIFFS.exists(LOCATION_FILE)) {
    logInfo("No saved location file.");
    return "";
  }

  File f = SPIFFS.open(LOCATION_FILE, FILE_READ);
  if (!f) {
    logError("Failed to open location file for read.");
    return "";
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logError("Failed to parse location file.");
    return "";
  }

  String location = doc["location"] | "";
  if (!location.isEmpty()) logInfo("Loaded saved surf location.");
  return location;
}

void deleteSurfLocation() {
  if (SPIFFS.exists(LOCATION_FILE)) {
    SPIFFS.remove(LOCATION_FILE);
    logInfo("Deleted saved surf location.");
  }
  surfLocation = DEFAULT_SURF_LOCATION;
  cachedLocation = LocationInfo();
}

bool connectWifi(const WifiCredentials &creds) {
  if (!creds.valid) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
  showStatus("Connecting Wi-Fi", creds.ssid, CYAN);

  for (uint8_t attempt = 0; attempt < 60; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      showStatus("Wi-Fi connected", WiFi.localIP().toString(), GREEN);
      logInfo("Connected to Wi-Fi " + creds.ssid);
      delay(1000);
      return true;
    }
    delay(500);
  }

  logError("Wi-Fi connection failed for SSID: " + creds.ssid);
  showStatus("Wi-Fi failed", "Tap to re-enter", RED);
  return false;
}

String touchKeyboardInput(const String &title, const String &initial, bool secret = false) {
  String value = initial;
  const char *rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM@._-"};
  Rect keyRects[44];
  String keyLabels[44];
  int keyCount = 0;

  while (true) {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(8, 8);
    gfx->println(title);

    gfx->drawRect(8, 30, gfx->width() - 16, 28, WHITE);
    gfx->setCursor(12, 38);
    gfx->setTextColor(WHITE);
    String shown = value;
    if (secret) {
      shown = "";
      for (size_t i = 0; i < value.length(); ++i) shown += '*';
    }
    if (shown.length() > 28) shown = shown.substring(shown.length() - 28);
    gfx->println(shown);

    keyCount = 0;
    int y = 66;
    for (int r = 0; r < 4; ++r) {
      int len = strlen(rows[r]);
      int keyW = (gfx->width() - 16 - (len - 1) * 2) / len;
      for (int i = 0; i < len; ++i) {
        Rect kr = {int16_t(8 + i * (keyW + 2)), int16_t(y), int16_t(keyW), int16_t(24)};
        String label = String(rows[r][i]);
        drawButton(kr, label, 0x7BEF, WHITE, 1);
        keyRects[keyCount] = kr;
        keyLabels[keyCount] = label;
        keyCount++;
      }
      y += 28;
    }

    Rect back = {8, 180, 74, 26};
    Rect clear = {88, 180, 74, 26};
    Rect space = {168, 180, 74, 26};
    Rect done = {248, 180, 64, 26};
    drawButton(back, "<-", 0xFD20, BLACK, 1);
    drawButton(clear, "CLR", RED, WHITE, 1);
    drawButton(space, "SPC", BLUE, WHITE, 1);
    drawButton(done, "OK", GREEN, BLACK, 1);

    while (true) {
      TouchPoint p = getTouchPoint();
      if (!p.pressed) {
        delay(20);
        continue;
      }

      for (int i = 0; i < keyCount; ++i) {
        if (pointInRect(p.x, p.y, keyRects[i])) {
          value += keyLabels[i];
          while (touch.touched()) delay(20);
          goto redraw;
        }
      }

      if (pointInRect(p.x, p.y, back) && !value.isEmpty()) {
        value.remove(value.length() - 1);
        while (touch.touched()) delay(20);
        goto redraw;
      }
      if (pointInRect(p.x, p.y, clear)) {
        value = "";
        while (touch.touched()) delay(20);
        goto redraw;
      }
      if (pointInRect(p.x, p.y, space)) {
        value += " ";
        while (touch.touched()) delay(20);
        goto redraw;
      }
      if (pointInRect(p.x, p.y, done)) {
        while (touch.touched()) delay(20);
        return value;
      }

      delay(20);
    }
  redraw:
    delay(20);
  }
}

WifiCredentials runWifiSetupTouch() {
  WifiCredentials creds;
  Rect ssidButton = {12, 64, 296, 36};
  Rect passButton = {12, 116, 296, 36};
  Rect connectButton = {12, 172, 296, 44};

  while (true) {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println("Wi-Fi Setup");

    drawButton(ssidButton, "SSID: " + (creds.ssid.isEmpty() ? String("<tap to set>") : creds.ssid), BLUE, WHITE, 1);
    String masked = "<tap to set>";
    if (!creds.password.isEmpty()) {
      masked = "";
      for (size_t i = 0; i < creds.password.length(); ++i) masked += '*';
    }
    drawButton(passButton, "PASS: " + masked, BLUE, WHITE, 1);
    drawButton(connectButton, "Save + Connect", GREEN, BLACK, 2);

    TouchPoint p = getTouchPoint();
    if (!p.pressed) {
      delay(30);
      continue;
    }

    if (pointInRect(p.x, p.y, ssidButton)) {
      while (touch.touched()) delay(20);
      creds.ssid = touchKeyboardInput("Enter SSID", creds.ssid, false);
    } else if (pointInRect(p.x, p.y, passButton)) {
      while (touch.touched()) delay(20);
      creds.password = touchKeyboardInput("Enter Password", creds.password, true);
    } else if (pointInRect(p.x, p.y, connectButton) && !creds.ssid.isEmpty()) {
      while (touch.touched()) delay(20);
      creds.valid = true;
      saveWifiCredentials(creds);
      return creds;
    }

    while (touch.touched()) delay(20);
    delay(30);
  }
}

String runLocationSetupTouch() {
  String location = surfLocation;
  Rect locationButton = {12, 76, 296, 44};
  Rect saveButton = {12, 140, 144, 44};
  Rect skipButton = {164, 140, 144, 44};

  while (true) {
    gfx->fillScreen(BLACK);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println("Surf Location");

    String shownLocation = location.isEmpty() ? String("<tap to set>") : location;
    if (shownLocation.length() > 38) shownLocation = shownLocation.substring(0, 38) + "...";
    drawButton(locationButton, shownLocation, BLUE, WHITE, 1);
    drawButton(saveButton, "Save", GREEN, BLACK, 2);
    drawButton(skipButton, "Default", 0x7BEF, BLACK, 2);

    TouchPoint p = getTouchPoint();
    if (!p.pressed) {
      delay(30);
      continue;
    }

    if (pointInRect(p.x, p.y, locationButton)) {
      while (touch.touched()) delay(20);
      location = touchKeyboardInput("Enter surf location", location, false);
    } else if (pointInRect(p.x, p.y, saveButton) && !location.isEmpty()) {
      while (touch.touched()) delay(20);
      saveSurfLocation(location);
      return location;
    } else if (pointInRect(p.x, p.y, skipButton)) {
      while (touch.touched()) delay(20);
      saveSurfLocation(DEFAULT_SURF_LOCATION);
      return DEFAULT_SURF_LOCATION;
    }

    while (touch.touched()) delay(20);
    delay(30);
  }
}

String urlEncode(const String &value) {
  String encoded;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') encoded += c;
    else if (c == ' ') encoded += "%20";
    else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

LocationInfo fetchLocation(const String &location) {
  LocationInfo info;
  if (WiFi.status() != WL_CONNECTED) return info;

  HTTPClient http;
  String url = String(GEOCODE_URL) + "?name=" + urlEncode(location) + "&count=1&language=en&format=json";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return info;
  }

  DynamicJsonDocument doc(12 * 1024);
  String payload = http.getString();
  http.end();
  if (deserializeJson(doc, payload)) return info;

  JsonObject first = doc["results"][0];
  if (first.isNull()) return info;

  info.latitude = first["latitude"] | 0.0f;
  info.longitude = first["longitude"] | 0.0f;
  info.displayName = String((const char *)first["name"]);
  String admin1 = first["admin1"] | "";
  String country = first["country"] | "";
  if (!admin1.isEmpty()) info.displayName += ", " + admin1;
  if (!country.isEmpty()) info.displayName += ", " + country;
  info.valid = true;
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
  if (times.isNull() || heights.isNull() || periods.isNull() || directions.isNull() || times.size() == 0) return forecast;

  forecast.timeLabel = String(times[0].as<const char *>());
  forecast.waveHeight = heights[0] | 0.0f;
  forecast.wavePeriod = periods[0] | 0.0f;
  forecast.waveDirection = directions[0] | 0.0f;
  forecast.valid = true;
  return forecast;
}

void drawForgetButton() {
  forgetButton = {gfx->width() - 214, 8, 102, 24};
  forgetLocationButton = {gfx->width() - 106, 8, 102, 24};
  drawButton(forgetButton, "Forget WiFi", RED, WHITE, 1);
  drawButton(forgetLocationButton, "Forget Loc", 0xFD20, BLACK, 1);
}

void drawForecast(const LocationInfo &location, const SurfForecast &forecast) {
  gfx->fillScreen(BLACK);
  int16_t w = gfx->width();

  gfx->setTextColor(CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(10, 12);
  gfx->println("Surf spot");

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 34);
  String name = location.displayName;
  if (name.length() > 24) name = name.substring(0, 24) + "...";
  gfx->println(name);

  gfx->setTextColor(YELLOW);
  gfx->setCursor(10, 72);
  gfx->println("Wave height");
  gfx->setTextColor(WHITE);
  gfx->setTextSize(4);
  gfx->setCursor(10, 94);
  gfx->println(String(forecast.waveHeight, 2) + " m");

  bool happy = forecast.waveHeight >= 1.0f;
  uint16_t accent = happy ? GREEN : RED;
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(10, 158);
  gfx->println("Period / Direction");
  gfx->setTextColor(accent);
  gfx->setCursor(10, 182);
  String detailStr = String(forecast.wavePeriod, 1) + "s  " + String(forecast.waveDirection, 0) + (char)248;
  gfx->println(detailStr);

  gfx->setTextSize(6);
  gfx->setCursor(w - 90, 120);
  gfx->setTextColor(accent);
  gfx->println(happy ? ":)" : ":(");

  drawForgetButton();
}

void setupDisplay() {
#if TFT_BL >= 0
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  gfx->begin();
  gfx->setRotation(1);
  gfx->fillScreen(BLACK);
}

void setupTouch() {
  touch.begin();
  touch.setRotation(1);
}

bool handleMainScreenTouch() {
  TouchPoint p = getTouchPoint();
  if (!p.pressed) return false;

  if (pointInRect(p.x, p.y, forgetButton)) {
    deleteWifiCredentials();
    WiFi.disconnect(true, true);
    showStatus("Credentials deleted", "Reconfigure Wi-Fi", 0xFD20);
    delay(1200);
    return true;
  }
  if (pointInRect(p.x, p.y, forgetLocationButton)) {
    deleteSurfLocation();
    showStatus("Location deleted", "Reconfigure location", 0xFD20);
    delay(1200);
    return true;
  }
  return false;
}

void ensureWifiConnected() {
  wifiCredentials = loadWifiCredentials();
  if (!wifiCredentials.valid || !connectWifi(wifiCredentials)) {
    while (true) {
      wifiCredentials = runWifiSetupTouch();
      if (connectWifi(wifiCredentials)) break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupDisplay();
  setupTouch();
  if (!SPIFFS.begin(true)) {
    showStatus("SPIFFS failed", "Restart device", RED);
    while (true) delay(1000);
  }

  ensureWifiConnected();

  surfLocation = loadSurfLocation();
  if (surfLocation.isEmpty()) surfLocation = runLocationSetupTouch();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) ensureWifiConnected();

  if (surfLocation.isEmpty()) surfLocation = runLocationSetupTouch();

  showStatus("Finding spot", surfLocation, CYAN);
  if (!cachedLocation.valid) cachedLocation = fetchLocation(surfLocation);
  if (!cachedLocation.valid) {
    showStatus("Location failed", "Retrying...", RED);
    delay(4000);
    return;
  }

  showStatus("Fetching surf", cachedLocation.displayName, CYAN);
  SurfForecast forecast = fetchSurfForecast(cachedLocation.latitude, cachedLocation.longitude);
  if (!forecast.valid) {
    showStatus("Fetch failed", "Retrying...", RED);
    delay(4000);
    return;
  }

  drawForecast(cachedLocation, forecast);

  uint32_t start = millis();
  while (millis() - start < REFRESH_INTERVAL_MS) {
    if (handleMainScreenTouch()) {
      ensureWifiConnected();
      cachedLocation = LocationInfo();
      break;
    }
    delay(50);
  }
}
