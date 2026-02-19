#include <Arduino.h>
#include <vector>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

const char *DEFAULT_LOCATION_1_NAME = "Mayport Poles";
const float DEFAULT_LOCATION_1_LAT = 30.3970f;
const float DEFAULT_LOCATION_1_LON = -81.4276f;
const char *DEFAULT_LOCATION_2_NAME = "Jax Beach Pier";
const float DEFAULT_LOCATION_2_LAT = 30.3268f;
const float DEFAULT_LOCATION_2_LON = -81.3836f;
const char *WIFI_FILE = "/wifi.json";
const char *LOCATION_FILE = "/location.json";
const char *THEME_FILE = "/theme.json";
const char *WAVE_PREF_FILE = "/wave_pref.json";

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

static const uint32_t REFRESH_INTERVAL_MS = 900000; // 15 minutes
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

struct Theme {
  uint16_t background;
  uint16_t text;
  uint16_t textSecondary;
  uint16_t accent;
  uint16_t buttonPrimary;
  uint16_t buttonSecondary;
  uint16_t buttonDanger;
  uint16_t buttonWarning;
  uint16_t buttonKeys;
  uint16_t buttonList;
  uint16_t border;
  uint16_t success;
  uint16_t error;
  uint16_t cloudColor;
  uint16_t periodDirTextColor;
  uint16_t periodDirNumberColor;
};

LocationInfo cachedLocation;
WifiCredentials wifiCredentials;
Rect forgetButton = {0, 0, 0, 0};
Rect forgetLocationButton = {0, 0, 0, 0};
Rect themeButton = {0, 0, 0, 0};
Rect waveButton = {0, 0, 0, 0};
String surfLocation = DEFAULT_LOCATION_1_NAME;
int locationRetryCount = 0;
bool darkMode = true;
float waveHeightThreshold = 1.0f;  // Default to 1.0 feet

// Dark theme colors
Theme darkTheme = {
  BLACK,      // background
  WHITE,      // text
  CYAN,       // textSecondary (accent text)
  YELLOW,     // accent (sun - yellow on night mode)
  0x2C40,     // buttonPrimary (dark green)
  0x8000,     // buttonSecondary (dark red)
  0x8000,     // buttonDanger (dark red)
  0xCA00,     // buttonWarning (dark orange)
  0x2104,     // buttonKeys (dark gray)
  0x4A49,     // buttonList (dark blue-gray)
  BLACK,      // border
  0x2C40,     // success (dark green)
  0x7FFF,     // error (inverted - cyan)
  0x4DFF,     // cloudColor (light blue on night mode)
  CYAN,       // periodDirTextColor (cyan on night mode)
  0x65C8      // periodDirNumberColor (lighter green on night mode)
};

// Light theme colors
Theme lightTheme = {
  WHITE,      // background
  BLACK,      // text
  BLUE,       // textSecondary
  0xFE80,     // accent (orange-yellow on day mode)
  GREEN,      // buttonPrimary
  RED,        // buttonSecondary
  RED,        // buttonDanger
  0xFD20,     // buttonWarning (orange)
  0xCE79,     // buttonKeys (light gray)
  0xAD55,     // buttonList (lighter gray)
  BLACK,      // border
  GREEN,      // success
  0x07FF,     // error (inverted - cyan)
  0x0016,     // cloudColor (dark blue on day mode)
  0xCA00,     // periodDirTextColor (orange on day mode)
  0x2400      // periodDirNumberColor (darker green on day mode)
};

Theme currentTheme = darkTheme;

void logInfo(const String &message) { Serial.printf("[INFO  %10lu ms] %s\n", millis(), message.c_str()); }
void logError(const String &message) { Serial.printf("[ERROR %10lu ms] %s\n", millis(), message.c_str()); }

// Forward declarations
std::vector<LocationInfo> fetchLocationMatches(const String &location, int maxResults);
int selectLocationFromList(const std::vector<LocationInfo> &locations);
int selectDefaultLocation();

// Invert a 16-bit RGB565 color
uint16_t invertColor(uint16_t color) {
  return color ^ 0xFFFF;
}

