#include "Display.h"
#include "Config.h"
#include "Theme.h"
#include "TouchUI.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>

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

// Blend two theme colors (values are stored pre-inverted for the panel, so mix
// in intended-color space and re-invert). t: 0 = a, 255 = b.
static uint16_t mixTheme(uint16_t aInv, uint16_t bInv, uint8_t t) {
  uint16_t a = aInv ^ 0xFFFF, b = bInv ^ 0xFFFF;
  uint8_t rA = (a >> 11) & 0x1F, gA = (a >> 5) & 0x3F, bA = a & 0x1F;
  uint8_t rB = (b >> 11) & 0x1F, gB = (b >> 5) & 0x3F, bB = b & 0x1F;
  uint8_t r  = rA + (int16_t)(rB - rA) * t / 255;
  uint8_t g  = gA + (int16_t)(gB - gA) * t / 255;
  uint8_t bl = bA + (int16_t)(bB - bA) * t / 255;
  return (uint16_t)(((r << 11) | (g << 5) | bl) ^ 0xFFFF);
}

void drawCenteredText(const String &text, int16_t centerX, int16_t y, uint8_t textSize, uint16_t color) {
  int16_t w = (int16_t)text.length() * 6 * textSize;
  gfx->setTextColor(color);
  gfx->setTextSize(textSize);
  gfx->setCursor(centerX - w / 2, y);
  gfx->print(text);
}

void drawScreenHeader(const String &title) {
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(3);
  gfx->setCursor(12, 14);
  gfx->print(title);
  gfx->drawFastHLine(12, 47, gfx->width() - 24, currentTheme.border);
  gfx->fillRect(12, 45, 52, 4, currentTheme.accent);
}

void drawButton(const Rect &r, const String &label, uint16_t bg, uint16_t fg = BLACK, uint8_t textSize = 2) {
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 6, bg);
  // Outline blended from the fill toward the theme hairline so every chip has
  // definition on both dark and light surfaces.
  gfx->drawRoundRect(r.x, r.y, r.w, r.h, 6, mixTheme(bg, currentTheme.border, 150));

  String text = label;
  int16_t maxChars = (r.w - 6) / (6 * textSize);
  if ((int16_t)text.length() > maxChars && maxChars > 0) text = text.substring(0, maxChars);

  int16_t tw = (int16_t)text.length() * 6 * textSize;
  int16_t tx = r.x + (r.w - tw) / 2;
  if (tx < r.x + 3) tx = r.x + 3;
  int16_t ty = r.y + (r.h - 8 * textSize) / 2 + 1;

  gfx->setTextColor(fg);
  gfx->setTextSize(textSize);
  gfx->setCursor(tx, ty);
  gfx->print(text);
}

void showStatus(const String &line1, const String &line2 = "", uint16_t color = WHITE) {
  gfx->fillScreen(currentTheme.background);
  int16_t cx = gfx->width() / 2;
  int16_t cy = gfx->height() / 2;
  gfx->fillRect(cx - 26, cy - 58, 52, 4, currentTheme.accent);
  drawCenteredText(line1, cx, cy - 36, 3, color);
  if (!line2.isEmpty()) {
    drawCenteredText(line2, cx, cy + 6, 2, currentTheme.textSecondary);
  }
}

void drawGoodSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // Sunrise + clean peeling wave + surfer silhouette, in theme colors
  uint16_t seaColor    = currentTheme.cloudColor;
  uint16_t foamColor   = currentTheme.text;
  uint16_t surferColor = currentTheme.text;
  uint16_t sunColor    = currentTheme.accent;

  // Rising sun
  gfx->fillCircle(x + 22, y - 37, 18, sunColor);
  for (int i = 0; i < 7; i++) {
    float angle = (-0.25f + i * 0.25f) * PI;
    int16_t x1 = x + 22 + cos(angle) * 22;
    int16_t y1 = y - 37 + sin(angle) * 22;
    int16_t x2 = x + 22 + cos(angle) * 33;
    int16_t y2 = y - 37 + sin(angle) * 33;
    gfx->drawLine(x1, y1, x2, y2, sunColor);
  }
  gfx->drawLine(x + 22, y - 59, x + 22, y - 70, sunColor);

  // Clean wave face and lip
  gfx->fillCircle(x - 11, y + 11, 46, seaColor);
  gfx->fillCircle(x + 9, y + 13, 37, currentTheme.background);
  gfx->drawCircle(x - 11, y + 11, 46, foamColor);
  gfx->drawCircle(x - 12, y + 11, 46, foamColor);

  // Foam streaks
  gfx->drawLine(x - 37, y + 28, x - 3, y + 9, foamColor);
  gfx->drawLine(x - 33, y + 33, x - 1, y + 18, foamColor);
  gfx->drawLine(x - 24, y + 37, x + 2, y + 24, foamColor);

  // Surfer + board
  gfx->fillCircle(x + 4, y - 7, 4, surferColor);
  gfx->drawLine(x + 4, y - 2, x + 1, y + 11, surferColor);
  gfx->drawLine(x + 1, y + 11, x + 11, y + 17, surferColor);
  gfx->drawLine(x + 2, y + 4, x + 13, y + 1, surferColor);
  gfx->drawLine(x - 7, y + 17, x + 20, y + 11, surferColor);
  gfx->drawLine(x - 6, y + 18, x + 21, y + 12, surferColor);

  // Birds in distance
  gfx->drawLine(x - 44, y - 46, x - 37, y - 51, currentTheme.textSecondary);
  gfx->drawLine(x - 37, y - 51, x - 31, y - 46, currentTheme.textSecondary);
  gfx->drawLine(x - 24, y - 42, x - 18, y - 46, currentTheme.textSecondary);
  gfx->drawLine(x - 18, y - 46, x - 11, y - 42, currentTheme.textSecondary);
}

void drawBadSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // "No surf" visual: surfboard icon behind a prohibition ring/slash.
  uint16_t boardColor  = currentTheme.buttonList;
  uint16_t noSurfColor = currentTheme.error;
  uint16_t finColor    = currentTheme.border;

  // Prohibition ring: draw as filled outer disc then punch out interior with
  // background so the ring is perfectly solid (no diagonal gaps from drawCircle).
  int16_t ringRadius = 56;
  gfx->fillCircle(x, y + 8, ringRadius + 1, noSurfColor);
  gfx->fillCircle(x, y + 8, ringRadius - 3, currentTheme.background);

  // Surfboard silhouette drawn on top of the (now hollow) ring interior.
  gfx->fillCircle(x, y - 24, 17, boardColor);
  gfx->fillRect(x - 17, y - 24, 35, 59, boardColor);
  gfx->fillCircle(x, y + 35, 17, boardColor);

  // Twin fins
  gfx->fillTriangle(x - 13, y + 38, x - 8, y + 38, x - 11, y + 54, finColor);
  gfx->fillTriangle(x + 8, y + 38, x + 13, y + 38, x + 11, y + 54, finColor);

  // Board stringer
  gfx->drawLine(x, y - 37, x, y + 48, currentTheme.border);

  // Diagonal slash — drawn as filled trapezoid (two triangles) for solid look
  gfx->fillTriangle(x - 41, y + 44,  x - 35, y + 50,  x + 45, y - 26, noSurfColor);
  gfx->fillTriangle(x - 41, y + 44,  x + 45, y - 26,  x + 39, y - 32, noSurfColor);

  // The graphic doubles as the game launcher — hint at it.
  drawCenteredText("tap to play", x, y + 72, 1, currentTheme.textSecondary);
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
  drawButton(forgetButton, "WiFi", currentTheme.buttonWarning, currentTheme.text, 1);

  // Top-right: Theme toggle
  themeButton = {int16_t(startX + btnW + gap), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(themeButton, darkMode ? "Light" : "Dark", currentTheme.text, currentTheme.background, 1);

  // Middle-left: Forget Location
  forgetLocationButton = {int16_t(startX), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetLocationButton, "Loc", currentTheme.buttonWarning, currentTheme.text, 1);

  // Middle-right: Reset Wave
  waveButton = {int16_t(startX + btnW + gap), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(waveButton, "Wave", currentTheme.buttonWarning, currentTheme.text, 1);

  // Bottom-right: Reset All
  tideButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(tideButton, "Reset", currentTheme.buttonDanger, currentTheme.text, 1);
}

void drawSettingsButton(Rect &settingsButton) {
  // Single settings button in top right corner
  int btnW = 60;
  int btnH = 24;
  int startX = gfx->width() - btnW - 5;
  int startY = 5;

  settingsButton = {int16_t(startX), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(settingsButton, "Settings", currentTheme.buttonSecondary, currentTheme.text, 1);
}

void drawSettingsScreen(Rect &backButton, Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton, Rect &filesButton, Rect &leaderboardButton) {
  gfx->fillScreen(currentTheme.background);

  drawScreenHeader("Settings");

  // Button layout - centered grid
  int btnW = 185;
  int btnH = 40;
  int gap = 10;
  int startX = (gfx->width() - (btnW * 2 + gap)) / 2;
  int startY = 80;

  // Row 1
  forgetButton = {int16_t(startX), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetButton, "Reset WiFi", currentTheme.buttonWarning, currentTheme.text, 2);

  themeButton = {int16_t(startX + btnW + gap), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(themeButton, darkMode ? "Light Mode" : "Dark Mode", currentTheme.text, currentTheme.background, 2);

  // Row 2
  forgetLocationButton = {int16_t(startX), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetLocationButton, "Reset Location", currentTheme.buttonWarning, currentTheme.text, 2);

  waveButton = {int16_t(startX + btnW + gap), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(waveButton, "Reset Wave", currentTheme.buttonWarning, currentTheme.text, 2);

  // Row 3: Leaderboard | View Files
  leaderboardButton = {int16_t(startX), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(leaderboardButton, "Leaderboard", currentTheme.buttonList, currentTheme.text, 2);

  filesButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(filesButton, "View Files", currentTheme.buttonList, currentTheme.text, 2);

  // Row 4: Reset All | Back
  tideButton = {int16_t(startX), int16_t(startY + (btnH + gap) * 3), int16_t(btnW), int16_t(btnH)};
  drawButton(tideButton, "Reset All", currentTheme.buttonDanger, currentTheme.text, 2);

  backButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 3), int16_t(btnW), int16_t(btnH)};
  drawButton(backButton, "< Back", currentTheme.buttonSecondary, currentTheme.text, 2);

  // Website credit at very bottom
  drawCenteredText("billy-shaw.com", gfx->width() / 2, gfx->height() - 12, 1, currentTheme.border);
}

void drawForecast(const LocationInfo &location, const SurfForecast &forecast,
                  Rect &settingsButton, Rect &badSurfGraphicRect,
                  float waveHeightThreshold, float minTide, float maxTide, int tideDirection, bool hasTideFile) {
  Serial.printf("[DISPLAY] drawForecast: tideH=%.3fm (%.2fft), min=%.3fm (%.2fft), max=%.3fm (%.2fft), dir=%d\n",
    forecast.tideHeight, forecast.tideHeight * 3.28084f,
    minTide, minTide * 3.28084f,
    maxTide, maxTide * 3.28084f,
    tideDirection);
  gfx->fillScreen(currentTheme.background);
  int16_t w = gfx->width();

  // ── Header: overline + location name ──
  gfx->fillRect(12, 12, 4, 8, currentTheme.accent);
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(1);
  gfx->setCursor(22, 12);
  gfx->println("SURF SPOT");

  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(4);
  gfx->setCursor(12, 26);
  String name = location.displayName;
  if (name.length() > 10) gfx->setTextSize(3);
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
  if (name.length() > 30) name = name.substring(0, 30) + "...";
  gfx->println(name);

  gfx->drawFastHLine(12, 62, 268, currentTheme.border);

  // ── Hero: wave height ──
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(2);
  gfx->setCursor(12, 76);
  gfx->println("WAVE HEIGHT");

  float waveHeightFeet = forecast.waveHeight * 3.28084f;
  String waveStr = String(waveHeightFeet, 1);
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(7);
  gfx->setCursor(12, 98);
  gfx->print(waveStr);
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(3);
  gfx->setCursor(12 + (int16_t)waveStr.length() * 42 + 8, 98 + 56 - 24);
  gfx->print("ft");

  bool happy = waveHeightFeet >= waveHeightThreshold;
  uint16_t accent = happy ? currentTheme.success : currentTheme.error;

  // Right side condition graphic moved up to leave room for arrows
  if (happy) {
    drawGoodSurfGraphic(w - 113, 130, currentTheme.accent);
    badSurfGraphicRect = {0, 0, 0, 0}; // No bad surf graphic
  } else {
    int16_t badSurfX = w - 133;
    int16_t badSurfY = 140;
    drawBadSurfGraphic(badSurfX, badSurfY, accent);
    // Set touchable area around the bad surf graphic (roughly 100x100 box)
    badSurfGraphicRect = {int16_t(badSurfX - 50), int16_t(badSurfY - 50), 100, 100};
  }

  // ── Middle data row: period + wind ──
  gfx->drawFastHLine(12, 192, 268, currentTheme.border);

  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(2);
  gfx->setCursor(12, 204);
  gfx->println("PERIOD");
  String periodStr = String(forecast.wavePeriod, 1);
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(12, 224);
  gfx->print(periodStr);
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(3);
  gfx->setCursor(12 + (int16_t)periodStr.length() * 30 + 4, 224 + 40 - 24);
  gfx->print("s");

  const int16_t windLabelX = 170;
  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(2);
  gfx->setCursor(windLabelX, 204);
  gfx->println("WIND");
  String windStr = String(forecast.windSpeed, 1);
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(windLabelX, 224);
  gfx->print(windStr);
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(2);
  gfx->setCursor(windLabelX + (int16_t)windStr.length() * 30 + 4, 224 + 40 - 16);
  gfx->print("mph");

  // ── Bottom row: direction compasses ──
  const int16_t arrowY = 292;
  const int16_t swellCenterX = 70;
  const int16_t windCenterX = 249;

  gfx->drawCircle(swellCenterX, arrowY, 23, currentTheme.border);
  gfx->drawCircle(windCenterX, arrowY, 23, currentTheme.border);
  drawDirectionArrow(swellCenterX, arrowY, 36, forecast.waveDirection + 180.0f, currentTheme.accent);
  drawDirectionArrow(windCenterX, arrowY, 36, forecast.windDirection + 180.0f, currentTheme.text);

  // Cardinal + caption beside each compass
  auto degreesToCardinal = [](float deg) -> const char* {
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    int idx = (int)((deg + 22.5f) / 45.0f) % 8;
    const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[idx];
  };
  gfx->setTextSize(2);
  gfx->setTextColor(currentTheme.text);
  gfx->setCursor(swellCenterX + 30, 278);
  gfx->print(degreesToCardinal(forecast.waveDirection));
  gfx->setCursor(windCenterX + 30, 278);
  gfx->print(degreesToCardinal(forecast.windDirection));
  gfx->setTextSize(1);
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setCursor(swellCenterX + 30, 298);
  gfx->print("SWELL");
  gfx->setCursor(windCenterX + 30, 298);
  gfx->print("WIND");

  // ── Tide meter (right column) ──
  const int16_t tideX    = 415;
  const int16_t tideBarY = 185;
  const int16_t tideBarW = 45;
  const int16_t tideBarH = 96;

  float tideHeightFeet = forecast.tideHeight * 3.28084f;
  float minTideFeet    = minTide * 3.28084f;
  float maxTideFeet    = maxTide * 3.28084f;

  // Check if tide data is unavailable (all zero)
  bool tideUnavailable = (forecast.tideHeight == 0.0f && minTide == 0.0f && maxTide == 0.0f);
  if (tideUnavailable) {
    Serial.println("[DISPLAY] Tide data unavailable: tideHeight, minTide, and maxTide are all 0");
  }

  // Normalise current position within today's low→high range (0=low, 1=high)
  float tideRange = maxTide - minTide;
  float tideNormalized = 0.0f;
  if (!tideUnavailable && tideRange > 0.01f) {
    tideNormalized = (forecast.tideHeight - minTide) / tideRange;
    tideNormalized = max(0.0f, min(1.0f, tideNormalized));
  }

  drawCenteredText("TIDE", tideX + tideBarW / 2, 160, 2, currentTheme.textSecondary);

  gfx->drawRoundRect(tideX, tideBarY, tideBarW, tideBarH, 4, currentTheme.border);
  if (!tideUnavailable) {
    int16_t fillHeight = (int16_t)(tideNormalized * (tideBarH - 4));
    if (fillHeight > 0) {
      gfx->fillRect(tideX + 2, tideBarY + tideBarH - 2 - fillHeight, tideBarW - 4, fillHeight, currentTheme.cloudColor);
      // Water line marking the current level
      gfx->fillRect(tideX + 2, tideBarY + tideBarH - 2 - fillHeight, tideBarW - 4, 2, currentTheme.text);
    }
  }

  if (tideUnavailable) {
    drawCenteredText("no tide", tideX + tideBarW / 2, tideBarY + tideBarH + 6, 1, currentTheme.textSecondary);
    drawCenteredText("data", tideX + tideBarW / 2, tideBarY + tideBarH + 16, 1, currentTheme.textSecondary);
  } else {
    // H / L bounds hugging the bar
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(1);
    gfx->setCursor(tideX + 1, tideBarY - 9);
    gfx->print("H " + String(maxTideFeet, 1));
    gfx->setCursor(tideX + 1, tideBarY + tideBarH + 4);
    gfx->print("L " + String(minTideFeet, 1));

    // Current tide + trend arrow below the bar.
    // Arrow only drawn if the tide snapshot file exists.
    String nowStr = String(tideHeightFeet, 1) + "ft";
    int16_t nowW = (int16_t)nowStr.length() * 12;
    int16_t nowX = tideX + (tideBarW - nowW) / 2;
    if (nowX < 402) nowX = 402;
    gfx->setTextColor(currentTheme.text);
    gfx->setTextSize(2);
    gfx->setCursor(nowX, tideBarY + tideBarH + 16);
    gfx->print(nowStr);

    if (hasTideFile && tideDirection != 0) {
      int16_t arrowCX = nowX - 10;
      int16_t arrowSz = 5;
      int16_t midY = tideBarY + tideBarH + 16 + 7;
      if (tideDirection > 0) {
        gfx->fillTriangle(arrowCX,           midY - arrowSz,
                          arrowCX - arrowSz, midY + arrowSz,
                          arrowCX + arrowSz, midY + arrowSz, currentTheme.accent);
      } else {
        gfx->fillTriangle(arrowCX,           midY + arrowSz,
                          arrowCX - arrowSz, midY - arrowSz,
                          arrowCX + arrowSz, midY - arrowSz, currentTheme.accent);
      }
      // Slack (direction == 0): no arrow drawn
    }
  }

  drawSettingsButton(settingsButton);
}

void viewFilesScreen(Rect &backButton) {
  // Collect all file info first
  struct FileInfo {
    String name;
    String content;
  };

  std::vector<FileInfo> files;
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    FileInfo info;
    info.name = String(file.name());
    info.content = "";
    while (file.available() && info.content.length() < 500) {
      info.content += (char)file.read();
    }
    files.push_back(info);
    file = root.openNextFile();
  }

  int scrollOffset = 0;
  const int lineHeight = 10;
  const int headerHeight = 30;
  const int footerHeight = 45;
  const int contentHeight = 320 - headerHeight - footerHeight;
  const int maxLines = contentHeight / lineHeight;

  // Calculate total content height
  int totalLines = 0;
  for (const auto& f : files) {
    totalLines += 1; // filename
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, f.content);
    if (!error) {
      JsonObject obj = doc.as<JsonObject>();
      for (JsonPair kv : obj) {
        totalLines++;
      }
    } else if (f.content.length() > 0) {
      totalLines++;
    }
    totalLines++; // gap
  }

  bool needsRedraw = true;

  while (true) {
    if (needsRedraw) {
      gfx->fillScreen(currentTheme.background);

      // Title
      gfx->setTextColor(currentTheme.text);
      gfx->setTextSize(2);
      gfx->setCursor(12, 6);
      gfx->println("Stored Files");
      gfx->fillRect(12, 25, 36, 3, currentTheme.accent);

      // Show scroll indicator if needed
      if (totalLines > maxLines) {
        gfx->setTextColor(currentTheme.textSecondary);
        gfx->setTextSize(1);
        gfx->setCursor(390, 8);
        gfx->print(scrollOffset / lineHeight);
        gfx->print("/");
        gfx->print((totalLines - maxLines));
      }

      // Draw content with scroll offset
      gfx->setTextSize(1);
      int16_t y = headerHeight;
      int currentLine = 0;

      for (const auto& f : files) {
        // File name header
        if (currentLine >= scrollOffset / lineHeight && y < headerHeight + contentHeight) {
          gfx->setTextColor(currentTheme.accent);
          gfx->setCursor(5, y);
          gfx->print(">");
          gfx->setTextColor(currentTheme.text);
          gfx->setCursor(12, y);
          gfx->println(f.name);
          y += lineHeight;
        }
        currentLine++;

        // Parse and display content
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, f.content);

        if (!error) {
          JsonObject obj = doc.as<JsonObject>();
          for (JsonPair kv : obj) {
            if (currentLine >= scrollOffset / lineHeight && y < headerHeight + contentHeight) {
              gfx->setCursor(15, y);
              gfx->setTextColor(currentTheme.textSecondary);
              gfx->print(String(kv.key().c_str()) + ":");

              gfx->setTextColor(currentTheme.text);
              String value = kv.value().as<String>();
              if (value.length() > 30) value = value.substring(0, 27) + "...";
              gfx->print(" " + value);
              y += lineHeight;
            }
            currentLine++;
          }
        } else if (f.content.length() > 0) {
          if (currentLine >= scrollOffset / lineHeight && y < headerHeight + contentHeight) {
            gfx->setTextColor(currentTheme.text);
            gfx->setCursor(15, y);
            String content = f.content;
            if (content.length() > 45) content = content.substring(0, 42) + "...";
            gfx->println(content);
            y += lineHeight;
          }
          currentLine++;
        }
        currentLine++; // gap
      }

      if (files.size() == 0) {
        gfx->setCursor(10, 50);
        gfx->setTextColor(currentTheme.textSecondary);
        gfx->println("No files found");
      }

      // Back button at bottom
      int btnW = 140;
      int btnH = 40;
      int startX = (gfx->width() - btnW) / 2;
      backButton = {int16_t(startX), int16_t(320 - btnH - 5), int16_t(btnW), int16_t(btnH)};
      drawButton(backButton, "< Back", currentTheme.buttonSecondary, currentTheme.text, 2);

      needsRedraw = false;
    }

    // Handle touch for scrolling or back button
    TouchPoint p = getTouchPoint();
    if (p.pressed) {
      if (pointInRect(p.x, p.y, backButton)) {
        while (touch.touched()) delay(20);
        return;
      }

      // Scroll up/down based on touch position
      if (p.y < 160 && scrollOffset > 0) {
        // Scroll up
        scrollOffset -= lineHeight * 3;
        if (scrollOffset < 0) scrollOffset = 0;
        needsRedraw = true;
      } else if (p.y >= 160 && scrollOffset < (totalLines - maxLines) * lineHeight) {
        // Scroll down
        scrollOffset += lineHeight * 3;
        int maxScroll = (totalLines - maxLines) * lineHeight;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
        needsRedraw = true;
      }

      while (touch.touched()) delay(20);
      delay(100);
    }

    delay(50);
  }
}

