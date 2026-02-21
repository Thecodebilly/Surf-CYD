#include "TouchUI.h"
#include "Config.h"
#include "Theme.h"
#include "Display.h"
#include "Storage.h"
#include "Network.h"
#include <WiFi.h>
#include <SPI.h>

XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

void setupTouch() {
  // Initialize SPI bus for touchscreen (shares same pins as display)
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TOUCH_CS);
  touch.begin();
  // No rotation - we handle mapping manually
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

String touchKeyboardInput(const String &title, const String &initial, bool secret) {
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

      drawButton(ssidButton, "SSID: " + (creds.ssid.isEmpty() ? String("<tap to set>") : creds.ssid), 
                 currentTheme.buttonSecondary, currentTheme.text, 1);
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

String runLocationSetupTouch(LocationInfo &cachedLocation) {
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

int handleMainScreenTouch(const Rect &forgetButton, const Rect &forgetLocationButton, 
                          const Rect &themeButton, const Rect &waveButton,
                          String &surfLocation, LocationInfo &cachedLocation, 
                          float &waveHeightThreshold) {
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