bool pointInRect(int16_t x, int16_t y, const Rect &r) {
  return x >= r.x && y >= r.y && x < (r.x + r.w) && y < (r.y + r.h);
}

TouchPoint getTouchPoint() {
  TouchPoint p;
  if (!touch.touched()) return p;

  TS_Point raw = touch.getPoint();
  
  // Inverted mapping on both axes
  p.x = map(raw.x, TOUCH_MIN_X, TOUCH_MAX_X, gfx->width(), 0);
  p.y = map(raw.y, TOUCH_MIN_Y, TOUCH_MAX_Y, gfx->height(), 0);
  p.x = constrain(p.x, 0, gfx->width() - 1);
  p.y = constrain(p.y, 0, gfx->height() - 1);
  p.pressed = true;
  
  // Debug output
  Serial.printf("Touch: raw(%d,%d) -> mapped(%d,%d)\n", raw.x, raw.y, p.x, p.y);
  
  return p;
}

void drawButton(const Rect &r, const String &label, uint16_t bg, uint16_t fg = BLACK, uint8_t textSize = 2) {
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
  gfx->drawRoundRect(r.x, r.y, r.w, r.h, 6, currentTheme.border);
  gfx->setTextColor(fg);
  gfx->setTextSize(textSize);
  int16_t tx = r.x + 8;
  int16_t ty = r.y + (r.h / 2) - 8;
  gfx->setCursor(tx, ty);
  gfx->println(label);
}

