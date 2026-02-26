#include "Display.h"
#include "Config.h"
#include "Theme.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ST7796(bus, TFT_RST, 1 /* rotation */, true /* IPS */, 320, 480, 0, 0, 0, 0);

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

void drawGoodSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // Sunrise + clean peeling wave + surfer silhouette (inverted palette)
  auto invert565 = [](uint16_t c) -> uint16_t { return static_cast<uint16_t>(~c); };
  uint16_t skyColor = invert565(color);
  uint16_t seaColor = invert565(currentTheme.accent);
  uint16_t foamColor = invert565(currentTheme.text);
  uint16_t surferColor = invert565(currentTheme.background);

  // Rising sun
    // Rising sun (bitwise inverted yellow)
    uint16_t sunColor = 0x001F; // ~0xFFE0 = 0x001F (blue in RGB565)
    gfx->fillCircle(x + 20, y - 34, 16, sunColor);
    for (int i = 0; i < 7; i++) {
      float angle = (-0.25f + i * 0.25f) * PI;
      int16_t x1 = x + 20 + cos(angle) * 20;
      int16_t y1 = y - 34 + sin(angle) * 20;
      int16_t x2 = x + 20 + cos(angle) * 30;
      int16_t y2 = y - 34 + sin(angle) * 30;
      gfx->drawLine(x1, y1, x2, y2, sunColor);
    }
    // Add one more ray directly on top
    int16_t x1_top = x + 20;
    int16_t y1_top = y - 34 - 20;
    int16_t x2_top = x + 20;
    int16_t y2_top = y - 34 - 30;
    gfx->drawLine(x1_top, y1_top, x2_top, y2_top, sunColor);

  // Clean wave face and lip
  gfx->fillCircle(x - 10, y + 10, 42, seaColor);
  gfx->fillCircle(x + 8, y + 12, 34, currentTheme.background);
  gfx->drawCircle(x - 10, y + 10, 42, foamColor);
  gfx->drawCircle(x - 11, y + 10, 42, foamColor);

  // Foam streaks
  gfx->drawLine(x - 34, y + 25, x - 3, y + 8, foamColor);
  gfx->drawLine(x - 30, y + 30, x - 1, y + 16, foamColor);
  gfx->drawLine(x - 22, y + 34, x + 2, y + 22, foamColor);

  // Surfer + board
  gfx->fillCircle(x + 4, y - 6, 4, surferColor);
  gfx->drawLine(x + 4, y - 2, x + 1, y + 10, surferColor);
  gfx->drawLine(x + 1, y + 10, x + 10, y + 15, surferColor);
  gfx->drawLine(x + 2, y + 4, x + 12, y + 1, surferColor);
  gfx->drawLine(x - 6, y + 15, x + 18, y + 10, surferColor);
  gfx->drawLine(x - 5, y + 16, x + 19, y + 11, surferColor);

  // Birds in distance
  gfx->drawLine(x - 40, y - 42, x - 34, y - 46, foamColor);
  gfx->drawLine(x - 34, y - 46, x - 28, y - 42, foamColor);
  gfx->drawLine(x - 22, y - 38, x - 16, y - 42, foamColor);
  gfx->drawLine(x - 16, y - 42, x - 10, y - 38, foamColor);
}

void drawBadSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // Blown-out conditions: yellow storm cloud and choppy closeout lines.
  (void)color;
    // Storm cloud (bitwise inverted grey)
    uint16_t stormColor = 0x7BEF; // ~0x8410 = 0x7BEF (yellowish-white in RGB565)
  uint16_t chopColor = currentTheme.cloudColor;

  // Heavy storm cloud.
  gfx->fillCircle(x - 28, y - 16, 14, stormColor);
  gfx->fillCircle(x - 6, y - 22, 18, stormColor);
  gfx->fillCircle(x + 20, y - 16, 14, stormColor);
  gfx->fillRect(x - 42, y - 16, 84, 20, stormColor);
  gfx->drawLine(x - 42, y + 4, x + 42, y + 4, chopColor);

  // Closeout bars/chop.
  for (int i = 0; i < 4; i++) {
    int16_t yOff = y + 18 + i * 9;
    gfx->drawLine(x - 42, yOff, x - 26, yOff - 6, chopColor);
    gfx->drawLine(x - 26, yOff - 6, x - 10, yOff, chopColor);
    gfx->drawLine(x - 10, yOff, x + 6, yOff - 6, chopColor);
    gfx->drawLine(x + 6, yOff - 6, x + 22, yOff, chopColor);
    gfx->drawLine(x + 22, yOff, x + 42, yOff - 5, chopColor);
  }
}


