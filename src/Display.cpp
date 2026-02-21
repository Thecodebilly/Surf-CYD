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
  
  // Cloud body (three overlapping circles)
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
  if (name.length() > 20) gfx->setTextSize(3);
  
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

  drawForgetButton(forgetButton, forgetLocationButton, themeButton, waveButton);
}