void showStatus(const String &line1, const String &line2 = "", uint16_t color = WHITE) {
  gfx->fillScreen(currentTheme.background);
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(12, 24);
  gfx->println(line1);
  if (!line2.isEmpty()) {
    gfx->setTextColor(currentTheme.text);
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

void applyTheme() {
  currentTheme = darkMode ? darkTheme : lightTheme;
  // Invert all theme colors to compensate for display hardware inversion
  currentTheme.background = invertColor(currentTheme.background);
  currentTheme.text = invertColor(currentTheme.text);
  currentTheme.textSecondary = invertColor(currentTheme.textSecondary);
  currentTheme.accent = invertColor(currentTheme.accent);
  currentTheme.buttonPrimary = invertColor(currentTheme.buttonPrimary);
  currentTheme.buttonSecondary = invertColor(currentTheme.buttonSecondary);
  currentTheme.buttonDanger = invertColor(currentTheme.buttonDanger);
  currentTheme.buttonWarning = invertColor(currentTheme.buttonWarning);
  currentTheme.buttonKeys = invertColor(currentTheme.buttonKeys);
  currentTheme.buttonList = invertColor(currentTheme.buttonList);
  currentTheme.border = invertColor(currentTheme.border);
  currentTheme.success = invertColor(currentTheme.success);
  currentTheme.error = invertColor(currentTheme.error);
  currentTheme.cloudColor = invertColor(currentTheme.cloudColor);
  currentTheme.periodDirTextColor = invertColor(currentTheme.periodDirTextColor);
  currentTheme.periodDirNumberColor = invertColor(currentTheme.periodDirNumberColor);
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
  surfLocation = "";  // Clear location to force re-entry
  cachedLocation = LocationInfo();
}

void deleteWaveHeightPreference() {
  if (SPIFFS.exists(WAVE_PREF_FILE)) {
    SPIFFS.remove(WAVE_PREF_FILE);
    logInfo("Deleted saved wave height preference.");
  }
  waveHeightThreshold = 1.0f;  // Reset to default
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

String touchKeyboardInput(const String &title, const String &initial, bool secret = false) {
  String value = initial;
  bool shiftOn = false;
  const char *rowsUpper[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM@._-"};
  const char *rowsLower[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm@._-"};
  Rect keyRects[44];
  String keyLabels[44];
  int keyCount = 0;

  while (true) {
    gfx->fillScreen(currentTheme.background);
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(2);
    gfx->setCursor(8, 8);
    gfx->println(title);

    gfx->drawRect(8, 30, gfx->width() - 16, 28, currentTheme.border);
    gfx->setCursor(12, 38);
    gfx->setTextColor(currentTheme.text);
    String shown = value;
    if (secret) {
      shown = "";
      for (size_t i = 0; i < value.length(); ++i) shown += '*';
    }
    if (shown.length() > 28) shown = shown.substring(shown.length() - 28);
    gfx->println(shown);

    const char **rows = shiftOn ? rowsUpper : rowsLower;
    keyCount = 0;
    int y = 66;
    for (int r = 0; r < 4; ++r) {
      int len = strlen(rows[r]);
      int keyW = (gfx->width() - 16 - (len - 1) * 2) / len;
      for (int i = 0; i < len; ++i) {
        Rect kr = {int16_t(8 + i * (keyW + 2)), int16_t(y), int16_t(keyW), int16_t(24)};
        String label = String(rows[r][i]);
        drawButton(kr, label, currentTheme.buttonKeys, currentTheme.text, 1);
        keyRects[keyCount] = kr;
        keyLabels[keyCount] = label;
        keyCount++;
      }
      y += 28;
    }

    Rect shift = {8, 180, 60, 26};
    Rect back = {74, 180, 60, 26};
    Rect clear = {140, 180, 60, 26};
    Rect space = {206, 180, 60, 26};
    Rect done = {272, 180, 40, 26};
    drawButton(shift, shiftOn ? "ABC" : "abc", currentTheme.buttonWarning, currentTheme.text, 1);
    drawButton(back, "<-", currentTheme.buttonWarning, currentTheme.text, 1);
    drawButton(clear, "CLR", currentTheme.buttonDanger, currentTheme.text, 1);
    drawButton(space, "SPC", currentTheme.buttonSecondary, currentTheme.text, 1);
    drawButton(done, "OK", currentTheme.buttonPrimary, currentTheme.text, 1);

    while (true) {
      TouchPoint p = getTouchPoint();
      if (!p.pressed) {
        delay(50);
        continue;
      }

      for (int i = 0; i < keyCount; ++i) {
        if (pointInRect(p.x, p.y, keyRects[i])) {
          value += keyLabels[i];
          while (touch.touched()) delay(20);
          goto redraw;
        }
      }

      if (pointInRect(p.x, p.y, shift)) {
        shiftOn = !shiftOn;
        while (touch.touched()) delay(20);
        goto redraw;
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
  
  bool needsRedraw = true;

  while (true) {
    if (needsRedraw) {
      gfx->fillScreen(currentTheme.background);
      gfx->setTextColor(currentTheme.textSecondary);
      gfx->setTextSize(2);
      gfx->setCursor(10, 10);
      gfx->println("Wi-Fi Setup");

      drawButton(ssidButton, "SSID: " + (creds.ssid.isEmpty() ? String("<tap to set>") : creds.ssid), currentTheme.buttonSecondary, currentTheme.text, 1);
      String masked = "<tap to set>";
      if (!creds.password.isEmpty()) {
        masked = "";
        for (size_t i = 0; i < creds.password.length(); ++i) masked += '*';
      }
      drawButton(passButton, "PASS: " + masked, currentTheme.buttonSecondary, currentTheme.text, 1);
      drawButton(connectButton, "Save + Connect", currentTheme.buttonPrimary, currentTheme.text, 2);
      needsRedraw = false;
    }

    TouchPoint p = getTouchPoint();
    if (!p.pressed) {
      delay(50);
      continue;
    }

    if (pointInRect(p.x, p.y, ssidButton)) {
      while (touch.touched()) delay(20);
      creds.ssid = touchKeyboardInput("Enter SSID", creds.ssid, false);
      needsRedraw = true;
    } else if (pointInRect(p.x, p.y, passButton)) {
      while (touch.touched()) delay(20);
      creds.password = touchKeyboardInput("Enter Password", creds.password, true);
      needsRedraw = true;
    } else if (pointInRect(p.x, p.y, connectButton) && !creds.ssid.isEmpty()) {
      while (touch.touched()) delay(20);
      creds.valid = true;
      saveWifiCredentials(creds);
      return creds;
    }

    while (touch.touched()) delay(20);
    delay(50);
  }
}

int selectLocationFromList(const std::vector<LocationInfo> &locations) {
  if (locations.empty()) return -1;
  
  const int itemHeight = 36;
  const int startY = 40;
  
  while (true) {
    gfx->fillScreen(currentTheme.background);
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println("Select Location:");
    
    std::vector<Rect> buttons;
    for (size_t i = 0; i < locations.size() && i < 7; i++) {
      Rect r = {8, int16_t(startY + i * itemHeight), int16_t(gfx->width() - 16), int16_t(itemHeight - 4)};
      String label = locations[i].displayName;
      if (label.length() > 35) label = label.substring(0, 35) + "...";
      drawButton(r, label, currentTheme.buttonList, currentTheme.text, 1);
      buttons.push_back(r);
    }
    
    // Cancel button
    Rect cancelBtn = {8, int16_t(startY + 7 * itemHeight), int16_t(gfx->width() - 16), 30};
    drawButton(cancelBtn, "Cancel", currentTheme.buttonDanger, currentTheme.text, 2);
    
    while (true) {
      TouchPoint p = getTouchPoint();
      if (!p.pressed) {
        delay(50);
        continue;
      }
      
      // Check location buttons
      for (size_t i = 0; i < buttons.size(); i++) {
        if (pointInRect(p.x, p.y, buttons[i])) {
          while (touch.touched()) delay(20);
          return (int)i;
        }
      }
      
      // Check cancel button
      if (pointInRect(p.x, p.y, cancelBtn)) {
        while (touch.touched()) delay(20);
        return -1;
      }
      
      delay(50);
    }
  }
}

int selectDefaultLocation() {
  while (true) {
    gfx->fillScreen(currentTheme.background);
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(2);
    gfx->setCursor(10, 10);
    gfx->println("Choose Default:");
    
    Rect loc1Btn = {10, 70, 300, 70};
    Rect loc2Btn = {10, 160, 300, 70};
    Rect cancelBtn = {10, 250, 300, 40};
    
    drawButton(loc1Btn, DEFAULT_LOCATION_1_NAME, currentTheme.buttonPrimary, currentTheme.text, 2);
    drawButton(loc2Btn, DEFAULT_LOCATION_2_NAME, currentTheme.buttonPrimary, currentTheme.text, 2);
    drawButton(cancelBtn, "Cancel", currentTheme.buttonDanger, currentTheme.text, 2);
    
    while (true) {
      TouchPoint p = getTouchPoint();
      if (!p.pressed) {
        delay(50);
        continue;
      }
      
      if (pointInRect(p.x, p.y, loc1Btn)) {
        while (touch.touched()) delay(20);
        return 1;
      } else if (pointInRect(p.x, p.y, loc2Btn)) {
        while (touch.touched()) delay(20);
        return 2;
      } else if (pointInRect(p.x, p.y, cancelBtn)) {
        while (touch.touched()) delay(20);
        return 0;
      }
      
      delay(50);
    }
  }
}

float runWaveHeightSetupTouch() {
  float selectedThreshold = 1.0f;
  Rect sliderArea = {40, 120, 240, 50};
  Rect decButton = {40, 190, 60, 45};  // Decrease
  Rect incButton = {220, 190, 60, 45};  // Increase
  Rect saveButton = {20, 250, 140, 50};
  Rect skipButton = {180, 250, 140, 50};
  
  bool needsRedraw = true;

  while (true) {
    if (needsRedraw) {
      gfx->fillScreen(currentTheme.background);
      gfx->setTextColor(currentTheme.textSecondary);
      gfx->setTextSize(2);
      gfx->setCursor(10, 10);
      gfx->println("Wave Height Preference");
      
      gfx->setTextColor(currentTheme.text);
      gfx->setTextSize(3);
      gfx->setCursor(10, 35);
      gfx->println("What size waves");
      gfx->setCursor(10, 60);
      gfx->println("make you happy?");
      
      // Draw slider bar
      gfx->drawRect(sliderArea.x, sliderArea.y, sliderArea.w, sliderArea.h, currentTheme.border);
      int barWidth = (int)((selectedThreshold - 0.5f) / 9.5f * (sliderArea.w - 4));
      gfx->fillRect(sliderArea.x + 2, sliderArea.y + 2, barWidth, sliderArea.h - 4, currentTheme.accent);
      
      // Display selected value
      gfx->setTextColor(currentTheme.text);
      gfx->setTextSize(3);
      gfx->setCursor(140, 140);
      gfx->println(String(selectedThreshold, 1) + "ft");
      
      // Draw buttons
      drawButton(decButton, "-", currentTheme.buttonSecondary, currentTheme.text, 2);
      drawButton(incButton, "+", currentTheme.buttonPrimary, currentTheme.text, 2);
      drawButton(saveButton, "Save", currentTheme.buttonPrimary, currentTheme.text, 2);
      drawButton(skipButton, "3 ft", currentTheme.buttonList, currentTheme.text, 2);
      
      needsRedraw = false;
    }

    TouchPoint p = getTouchPoint();
    if (!p.pressed) {
      delay(50);
      continue;
    }

    if (pointInRect(p.x, p.y, decButton)) {
      selectedThreshold = max(0.5f, selectedThreshold - 0.5f);
      while (touch.touched()) delay(20);
      needsRedraw = true;
    } else if (pointInRect(p.x, p.y, incButton)) {
      selectedThreshold = min(10.0f, selectedThreshold + 0.5f);
      while (touch.touched()) delay(20);
      needsRedraw = true;
    } else if (pointInRect(p.x, p.y, saveButton)) {
      while (touch.touched()) delay(20);
      saveWaveHeightPreference(selectedThreshold);
      return selectedThreshold;
    } else if (pointInRect(p.x, p.y, skipButton)) {
      while (touch.touched()) delay(20);
      return selectedThreshold;  // Return default
    }

    while (touch.touched()) delay(20);
    delay(50);
  }
}

String runLocationSetupTouch() {
  String location = "";  // Start fresh each time
  Rect locationButton = {12, 76, 296, 44};
  Rect saveButton = {12, 140, 144, 44};
  Rect skipButton = {164, 140, 144, 44};
  
  bool needsRedraw = true;

  while (true) {
    if (needsRedraw) {
      gfx->fillScreen(currentTheme.background);
      gfx->setTextColor(currentTheme.textSecondary);
      gfx->setTextSize(2);
      gfx->setCursor(10, 10);
      gfx->println("Surf Location");

      String shownLocation = location.isEmpty() ? String("<tap to set>") : location;
      if (shownLocation.length() > 38) shownLocation = shownLocation.substring(0, 38) + "...";
      drawButton(locationButton, shownLocation, currentTheme.buttonSecondary, currentTheme.text, 1);
      drawButton(saveButton, "Save", currentTheme.buttonPrimary, currentTheme.text, 2);
      drawButton(skipButton, "Default", currentTheme.buttonList, currentTheme.text, 2);
      needsRedraw = false;
    }

    TouchPoint p = getTouchPoint();
    if (!p.pressed) {
      delay(50);
      continue;
    }

    if (pointInRect(p.x, p.y, locationButton)) {
      while (touch.touched()) delay(20);
      String searchTerm = touchKeyboardInput("Enter surf location", location, false);
      if (!searchTerm.isEmpty()) {
        // Show searching message
        gfx->fillScreen(currentTheme.background);
        gfx->setTextColor(currentTheme.textSecondary);
        gfx->setTextSize(2);
        gfx->setCursor(10, 10);
        gfx->println("Searching locations...");
        
        // Fetch matching locations
        auto matches = fetchLocationMatches(searchTerm, 8);
        
        if (matches.empty()) {
          gfx->setCursor(10, 50);
          gfx->setTextColor(currentTheme.error);
          gfx->println("No locations found");
          delay(2000);
          needsRedraw = true;
        } else if (matches.size() == 1) {
          location = matches[0].displayName;
          cachedLocation = matches[0];  // Cache the full location info
          needsRedraw = true;
        } else {
          // Show selection list
          int selectedIndex = selectLocationFromList(matches);
          if (selectedIndex >= 0 && selectedIndex < (int)matches.size()) {
            location = matches[selectedIndex].displayName;
            cachedLocation = matches[selectedIndex];  // Cache the full location info
          }
          needsRedraw = true;
        }
      }
    } else if (pointInRect(p.x, p.y, saveButton) && !location.isEmpty()) {
      while (touch.touched()) delay(20);
      // Ensure we have valid location coordinates before saving
      if (cachedLocation.valid) {
        saveSurfLocation(cachedLocation);
        return cachedLocation.displayName;
      } else {
        // Need to search and select first
        gfx->fillScreen(currentTheme.background);
        gfx->setTextColor(currentTheme.error);
        gfx->setTextSize(2);
        gfx->setCursor(10, 100);
        gfx->println("Please search and");
        gfx->setCursor(10, 125);
        gfx->println("select location first");
        delay(2000);
        needsRedraw = true;
      }
    } else if (pointInRect(p.x, p.y, skipButton)) {
      while (touch.touched()) delay(20);
      // Show default location selection
      int selected = selectDefaultLocation();
      if (selected == 1) {
        cachedLocation.displayName = DEFAULT_LOCATION_1_NAME;
        cachedLocation.latitude = DEFAULT_LOCATION_1_LAT;
        cachedLocation.longitude = DEFAULT_LOCATION_1_LON;
        cachedLocation.valid = true;
        saveSurfLocation(cachedLocation);
        return cachedLocation.displayName;
      } else if (selected == 2) {
        cachedLocation.displayName = DEFAULT_LOCATION_2_NAME;
        cachedLocation.latitude = DEFAULT_LOCATION_2_LAT;
        cachedLocation.longitude = DEFAULT_LOCATION_2_LON;
        cachedLocation.valid = true;
        saveSurfLocation(cachedLocation);
        return cachedLocation.displayName;
      }
      needsRedraw = true;
    }

    while (touch.touched()) delay(20);
    delay(50);
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

std::vector<LocationInfo> fetchLocationMatches(const String &location, int maxResults = 10) {
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
  if (times.isNull() || heights.isNull() || periods.isNull() || directions.isNull() || times.size() == 0) return forecast;

  forecast.timeLabel = String(times[0].as<const char *>());
  forecast.waveHeight = heights[0] | 0.0f;
  forecast.wavePeriod = periods[0] | 0.0f;
  forecast.waveDirection = directions[0] | 0.0f;
  forecast.valid = true;
  return forecast;
}

void drawGoodSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // Draw sun with rays (good surf)
  int16_t sunRadius = 30;
  
  // Draw sun rays (8 rays around the circle)
  for (int i = 0; i < 8; i++) {
    float angle = i * PI / 4.0;
    int16_t x1 = x + cos(angle) * (sunRadius + 5);
    int16_t y1 = y + sin(angle) * (sunRadius + 5);
    int16_t x2 = x + cos(angle) * (sunRadius + 18);
    int16_t y2 = y + sin(angle) * (sunRadius + 18);
    gfx->drawLine(x1, y1, x2, y2, color);
    gfx->drawLine(x1+1, y1, x2+1, y2, color);  // Thicker rays
    gfx->drawLine(x1, y1+1, x2, y2+1, color);
  }
  
  // Draw sun circle
  gfx->fillCircle(x, y, sunRadius, color);
  
  // Draw happy face on sun
  uint16_t faceColor = currentTheme.background;
  gfx->fillCircle(x - 10, y - 8, 4, faceColor);  // Left eye
  gfx->fillCircle(x + 10, y - 8, 4, faceColor);  // Right eye
  
  // Happy smile (arc)
  for (int i = -15; i <= 15; i++) {
    int16_t sx = x + i;
    int16_t sy = y + 8 + (int16_t)(sqrt(max(0, 225 - i*i)) / 2);
    gfx->drawPixel(sx, sy, faceColor);
    gfx->drawPixel(sx, sy+1, faceColor);
  }
}

void drawBadSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // Draw storm cloud with rain (bad surf)
  
  // Cloud body (three overlapping circles) - always red
  gfx->fillCircle(x - 20, y, 18, currentTheme.cloudColor);
  gfx->fillCircle(x, y - 10, 22, currentTheme.cloudColor);
  gfx->fillCircle(x + 20, y, 18, currentTheme.cloudColor);
  gfx->fillRect(x - 35, y, 70, 15, currentTheme.cloudColor);
  
  // Rain drops (diagonal lines)
  uint16_t rainColor = currentTheme.cloudColor;
  for (int i = 0; i < 5; i++) {
    int16_t rx = x - 25 + i * 12;
    int16_t ry = y + 18;
    gfx->drawLine(rx, ry, rx + 3, ry + 10, rainColor);
    gfx->drawLine(rx + 1, ry, rx + 4, ry + 10, rainColor);
    gfx->drawLine(rx + 1, ry + 14, rx + 4, ry + 22, rainColor);
    gfx->drawLine(rx + 2, ry + 14, rx + 5, ry + 22, rainColor);
  }
  
  // Sad face on cloud
  uint16_t faceColor = currentTheme.background;
  gfx->fillCircle(x - 10, y - 5, 3, faceColor);  // Left eye
  gfx->fillCircle(x + 10, y - 5, 3, faceColor);  // Right eye
  
  // Sad frown (inverted arc)
  for (int i = -12; i <= 12; i++) {
    int16_t sx = x + i;
    int16_t sy = y + 10 - (int16_t)(sqrt(max(0, 144 - i*i)) / 2.5);
    gfx->drawPixel(sx, sy, faceColor);
    gfx->drawPixel(sx, sy-1, faceColor);
  }
}

void drawForgetButton() {
  // 2x2 grid layout in top right
  int btnW = 68;
  int btnH = 20;
  int gap = 1;
  int startX = gfx->width() - (btnW * 2 + gap);
  int startY = 2;
  
  // Top-left: Forget WiFi
  forgetButton = {int16_t(startX), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetButton, "WiFi", currentTheme.success, currentTheme.text, 1);
  
  // Top-right: Theme toggle
  themeButton = {int16_t(startX + btnW + gap), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(themeButton, darkMode ? "Light" : "Dark", currentTheme.text, currentTheme.background, 1);
  
  // Bottom-left: Forget Location
  forgetLocationButton = {int16_t(startX), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetLocationButton, "Loc", currentTheme.buttonWarning, currentTheme.text, 1);
  
  // Bottom-right: Reset Wave
  waveButton = {int16_t(startX + btnW + gap), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(waveButton, "Wave", currentTheme.buttonDanger, currentTheme.text, 1);
}

void drawForecast(const LocationInfo &location, const SurfForecast &forecast) {
  gfx->fillScreen(currentTheme.background);
  int16_t w = gfx->width();

  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(3);
  gfx->setCursor(10, 10);
  gfx->println("Surf spot");

  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(4);
  gfx->setCursor(10, 38);
  String name = location.displayName;
  // Get string before 2nd comma for shorter display
  if (name.length() > 20) {
    gfx->setTextSize(2);
    int firstComma = name.indexOf(',');
    if (firstComma >= 0) {
      int secondComma = name.indexOf(',', firstComma + 1);
      if (secondComma >= 0) {
        name = name.substring(0, secondComma);
      }
    }
  }
  if (name.length()> 20 ) gfx->setTextSize(3);
  
  if (name.length() > 30) name = name.substring(0, 30) + "...";
  gfx->println(name);

  gfx->setTextColor(currentTheme.accent);
  gfx->setCursor(10, 82);
  gfx->println("Wave height");
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(9);
  gfx->setCursor(10, 120);
  float waveHeightFeet = forecast.waveHeight * 3.28084;
  gfx->println(String(waveHeightFeet, 1) + " ft");

  bool happy = forecast.waveHeight * 3.28084 >= waveHeightThreshold;
  uint16_t accent = happy ? currentTheme.success : currentTheme.error;
  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(5);
  gfx->setCursor(10, 200);
  gfx->println("Period | Dir");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setCursor(10, 255);
  String detailStr = String(forecast.wavePeriod, 1) + "s     " + String(forecast.waveDirection, 0) + (char)248;
  gfx->println(detailStr);

  // Draw surf condition graphic
  if (happy) {
    drawGoodSurfGraphic(w - 70, 150, currentTheme.accent);
  } else {
    drawBadSurfGraphic(w - 70, 150, accent);
  }

  drawForgetButton();
}

void setupDisplay() {
#if TFT_BL >= 0
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);  // Keep backlight off during init
#endif
  gfx->begin();
  gfx->setRotation(1);
  gfx->fillScreen(currentTheme.background);
#if TFT_BL >= 0
  digitalWrite(TFT_BL, HIGH);  // Turn on backlight after init
#endif
}

void setupTouch() {
  // Initialize SPI bus for touchscreen (shares same pins as display)
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
  touch.begin();
  // No rotation - we handle mapping manually
}

// Returns: 0=no touch, 1=location-affecting button, 2=theme/display only
int handleMainScreenTouch() {
  TouchPoint p = getTouchPoint();
  if (!p.pressed) return 0;

  if (pointInRect(p.x, p.y, forgetButton)) {
    deleteWifiCredentials();
    WiFi.disconnect(true, true);
    showStatus("Credentials deleted", "Reconfigure Wi-Fi", currentTheme.buttonWarning);
    delay(1200);
    return 1;  // Location-affecting
  }
  if (pointInRect(p.x, p.y, forgetLocationButton)) {
    deleteSurfLocation();
    showStatus("Location deleted", "Reconfigure location", currentTheme.buttonWarning);
    delay(1200);
    surfLocation = "";  // Ensure it's cleared
    cachedLocation = LocationInfo();
    return 1;  // Location-affecting
  }
  if (pointInRect(p.x, p.y, themeButton)) {
    darkMode = !darkMode;
    applyTheme();
    saveThemePreference(darkMode);
    return 2;  // Theme only - don't clear location
  }
  if (pointInRect(p.x, p.y, waveButton)) {
    deleteWaveHeightPreference();
    showStatus("Wave pref reset", "Reconfigure wave height", currentTheme.buttonWarning);
    delay(1200);
    waveHeightThreshold = runWaveHeightSetupTouch();
    return 2;  // Display only - don't clear location, just redraw
  }
  return 0;
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

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS failed");
    while (true) delay(1000);
  }

  // Load theme preference before display init
  darkMode = loadThemePreference();
  applyTheme();

  setupDisplay();
  setupTouch();

  ensureWifiConnected();

  cachedLocation = loadSurfLocationInfo();
  if (!cachedLocation.valid) {
    surfLocation = runLocationSetupTouch();
  } else {
    surfLocation = cachedLocation.displayName;
    logInfo("Using saved location: " + surfLocation);
  }
  
  // Load wave height preference (will prompt if not saved)
  waveHeightThreshold = loadWaveHeightPreference();
  if (waveHeightThreshold == 1.0f && !SPIFFS.exists(WAVE_PREF_FILE)) {
    waveHeightThreshold = runWaveHeightSetupTouch();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) ensureWifiConnected();

  if (surfLocation.isEmpty()) {
    surfLocation = runLocationSetupTouch();
    locationRetryCount = 0;  // Reset retry count for new location
  }

  showStatus("Finding spot", surfLocation, currentTheme.textSecondary);
  if (!cachedLocation.valid) cachedLocation = fetchLocation(surfLocation);
  if (!cachedLocation.valid) {
    locationRetryCount++;
    if (locationRetryCount >= 3) {
      showStatus("Location failed", "Enter new location", currentTheme.error);
      delay(3000);
      surfLocation = "";
      cachedLocation = LocationInfo();
      locationRetryCount = 0;
      return;
    }
    showStatus("Location failed", String("Retry ") + String(locationRetryCount) + "/3", currentTheme.error);
    delay(4000);
    return;
  }
  
  // Successfully found location, reset retry count
  locationRetryCount = 0;

  showStatus("Fetching surf", cachedLocation.displayName, currentTheme.textSecondary);
  SurfForecast forecast = fetchSurfForecast(cachedLocation.latitude, cachedLocation.longitude);
  if (!forecast.valid) {
    showStatus("Fetch failed", "Retrying...", currentTheme.error);
    delay(4000);
    return;
  }

  drawForecast(cachedLocation, forecast);

  uint32_t start = millis();
  while (millis() - start < REFRESH_INTERVAL_MS) {
    int touchResult = handleMainScreenTouch();
    if (touchResult == 1) {
      // Location-affecting button: WiFi, Location, or Wave
      ensureWifiConnected();
      cachedLocation = LocationInfo();
      locationRetryCount = 0;
      break;
    } else if (touchResult == 2) {
      // Theme button: just redraw with new theme, keep location
      drawForecast(cachedLocation, forecast);
    }
    delay(50);
  }
}