void drawDirectionArrow(int16_t cx, int16_t cy, int16_t length, float degrees, uint16_t color) {
  float radians = (degrees - 90.0f) * DEG_TO_RAD;
  int16_t halfLen = length / 2;
  int16_t endX = cx + (int16_t)(cosf(radians) * halfLen);
  int16_t endY = cy + (int16_t)(sinf(radians) * halfLen);

  if (endX < 0 || endX > gfx->width() || endY < 0 || endY > gfx->height()) {
    radians += PI;
    endX = cx + (int16_t)(cosf(radians) * halfLen);
    endY = cy + (int16_t)(sinf(radians) * halfLen);
  }

  int16_t startX = cx - (int16_t)(cosf(radians) * halfLen);
  int16_t startY = cy - (int16_t)(sinf(radians) * halfLen);

  gfx->drawLine(startX, startY, endX, endY, color);
  gfx->drawLine(startX + 1, startY, endX + 1, endY, color);

  float headAngle = 25.0f * DEG_TO_RAD;
  int16_t headLen = 12;
  int16_t leftX = endX - (int16_t)(cosf(radians - headAngle) * headLen);
  int16_t leftY = endY - (int16_t)(sinf(radians - headAngle) * headLen);
  int16_t rightX = endX - (int16_t)(cosf(radians + headAngle) * headLen);
  int16_t rightY = endY - (int16_t)(sinf(radians + headAngle) * headLen);

  gfx->drawLine(endX, endY, leftX, leftY, color);
  gfx->drawLine(endX, endY, rightX, rightY, color);
}

