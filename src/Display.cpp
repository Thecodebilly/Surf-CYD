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
  // Sunrise + clean peeling wave + surfer silhouette
  uint16_t skyColor = color;
  uint16_t seaColor = currentTheme.accent;
  uint16_t foamColor = currentTheme.text;
  uint16_t surferColor = currentTheme.background;

  // Rising sun
  gfx->fillCircle(x + 20, y - 34, 16, skyColor);
  for (int i = 0; i < 7; i++) {
    float angle = (-0.25f + i * 0.25f) * PI;
    int16_t x1 = x + 20 + cos(angle) * 20;
    int16_t y1 = y - 34 + sin(angle) * 20;
    int16_t x2 = x + 20 + cos(angle) * 30;
    int16_t y2 = y - 34 + sin(angle) * 30;
    gfx->drawLine(x1, y1, x2, y2, skyColor);
  }

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
  // Storm front + choppy closeout + warning buoy
  uint16_t stormColor = color;
  uint16_t chopColor = currentTheme.cloudColor;
  uint16_t lightningColor = currentTheme.text;
  uint16_t buoyColor = currentTheme.background;

  // Dark cloud shelf
  gfx->fillCircle(x - 24, y - 10, 16, stormColor);
  gfx->fillCircle(x - 2, y - 18, 20, stormColor);
  gfx->fillCircle(x + 24, y - 10, 16, stormColor);
  gfx->fillRect(x - 40, y - 10, 80, 20, stormColor);

  // Lightning bolt
  gfx->fillTriangle(x - 4, y + 2, x + 10, y + 2, x - 2, y + 24, lightningColor);
  gfx->fillTriangle(x - 2, y + 24, x + 7, y + 24, x - 9, y + 44, lightningColor);

  // Choppy wave lines
  for (int i = 0; i < 4; i++) {
    int16_t yOff = y + 20 + i * 8;
    gfx->drawLine(x - 40, yOff, x - 24, yOff - 5, chopColor);
    gfx->drawLine(x - 24, yOff - 5, x - 8, yOff, chopColor);
    gfx->drawLine(x - 8, yOff, x + 8, yOff - 5, chopColor);
    gfx->drawLine(x + 8, yOff - 5, x + 24, yOff, chopColor);
    gfx->drawLine(x + 24, yOff, x + 40, yOff - 4, chopColor);
  }

  // Warning buoy with X mark
  gfx->fillCircle(x - 28, y + 38, 8, buoyColor);
  gfx->drawLine(x - 32, y + 34, x - 24, y + 42, stormColor);
  gfx->drawLine(x - 24, y + 34, x - 32, y + 42, stormColor);
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

void drawForgetButton(Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton) {
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

void drawForecast(const LocationInfo &location, const SurfForecast &forecast, 
                  Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton,
                  float waveHeightThreshold) {
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

  int16_t windLabelX = 234;
  int16_t windValueX = windLabelX;

  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(3);
  gfx->setCursor(windLabelX, 200);
  gfx->println("Wind");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(windValueX, 228);
  gfx->println(String(forecast.windSpeed, 1) + " mph");

  // Bottom two-arrow strip
  const int16_t labelY = 252;
  const int16_t arrowY = 292;
  const int16_t swellCenterX = 90;
  const int16_t windCenterX = windLabelX + 65;

  drawDirectionArrow(swellCenterX, arrowY, 42, forecast.waveDirection + 180.0f, YELLOW);
  drawDirectionArrow(windCenterX, arrowY, 42, forecast.windDirection + 180.0f, BLACK);

  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(3);
  gfx->setCursor(64, 360);
  gfx->println(String(forecast.waveDirection, 0) + (char)248);

  gfx->setTextColor(currentTheme.accent);
  gfx->setCursor(325, 360);
  gfx->println(String(forecast.windDirection, 0) + (char)248);

  drawForgetButton(forgetButton, forgetLocationButton, themeButton, waveButton);
}
