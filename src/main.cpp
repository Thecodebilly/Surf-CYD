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

  // Tide direction: compare current height to stored snapshot, update snapshot every 30 min
  time_t currentTime = time(nullptr);
  currentHasTideFile = SPIFFS.exists(TIDE_DIRECTION_FILE);

  if (!currentHasTideFile) {
    // No snapshot yet — backdate the timestamp by one refresh interval so the
    // very next cycle has a 30-min comparison window and can show an arrow.
    time_t backdatedTime = currentTime - (REFRESH_INTERVAL_MS / 1000);
    saveTideDirection(forecast.tideHeight, backdatedTime, 0);
    currentTideDirection = 0;
    currentHasTideFile = true;  // file now exists; arrow will evaluate next cycle
    logInfo("Tide snapshot seeded (backdated " + String(REFRESH_INTERVAL_MS / 60000) + " min)");
  } else {
    float storedHeight = 0.0f;
    time_t storedTimestamp = 0;
    int storedDir = 0;
    loadTideDirection(storedHeight, storedTimestamp, storedDir);

    // Determine direction from change since last snapshot
    float heightChange = forecast.tideHeight - storedHeight;
    if (heightChange > 0.01f) {
      currentTideDirection = 1;   // Rising
      logInfo("Tide rising: +" + String(heightChange, 3) + "m since snapshot");
    } else if (heightChange < -0.01f) {
      currentTideDirection = -1;  // Falling
      logInfo("Tide falling: " + String(heightChange, 3) + "m since snapshot");
    } else {
      currentTideDirection = 0;   // Slack
      logInfo("Slack tide: " + String(heightChange, 3) + "m since snapshot");
    }

    // Update snapshot every 30 minutes
    int minsSinceSnapshot = (int)((currentTime - storedTimestamp) / 60);
    if (minsSinceSnapshot >= 30) {
      saveTideDirection(forecast.tideHeight, currentTime, currentTideDirection);
      logInfo("Tide snapshot updated (" + String(minsSinceSnapshot) + " min elapsed)");
    }
  }

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
