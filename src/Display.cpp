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
  uint16_t seaColor = invert565(currentTheme.accent);
  uint16_t foamColor = invert565(currentTheme.text);
  uint16_t surferColor = invert565(currentTheme.background);

  // Rising sun (~0xFFE0 = 0x001F, blue in RGB565 after inversion)
  uint16_t sunColor = 0x001F;
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
  gfx->drawLine(x - 44, y - 46, x - 37, y - 51, foamColor);
  gfx->drawLine(x - 37, y - 51, x - 31, y - 46, foamColor);
  gfx->drawLine(x - 24, y - 42, x - 18, y - 46, foamColor);
  gfx->drawLine(x - 18, y - 46, x - 11, y - 42, foamColor);
}

void drawBadSurfGraphic(int16_t x, int16_t y, uint16_t color) {
  // "No surf" visual: surfboard icon with prohibition circle/slash.
  // Surfboard is opposite of orange in light mode, yellow-green in dark mode
  // Circle is always cyan (opposite of red)
  uint16_t boardColor = darkMode ? 0x87E0 : 0x02DF;  // yellow-green : blue (opposite of orange)
  uint16_t noSurfColor = 0x07FF;  // cyan (opposite of red)
  uint16_t finColor = currentTheme.text;

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
  gfx->drawLine(x, y - 37, x, y + 48, color);

  // Diagonal slash — drawn as filled trapezoid (two triangles) for solid look
  // Corners of an ~8px-wide band along the slash direction
  gfx->fillTriangle(x - 41, y + 44,  x - 35, y + 50,  x + 45, y - 26, noSurfColor);
  gfx->fillTriangle(x - 41, y + 44,  x + 45, y - 26,  x + 39, y - 32, noSurfColor);
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
  
  // Bottom-right: Reset All
  tideButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(tideButton, "Reset", currentTheme.tideButtonColor, currentTheme.text, 1);
}