void drawForgetButton(Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton) {
  // 2x3 grid layout in top right (added tide button below wave)
  int btnW = 48;  // 30% skinnier than original 68
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
  
  // Middle-left: Forget Location
  forgetLocationButton = {int16_t(startX), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetLocationButton, "Loc", currentTheme.buttonWarning, currentTheme.text, 1);
  
  // Middle-right: Reset Wave
  waveButton = {int16_t(startX + btnW + gap), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(waveButton, "Wave", currentTheme.buttonDanger, currentTheme.text, 1);
  
  // Bottom-right: Reset Tide
  tideButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(tideButton, "Tide", currentTheme.tideButtonColor, currentTheme.text, 1);
}

void drawForecast(const LocationInfo &location, const SurfForecast &forecast, 
                  Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton,
                  float waveHeightThreshold, float minTide, float maxTide, int tideDirection) {
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
  if (name.length() > 20) gfx->setTextSize(3);
  if (name.length() > 30) name = name.substring(0, 30) + "...";
  gfx->println(name);

  gfx->setTextColor(currentTheme.accent);
  gfx->setTextSize(3);
  gfx->setCursor(10, 86);
  gfx->println("Wave height");

  float waveHeightFeet = forecast.waveHeight * 3.28084f;
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(7);
  gfx->setCursor(10, 118);
  gfx->println(String(waveHeightFeet, 1) + " ft");

  bool happy = waveHeightFeet >= waveHeightThreshold;
  uint16_t accent = happy ? currentTheme.success : currentTheme.error;

  // Right side condition graphic moved up to leave room for arrows
  if (happy) {
    drawGoodSurfGraphic(w - 78, 130, currentTheme.accent);
  } else {
    drawBadSurfGraphic(w - 78, 130, accent);
  }

  // Middle data row
  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(3);
  gfx->setCursor(10, 200);
  gfx->println("Period");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(10, 228);
  gfx->println(String(forecast.wavePeriod, 1) + " s");

  int16_t windLabelX = 184;  // Moved 100 pixels left from 284
  int16_t windValueX = windLabelX;

  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(3);
  gfx->setCursor(windLabelX, 200);
  gfx->println("Wind");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(windValueX, 228);
  gfx->println(String(forecast.windSpeed, 1) + " mph");

  // Bottom row: arrows and tide
  const int16_t labelY = 252;
  const int16_t arrowY = 292;
  const int16_t swellCenterX = 90;
  const int16_t windCenterX = windLabelX + 65;  // Will be 249

  drawDirectionArrow(swellCenterX, arrowY, 42, forecast.waveDirection + 180.0f, YELLOW);
  uint16_t windArrowColor = darkMode ? BLACK : WHITE;
  drawDirectionArrow(windCenterX, arrowY, 42, forecast.windDirection + 180.0f, windArrowColor);

  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(3);
  gfx->setCursor(64, 360);
  gfx->println(String(forecast.waveDirection, 0) + (char)248);

  gfx->setTextColor(currentTheme.accent);
  gfx->setCursor(275, 360);  // Moved 100 pixels left from 375
  gfx->println(String(forecast.windDirection, 0) + (char)248);

  // Tide bar (vertical) - positioned to the right of wind arrow
  const int16_t tideX = 420;  // Moved right 15 pixels
  const int16_t tideBarY = 195;  // Moved down 10 pixels
  const int16_t tideBarW = 45;  // 10% wider than 41
  const int16_t tideBarH = 96;  // 20% less than 120
  
  // Draw tide bar outline with mode-appropriate color
  uint16_t tideOutlineColor = darkMode ? BLACK : WHITE;
  gfx->drawRect(tideX, tideBarY, tideBarW, tideBarH, tideOutlineColor);
  
  // Normalize tide height to 0-1 range using actual min/max
  float tideRange = maxTide - minTide;
  float tideNormalized = 0.5f;  // Default to middle
  if (tideRange > 0.01f) {  // Avoid division by zero
    tideNormalized = (forecast.tideHeight - minTide) / tideRange;
    tideNormalized = max(0.0f, min(1.0f, tideNormalized));
  }
  
  // Fill the bar from bottom to top based on tide height with mode-appropriate color
  uint16_t tideFillColor = darkMode ? YELLOW : 0xFD20;  // Yellow in dark mode, orange in light mode
  int16_t fillHeight = (int16_t)(tideNormalized * (tideBarH - 4));
  if (fillHeight > 0) {
    gfx->fillRect(tideX + 2, tideBarY + tideBarH - 2 - fillHeight, tideBarW - 4, fillHeight, tideFillColor);
  }
  
  // Draw "TIDE" text vertically centered in the tide bar
  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(1);
  const char* tideText = "TIDE";
  int16_t charSpacing = 10;
  int16_t charWidth = 6;  // Font size 1 character width
  int16_t textHeight = 4 * charSpacing;  // Total height of 4 characters
  int16_t startY = tideBarY + (tideBarH - textHeight) / 2;
  int16_t textX = tideX + (tideBarW - charWidth) / 2;
  for (int i = 0; i < 4; i++) {
    gfx->setCursor(textX, startY + i * charSpacing);
    gfx->print(tideText[i]);
  }
  
  // Draw tide direction arrow
  if (tideDirection != 0) {
    int16_t arrowCenterX = tideX + tideBarW / 2;
    int16_t arrowSize = 5;
    uint16_t arrowColor = currentTheme.periodDirTextColor;
    
    if (tideDirection > 0) {
      // Rising tide - arrow above "TIDE" text
      int16_t arrowY = startY - 8;
      // Draw upward pointing triangle
      gfx->fillTriangle(arrowCenterX, arrowY - arrowSize,
                       arrowCenterX - arrowSize, arrowY + arrowSize,
                       arrowCenterX + arrowSize, arrowY + arrowSize,
                       arrowColor);
    } else {
      // Falling tide - arrow below "TIDE" text
      int16_t arrowY = startY + textHeight + 8;
      // Draw downward pointing triangle
      gfx->fillTriangle(arrowCenterX, arrowY + arrowSize,
                       arrowCenterX - arrowSize, arrowY - arrowSize,
                       arrowCenterX + arrowSize, arrowY - arrowSize,
                       arrowColor);
    }
  }
  
  // Display tide height in feet below the bar
  float tideHeightFeet = forecast.tideHeight * 3.28084f;
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(2);
  gfx->setCursor(tideX - 5, tideBarY + tideBarH + 8);
  gfx->println(String(tideHeightFeet, 1) + "ft");

  drawForgetButton(forgetButton, forgetLocationButton, themeButton, waveButton, tideButton);
}
