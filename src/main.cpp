#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>

#include "Config.h"
#include "Types.h"
#include "Theme.h"
#include "Storage.h"
#include "Network.h"
#include "Display.h"
#include "TouchUI.h"

// Global state
LocationInfo cachedLocation;
WifiCredentials wifiCredentials;
Rect forgetButton = {0, 0, 0, 0};
Rect forgetLocationButton = {0, 0, 0, 0};
Rect themeButton = {0, 0, 0, 0};
Rect waveButton = {0, 0, 0, 0};
Rect tideButton = {0, 0, 0, 0};
String surfLocation = "";
int locationRetryCount = 0;
int surfRetryCount = 0;
float waveHeightThreshold = 1.0f;
float minTide = -1.0f;
float maxTide = 1.0f;
unsigned long tideTimestamp = 0;

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
    surfLocation = runLocationSetupTouch(cachedLocation);
  } else {
    surfLocation = cachedLocation.displayName;
    logInfo("Using saved location: " + surfLocation);
  }
  
  // Load wave height preference (will prompt if not saved)
  waveHeightThreshold = loadWaveHeightPreference();
  if (waveHeightThreshold == 1.0f && !SPIFFS.exists(WAVE_PREF_FILE)) {
    waveHeightThreshold = runWaveHeightSetupTouch();
  }
  
  // Load tide range
  loadTideRange(minTide, maxTide, tideTimestamp);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) ensureWifiConnected();

  if (surfLocation.isEmpty()) {
    surfLocation = runLocationSetupTouch(cachedLocation);
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
    surfRetryCount++;
    if (surfRetryCount >= 3) {
      showStatus("Surf fetch failed", "Resetting settings...", currentTheme.error);
      delay(2000);
      // Delete all files except wifi
      deleteSurfLocation();
      deleteWaveHeightPreference();
      deleteThemePreference();
      surfRetryCount = 0;
      locationRetryCount = 0;
      surfLocation = "";
      cachedLocation = LocationInfo();
      showStatus("Reset complete", "Restarting setup...", currentTheme.textSecondary);
      delay(2000);
      return;
    }
    showStatus("Fetch failed", String("Retry ") + String(surfRetryCount) + "/3", currentTheme.error);
    delay(4000);
    return;
  }
  
  // Successfully fetched surf, reset retry count
  surfRetryCount = 0;
  
  // Update tide range tracking (24 hour window)
  unsigned long currentTime = millis();
  if (tideTimestamp == 0 || (currentTime - tideTimestamp) > 86400000) {
    // Reset after 24 hours or first run
    minTide = forecast.tideHeight;
    maxTide = forecast.tideHeight;
    tideTimestamp = currentTime;
    saveTideRange(minTide, maxTide, tideTimestamp);
  } else {
    // Update min/max if needed
    bool updated = false;
    if (forecast.tideHeight < minTide) {
      minTide = forecast.tideHeight;
      updated = true;
    }
    if (forecast.tideHeight > maxTide) {
      maxTide = forecast.tideHeight;
      updated = true;
    }
    if (updated) {
      saveTideRange(minTide, maxTide, tideTimestamp);
    }
  }

  drawForecast(cachedLocation, forecast, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, waveHeightThreshold, minTide, maxTide);

  uint32_t start = millis();
  while (millis() - start < REFRESH_INTERVAL_MS) {
    int touchResult = handleMainScreenTouch(forgetButton, forgetLocationButton, themeButton, waveButton, tideButton,
                                            surfLocation, cachedLocation, waveHeightThreshold, minTide, maxTide, tideTimestamp);
    if (touchResult == 1) {
      // Location-affecting button: WiFi, Location, or Wave
      ensureWifiConnected();
      cachedLocation = LocationInfo();
      locationRetryCount = 0;
      surfRetryCount = 0;
      break;
    } else if (touchResult == 2) {
      // Theme button: just redraw with new theme, keep location
      drawForecast(cachedLocation, forecast, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, waveHeightThreshold, minTide, maxTide);
    }
    delay(50);
  }
}