void drawSettingsButton(Rect &settingsButton) {
  // Single settings button in top right corner
  int btnW = 60;
  int btnH = 24;
  int startX = gfx->width() - btnW - 5;
  int startY = 5;
  
  settingsButton = {int16_t(startX), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(settingsButton, "Settings", currentTheme.buttonPrimary, currentTheme.text, 1);
}

void drawSettingsScreen(Rect &backButton, Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton, Rect &filesButton, Rect &leaderboardButton) {
  gfx->fillScreen(currentTheme.background);
  
  // Title
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(4);
  gfx->setCursor(10, 20);
  gfx->println("Settings");
  
  // Button layout - centered grid
  int btnW = 185;
  int btnH = 40;
  int gap = 10;
  int startX = (gfx->width() - (btnW * 2 + gap)) / 2;
  int startY = 80;
  
  // Row 1
  forgetButton = {int16_t(startX), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetButton, "Reset WiFi", currentTheme.success, currentTheme.text, 2);
  
  themeButton = {int16_t(startX + btnW + gap), int16_t(startY), int16_t(btnW), int16_t(btnH)};
  drawButton(themeButton, darkMode ? "Light Mode" : "Dark Mode", currentTheme.text, currentTheme.background, 2);
  
  // Row 2
  forgetLocationButton = {int16_t(startX), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(forgetLocationButton, "Reset Loc", currentTheme.buttonWarning, currentTheme.text, 2);
  
  waveButton = {int16_t(startX + btnW + gap), int16_t(startY + btnH + gap), int16_t(btnW), int16_t(btnH)};
  drawButton(waveButton, "Reset Wave", currentTheme.buttonDanger, currentTheme.text, 2);
  
  // Row 3: Leaderboard | View Files
  leaderboardButton = {int16_t(startX), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(leaderboardButton, "Leaderboard", 0x015F, currentTheme.text, 2);

  filesButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 2), int16_t(btnW), int16_t(btnH)};
  drawButton(filesButton, "View Files", currentTheme.buttonList, currentTheme.text, 2);

  // Row 4: Reset All | Back
  tideButton = {int16_t(startX), int16_t(startY + (btnH + gap) * 3), int16_t(btnW), int16_t(btnH)};
  drawButton(tideButton, "Reset All", currentTheme.tideButtonColor, currentTheme.text, 2);

  backButton = {int16_t(startX + btnW + gap), int16_t(startY + (btnH + gap) * 3), int16_t(btnW), int16_t(btnH)};
  drawButton(backButton, "< Back", currentTheme.buttonSecondary, currentTheme.text, 2);

  // Website credit at very bottom
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(1);
  int16_t siteWidth = strlen("billy-shaw.com") * 6;
  gfx->setCursor(gfx->width() / 2 - siteWidth / 2, gfx->height() - 10);
  gfx->print("billy-shaw.com");
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

  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(3);
  gfx->setCursor(10, 10);
  gfx->println("Surf spot");

  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(4);
  gfx->setCursor(10, 38);
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

  gfx->setTextColor(currentTheme.accent);
  gfx->setTextSize(3);
  gfx->setCursor(20, 86);
  gfx->println("Wave height");

  float waveHeightFeet = forecast.waveHeight * 3.28084f;
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(7);
  gfx->setCursor(20, 118);
  gfx->println(String(waveHeightFeet, 1) + " ft");

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

  // Middle data row
  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(3);
  gfx->setCursor(10, 200);
  gfx->println("Period");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(10, 228);
  gfx->println(String(forecast.wavePeriod, 1) + "s");

  int16_t windLabelX = 184;  // Moved 100 pixels left from 284
  int16_t windValueX = windLabelX;

  gfx->setTextColor(currentTheme.periodDirTextColor);
  gfx->setTextSize(3);
  gfx->setCursor(windLabelX, 200);
  gfx->println("Wind");
  gfx->setTextColor(currentTheme.periodDirNumberColor);
  gfx->setTextSize(5);
  gfx->setCursor(windValueX, 228);
  gfx->println(String(forecast.windSpeed, 1) + "mph");

  // Bottom row: direction arrows
  const int16_t arrowY = 292;
  const int16_t swellCenterX = 70;
  const int16_t windCenterX = windLabelX + 65;  // 249

  drawDirectionArrow(swellCenterX, arrowY, 42, forecast.waveDirection + 180.0f, YELLOW);
  uint16_t windArrowColor = darkMode ? BLACK : WHITE;
  drawDirectionArrow(windCenterX, arrowY, 42, forecast.windDirection + 180.0f, windArrowColor);

  // Cardinal direction labels below each arrow
  auto degreesToCardinal = [](float deg) -> const char* {
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    int idx = (int)((deg + 22.5f) / 45.0f) % 8;
    const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[idx];
  };
  const char* swellCardinal = degreesToCardinal(forecast.waveDirection);
  const char* windCardinal  = degreesToCardinal(forecast.windDirection);
  gfx->setTextSize(1);
  gfx->setTextColor(YELLOW);
  gfx->setCursor(swellCenterX - 34 - (int16_t)(strlen(swellCardinal) * 6), arrowY - 11);
  gfx->print(swellCardinal);
  gfx->setTextColor(windArrowColor);
  gfx->setCursor(windCenterX - 34 - (int16_t)(strlen(windCardinal) * 6), arrowY - 11);
  gfx->print(windCardinal);

  // Tide bar (vertical)
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

  // Outline + fill — cloudColor is pre-inverted by applyTheme() → renders as
  // light sky blue (RGB ~74,190,255) in dark mode, dark blue in light mode
  uint16_t tideOutlineColor = currentTheme.text;
  uint16_t tideFillColor    = currentTheme.cloudColor;
  gfx->drawRect(tideX, tideBarY, tideBarW, tideBarH, tideOutlineColor);
  if (!tideUnavailable) {
    int16_t fillHeight = (int16_t)(tideNormalized * (tideBarH - 4));
    if (fillHeight > 0) {
      gfx->fillRect(tideX + 2, tideBarY + tideBarH - 2 - fillHeight, tideBarW - 4, fillHeight, tideFillColor);
    }
  }

  // "TIDE" text vertically centered in bar, centered horizontally as a 4-char block
  gfx->setTextColor(currentTheme.text);
  gfx->setTextSize(1);
  const char *tideText    = "TIDE";
  const int16_t charSpacing = 10;
  const int16_t charWidth   = 6;   // pixels wide per char at textSize 1
  const int16_t textHeight  = 4 * charSpacing;
  int16_t tideTextStartY  = tideBarY + (tideBarH - textHeight) / 2;
  // Center the single-char column exactly in the bar
  int16_t tideTextX       = tideX + (tideBarW / 2) - (charWidth / 2);
  for (int i = 0; i < 4; i++) {
    gfx->setCursor(tideTextX, tideTextStartY + i * charSpacing);
    gfx->print(tideText[i]);
  }

  if (tideUnavailable) {
    // Show "N/A" below bar and skip H/L labels
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(1);
    gfx->setCursor(tideX - 11, tideBarY + tideBarH + 4);
    gfx->print("Tide info");
    gfx->setCursor(tideX - 14, tideBarY + tideBarH + 14);
    gfx->print("not avail.");
  } else {
    // Direction arrow — only drawn if the tide snapshot file exists.
    // Black in dark mode, white in light mode (WHITE/BLACK constants are stored
    // pre-inverted for hardware, so they render correctly on screen).
    if (hasTideFile) {
      int16_t arrowCX = tideX + (tideBarW / 2);
      int16_t arrowSz = 5;
      uint16_t arrowCol = darkMode ? BLACK : WHITE;
      const int16_t gap = 3;
      const int16_t upShift = 4; // move arrows 4px further from text
      if (tideDirection > 0) {
        // Up arrow — shifted 4px higher than before
        int16_t baseY = tideTextStartY - gap - upShift;
        int16_t tipY  = baseY - arrowSz;
        gfx->fillTriangle(arrowCX,           tipY,
                          arrowCX - arrowSz, baseY,
                          arrowCX + arrowSz, baseY, arrowCol);
      } else if (tideDirection < 0) {
        // Down arrow — shifted 4px lower than before
        int16_t baseY = tideTextStartY + textHeight + gap + upShift;
        int16_t tipY  = baseY + arrowSz;
        gfx->fillTriangle(arrowCX,           tipY,
                          arrowCX - arrowSz, baseY,
                          arrowCX + arrowSz, baseY, arrowCol);
      }
      // Slack (direction == 0): no arrow drawn
    }

    // Current tide in ft — below bar
    gfx->setTextColor(currentTheme.periodDirNumberColor);
    gfx->setTextSize(2);
    gfx->setCursor(tideX - 15, tideBarY + tideBarH + 4);
    gfx->print(String(tideHeightFeet, 1) + "ft");

    // H / L tiny labels above and below bar
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(1);
    gfx->setCursor(tideX + 2, tideBarY - 8);
    gfx->print("H " + String(maxTideFeet, 1) + "ft");
    gfx->setCursor(tideX + 2, tideBarY + tideBarH + 20);
    gfx->print("L " + String(minTideFeet, 1) + "ft");
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
      gfx->setTextColor(currentTheme.textSecondary);
      gfx->setTextSize(2);
      gfx->setCursor(10, 5);
      gfx->println("Stored Files");
      
      // Show scroll indicator if needed
      if (totalLines > maxLines) {
        gfx->setTextSize(1);
        gfx->setCursor(390, 5);
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

void drawWelcomeScreen(Rect &setupButton) {
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();

  gfx->fillScreen(currentTheme.text);

  // "Hi, I'm Surf Board." centered — textSize 3: each char 18px wide
  gfx->setTextColor(currentTheme.background);
  gfx->setTextSize(3);
  const char *line1 = "Hi, I'm Surf Board.";
  gfx->setCursor(screenWidth / 2 - (int16_t)(strlen(line1) * 18) / 2, screenHeight / 2 - 65);
  gfx->println(line1);

  // "What's your name?" centered
  gfx->setTextColor(~currentTheme.accent);
  const char *line2 = "What's your name?";
  gfx->setCursor(screenWidth / 2 - (int16_t)(strlen(line2) * 18) / 2, screenHeight / 2 - 18);
  gfx->println(line2);

  // Setup button centered
  setupButton = {int16_t(screenWidth / 2 - 100), int16_t(screenHeight / 2 + 65), 200, 50};
  drawButton(setupButton, "My name is...", ~currentTheme.success, currentTheme.background, 2);
}

void drawNameConfirmScreen(const String &name, Rect &confirmButton) {
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();

  gfx->fillScreen(currentTheme.text);

  gfx->setTextColor(currentTheme.background);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 72, screenHeight / 2 - 65);
  gfx->println("Your name is:");

  gfx->setTextColor(~currentTheme.accent);
  gfx->setTextSize(4);
  int16_t namePixW = (int16_t)(name.length() * 24);
  if (namePixW > screenWidth - 20) namePixW = screenWidth - 20;
  gfx->setCursor(screenWidth / 2 - namePixW / 2, screenHeight / 2 - 18);
  gfx->println(name);

  confirmButton = {int16_t(screenWidth / 2 - 75), int16_t(screenHeight / 2 + 65), 150, 50};
  drawButton(confirmButton, " Confirm", ~currentTheme.success, currentTheme.background, 2);
}
