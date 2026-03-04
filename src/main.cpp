#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <time.h>

#include "Config.h"
#include "Types.h"
#include "Theme.h"
#include "Storage.h"
#include "Network.h"
#include "Display.h"
#include "TouchUI.h"
#include "Game.h"

// Global state
LocationInfo cachedLocation;
WifiCredentials wifiCredentials;
bool inSettingsMode = false;
bool inGameMode = false;
Rect settingsButton = {0, 0, 0, 0};
Rect backButton = {0, 0, 0, 0};
Rect forgetButton = {0, 0, 0, 0};
Rect forgetLocationButton = {0, 0, 0, 0};
Rect themeButton = {0, 0, 0, 0};
Rect waveButton = {0, 0, 0, 0};
Rect tideButton = {0, 0, 0, 0};
Rect filesButton = {0, 0, 0, 0};
Rect leaderboardButton = {0, 0, 0, 0};
Rect badSurfGraphicRect = {0, 0, 0, 0};
Rect exitButton = {0, 0, 0, 0};
String surfLocation = "";
int locationRetryCount = 0;
int surfRetryCount = 0;
float waveHeightThreshold = 1.0f;
int currentTideDirection = 0;
bool currentHasTideFile = false;

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
  
  // Check if this is first boot (no config files exist)
  bool isFirstBoot = !SPIFFS.exists(WIFI_FILE) && 
                     !SPIFFS.exists(LOCATION_FILE) && 
                     !SPIFFS.exists(THEME_FILE) &&
                     !SPIFFS.exists(WAVE_PREF_FILE);
  
  if (isFirstBoot) {
    // Show welcome screen until user presses setup
    Rect setupButton = {0, 0, 0, 0};
    drawWelcomeScreen(setupButton);
    
    // Wait for setup button press
    while (true) {
      if (touch.touched()) {
        TS_Point raw = touch.getPoint();
        int16_t touchX = map(raw.x, TOUCH_MIN_X, TOUCH_MAX_X, gfx->width(), 0);
        int16_t touchY = map(raw.y, TOUCH_MIN_Y, TOUCH_MAX_Y, gfx->height(), 0);
        if (pointInRect(touchX, touchY, setupButton)) {
          while (touch.touched()) delay(20);
          break;
        }
        while (touch.touched()) delay(20);
      }
      delay(50);
    }

    // Name entry — keyboard, then confirm screen
    String playerName;
    while (true) {
      playerName = touchKeyboardInput("What's your name?", playerName, false);
      if (playerName.length() == 0) continue; // require non-empty

      Rect confirmButton = {0, 0, 0, 0};
      drawNameConfirmScreen(playerName, confirmButton);

      bool confirmed = false;
      while (!confirmed) {
        if (touch.touched()) {
          TS_Point raw = touch.getPoint();
          int16_t touchX = map(raw.x, TOUCH_MIN_X, TOUCH_MAX_X, gfx->width(), 0);
          int16_t touchY = map(raw.y, TOUCH_MIN_Y, TOUCH_MAX_Y, gfx->height(), 0);
          while (touch.touched()) delay(20);
          if (pointInRect(touchX, touchY, confirmButton)) {
            confirmed = true;
          } else {
            // Tapped elsewhere — go back to keyboard to re-enter
            break;
          }
        }
        delay(50);
      }
      if (confirmed) break; // exit loop, proceed to WiFi
    }

    savePlayerName(playerName);
  }

  ensureWifiConnected();
  
  // Configure NTP time sync
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  logInfo("Waiting for NTP time sync...");
  // Wait up to 10 seconds for time sync
  int timeoutCount = 0;
  while (time(nullptr) < 1000000000 && timeoutCount < 100) {
    delay(100);
    timeoutCount++;
  }
  if (time(nullptr) >= 1000000000) {
    logInfo("NTP time synced successfully: " + String(time(nullptr)));
  } else {
    logError("NTP time sync failed after 10 seconds, time=" + String(time(nullptr)));
  }

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
      deleteTideBounds();
      deleteTideDirection();
      deleteTideHourlyCheck();
      currentHasTideFile = false;
      currentTideDirection = 0;
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

  // Tide direction: always compare the latest hourly reading to the current reading
  // so an up/down arrow is always available when tide data is available.
  time_t currentTime = time(nullptr);
  currentHasTideFile = SPIFFS.exists(TIDE_HOURLY_FILE);

  float hourlyStartHeight = 0.0f;
  int hourlyStartHour = -1;
  time_t ignoredHourlyStartTime = 0;
  bool hasHourlyReading = loadTideHourlyCheck(hourlyStartHeight, ignoredHourlyStartTime, hourlyStartHour);

  struct tm *nowTm = localtime(&currentTime);
  int currentHour = nowTm ? nowTm->tm_hour : -1;

  if (!hasHourlyReading || hourlyStartHour < 0 || currentHour < 0) {
    // Seed from current reading and default to rising arrow so it always renders.
    saveTideHourlyCheck(forecast.tideHeight, currentTime, currentHour);
    currentTideDirection = 1;
    currentHasTideFile = true;
    logInfo("Seeded hourly tide reading; defaulting arrow to rising for first sample.");
  } else {
    float heightChange = forecast.tideHeight - hourlyStartHeight;
    currentTideDirection = (heightChange >= 0.0f) ? 1 : -1;
    logInfo("Tide direction from hourly reading: " +
            String(hourlyStartHeight, 3) + "m -> " +
            String(forecast.tideHeight, 3) + "m (delta " +
            String(heightChange, 3) + "m)");

    // Start a new hourly baseline when the hour rolls over.
    if (currentHour != hourlyStartHour) {
      saveTideHourlyCheck(forecast.tideHeight, currentTime, currentHour);
      logInfo("Updated tide hourly baseline for hour " + String(currentHour));
    }
  }

  // Keep compatibility file updated for diagnostics/screens that inspect it.
  saveTideDirection(forecast.tideHeight, currentTime, currentTideDirection);

  drawForecast(cachedLocation, forecast, settingsButton, badSurfGraphicRect, waveHeightThreshold, forecast.minTide, forecast.maxTide, currentTideDirection, currentHasTideFile);

  uint32_t start = millis();
  while (millis() - start < REFRESH_INTERVAL_MS) {
    if (inSettingsMode) {
      // Handle settings screen
      int touchResult = handleSettingsScreenTouch(backButton, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, filesButton, leaderboardButton,
                                                   surfLocation, cachedLocation, waveHeightThreshold);
      if (touchResult == 1) {
        // Location-affecting button: WiFi or Location
        inSettingsMode = false;
        ensureWifiConnected();
        cachedLocation = LocationInfo();
        locationRetryCount = 0;
        surfRetryCount = 0;
        // Reset tide state so it is cleanly re-seeded for the new location
        currentHasTideFile = false;
        currentTideDirection = 0;
        break;
      } else if (touchResult == 2) {
        // Theme or Wave or Tide button: redraw settings screen
        drawSettingsScreen(backButton, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, filesButton, leaderboardButton);
      } else if (touchResult == 4) {
        // Back button: exit settings
        inSettingsMode = false;
        drawForecast(cachedLocation, forecast, settingsButton, badSurfGraphicRect, waveHeightThreshold, forecast.minTide, forecast.maxTide, currentTideDirection, currentHasTideFile);
      } else if (touchResult == 5) {
        // View files button: show files screen (handles its own input now)
        viewFilesScreen(backButton);
        drawSettingsScreen(backButton, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, filesButton, leaderboardButton);
      } else if (touchResult == 7) {
        // Leaderboard button
        showLeaderboard();
        drawSettingsScreen(backButton, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, filesButton, leaderboardButton);
      }
    } else if (inGameMode) {
      // Handle game screen - game is already running, just wait
      delay(50);
    } else {
      // Handle main screen
      int touchResult = handleMainScreenTouch(settingsButton, badSurfGraphicRect);
      if (touchResult == 3) {
        // Settings button: enter settings mode
        inSettingsMode = true;
        drawSettingsScreen(backButton, forgetButton, forgetLocationButton, themeButton, waveButton, tideButton, filesButton, leaderboardButton);
      } else if (touchResult == 6) {
        // Bad surf graphic touched: enter game mode
        inGameMode = true;
        runSurfGame(exitButton);
        // Game ended, return to main screen
        inGameMode = false;
        drawForecast(cachedLocation, forecast, settingsButton, badSurfGraphicRect, waveHeightThreshold, forecast.minTide, forecast.maxTide, currentTideDirection, currentHasTideFile);
      }
    }
    delay(50);
  }
}