// Decorative scalloped wave band along the bottom edge of full-screen pages.
static void drawWaveBand() {
  int16_t h = gfx->height();
  for (int16_t x = -10; x < gfx->width() + 40; x += 44) {
    gfx->drawCircle(x, h + 22, 36, currentTheme.cloudColor);
    gfx->drawCircle(x, h + 21, 36, currentTheme.cloudColor);
    gfx->drawCircle(x + 22, h + 30, 36, currentTheme.border);
  }
}

void drawWelcomeScreen(Rect &setupButton) {
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();

  gfx->fillScreen(currentTheme.background);

  // Sun in the top-left corner
  gfx->fillCircle(64, 58, 22, currentTheme.accent);
  for (int i = 0; i < 8; i++) {
    float angle = i * 0.25f * PI;
    int16_t x1 = 64 + cosf(angle) * 28;
    int16_t y1 = 58 + sinf(angle) * 28;
    int16_t x2 = 64 + cosf(angle) * 38;
    int16_t y2 = 58 + sinf(angle) * 38;
    gfx->drawLine(x1, y1, x2, y2, currentTheme.accent);
  }

  drawWaveBand();

  drawCenteredText("SURF BOARD", screenWidth / 2, 82, 4, currentTheme.text);
  gfx->fillRect(screenWidth / 2 - 30, 120, 60, 4, currentTheme.accent);
  drawCenteredText("Hi! What's your name?", screenWidth / 2, 140, 2, currentTheme.textSecondary);

  // Setup button centered
  setupButton = {int16_t(screenWidth / 2 - 100), int16_t(screenHeight / 2 + 65), 200, 50};
  drawButton(setupButton, "My name is...", currentTheme.buttonPrimary, currentTheme.text, 2);
}

void drawNameConfirmScreen(const String &name, Rect &confirmButton) {
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();

  gfx->fillScreen(currentTheme.background);
  drawWaveBand();

  drawCenteredText("Your name is:", screenWidth / 2, 92, 2, currentTheme.textSecondary);

  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(4);
  int16_t namePixW = (int16_t)(name.length() * 24);
  if (namePixW > screenWidth - 20) namePixW = screenWidth - 20;
  gfx->setCursor(screenWidth / 2 - namePixW / 2, 128);
  gfx->println(name);
  gfx->fillRect(screenWidth / 2 - namePixW / 2, 164, namePixW, 4, currentTheme.accent);

  confirmButton = {int16_t(screenWidth / 2 - 75), int16_t(screenHeight / 2 + 65), 150, 50};
  drawButton(confirmButton, "Confirm", currentTheme.buttonPrimary, currentTheme.text, 2);
}
