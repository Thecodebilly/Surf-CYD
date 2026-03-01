#include "Game.h"
#include "Config.h"
#include "Theme.h"
#include "TouchUI.h"
#include "Display.h"
#include "Storage.h"
#include "Database.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

// High score storage
unsigned long loadHighScore() {
  if (!SPIFFS.exists("/high_score.json")) return 0;
  
  File file = SPIFFS.open("/high_score.json", "r");
  if (!file) return 0;
  
  DynamicJsonDocument doc(128);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) return 0;
  return doc["highScore"] | 0;
}

bool saveHighScore(unsigned long score) {
  DynamicJsonDocument doc(128);
  doc["highScore"] = score;
  
  File file = SPIFFS.open("/high_score.json", "w");
  if (!file) return false;
  
  serializeJson(doc, file);
  file.close();
  
  logInfo("High score saved: " + String(score));
  return true;
}

void runSurfGame(Rect &exitButton) {
  // Simple endless runner surf game with optimized rendering
  
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();
  
  // Game state
  int16_t surferX = screenWidth / 2;
  int16_t surferY = screenHeight - 40;
  int16_t prevSurferX = surferX;
  const int16_t surferSize = 12;  // Increased for stick figure
  const int16_t surferSpeed = 12;  // Faster movement
  
  // Obstacles (shark fins)
  const int maxObstacles = 36;  // 3x more slots for multi-spawn waves
  int16_t obstacleX[maxObstacles];
  int16_t obstacleY[maxObstacles];
  int16_t prevObstacleX[maxObstacles];
  int16_t prevObstacleY[maxObstacles];
  bool obstacleActive[maxObstacles];
  const int16_t obstacleWidth = 16;  // Width of shark fin base
  const int16_t obstacleHeight = 20; // Height of shark fin
  int16_t obstacleSpeed = 2;  // 2x slower base speed
  
  // Initialize obstacles
  for (int i = 0; i < maxObstacles; i++) {
    obstacleActive[i] = false;
    prevObstacleY[i] = -100;
    prevObstacleX[i] = 0;
  }

  // Coconut collectible — occasional bonus item worth +300 pts
  const int16_t coconutRadius = 9;
  int16_t coconutX = 0, coconutY = 0;
  int16_t prevCoconutX = 0, prevCoconutY = -100;
  bool coconutActive = false;
  unsigned long lastCoconutSpawn = millis();
  const unsigned long coconutInterval = 6000; // spawn every 6 seconds
  // Colors (device inverts): 0x75DD displays as saddle brown, 0xE79F as near-black holes
  const uint16_t coconutColor = 0x75DD;
  const uint16_t coconutHoleColor = 0xE79F;
  unsigned long coconutBonus = 0; // accumulated bonus points from collected coconuts

  // Golden coconut — very rare, worth +500 pts
  int16_t goldenX = 0, goldenY = 0;
  int16_t prevGoldenX = 0, prevGoldenY = -100;
  bool goldenActive = false;
  unsigned long lastGoldenSpawn = millis();
  const unsigned long goldenInterval = 35000; // spawn every 35 seconds
  // 0x001F displays as gold/yellow on inverted display (~0xFFE0)
  const uint16_t goldenColor = 0x001F;
  const uint16_t goldenShineColor = 0x07E0; // displays as magenta ~0xF81F, subtle highlight ring

  unsigned long score = 0;
  unsigned long prevScore = 0;
  unsigned long lastSpawn = millis();
  unsigned long spawnInterval = 1200;
  unsigned long gameStart = millis();
  bool gameOver = false;
  uint16_t frameCount = 0;  // for throttling static UI repaints
  unsigned long highScore = loadHighScore();
  
  // Fetch global high score
  GlobalHighScore globalHS = fetchGlobalHighScore();
  unsigned long globalHighScore = globalHS.valid ? globalHS.score : 0;
  String globalHighScoreName = globalHS.valid ? globalHS.name : "---";
  
  // Exit button setup
  exitButton = {int16_t(screenWidth - 50), 5, 45, 30};
  
  // Game colors based on mode
  // Dark mode bg = opposite of dark blue/purple (0x0814) = 0xF7EB (warm light)
  // Light mode: bgColor displays as darker ocean blue (0x1C9F) → code 0xE360
  //             oceanColor displays as lighter blue   (0xAEBC) → code 0x5143
  uint16_t oceanColor, bgColor;
  if (darkMode) {
    bgColor    = 0xF7EB;  // ~0x0814 (opposite of dark blue/purple)
    oceanColor = 0xFCE0;  // ~0x031F (opposite of a light shade of blue)
  } else {
    bgColor    = 0xE360;  // displays as darker ocean blue (dodger blue ~0x1C9F)
    oceanColor = 0x5143;  // displays as lighter blue (light blue ~0xAEBC) — slightly lighter than bg
  }
  uint16_t initialOceanColor = oceanColor;
  uint16_t prevOceanColor = oceanColor;
  uint16_t initialBgColor = bgColor;
  uint16_t prevBgColor = bgColor;
  // 0x07FF = cyan on RGB565, inverts to red on this display — danger target
  const uint16_t dangerOceanColor = 0x07FF;
  const uint16_t dangerBgColor    = 0x07FF;
  // RGB565 lerp helper
  auto lerpRGB565 = [](uint16_t a, uint16_t b, float t) -> uint16_t {
    uint8_t rA = (a >> 11) & 0x1F, gA = (a >> 5) & 0x3F, bA = a & 0x1F;
    uint8_t rB = (b >> 11) & 0x1F, gB = (b >> 5) & 0x3F, bB = b & 0x1F;
    uint8_t r  = (uint8_t)(rA + (rB - rA) * t);
    uint8_t g  = (uint8_t)(gA + (int8_t)((int)gB - gA) * t);
    uint8_t bl = (uint8_t)(bA + (bB - bA) * t);
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | bl;
  };
  uint16_t textColor = currentTheme.text;
  uint16_t surferColor = ~YELLOW;  // Inverted yellow (becomes blue)
  // Board color: 0x02DF displays as orange (light), 0x77FF displays as dark red (dark)
  uint16_t boardColor = darkMode ? 0x77FF : 0x02DF;
  uint16_t sharkColor = 0x7BEF;  // Opposite of mid-gray (0x8410)
  uint16_t exitBgColor = currentTheme.buttonDanger;
  uint16_t exitTextColor = currentTheme.text;
  
  // Initial full screen draw
  gfx->fillScreen(bgColor);
  
  // Draw static ocean background
  gfx->fillRect(0, surferY - 15, screenWidth, 55, oceanColor);
  
  // Draw exit button (only once) with inverted colors
  gfx->fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, exitBgColor);
  gfx->drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, currentTheme.border);
  gfx->setTextColor(exitTextColor);
  gfx->setTextSize(1);
  gfx->setCursor(exitButton.x + 8, exitButton.y + (exitButton.h / 2) - 4);
  gfx->println("Exit");
  
  // Draw initial score
  gfx->setTextColor(textColor);
  gfx->setTextSize(2);
  gfx->setCursor(10, 10);
  gfx->print("Score: 0");

  // "SHARKS!" banner — opposite of red = cyan (0x07FF)
  gfx->setTextColor(0x07FF);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 42, 36);
  gfx->println("SHARKS!");
  
  // Game loop
  while (!gameOver) {
    // Progressive difficulty — slower individual sharks (base 1, max 5)
    int currentSpeed = 1 + (score / 150);
    if (currentSpeed > 5) currentSpeed = 5;
    obstacleSpeed = currentSpeed;
    
    // Decrease spawn interval rapidly — use signed math to avoid unsigned underflow
    long spawnCalc = 1000L - (long)(score * 4);
    spawnInterval = (unsigned long)(spawnCalc < 80 ? 80 : spawnCalc);
    
    // Danger level based on elapsed time — full red at 10 minutes (600,000 ms)
    float dangerLevel = (float)(millis() - gameStart) / 600000.0f;
    if (dangerLevel > 1.0f) dangerLevel = 1.0f;
    uint16_t currentOceanColor = lerpRGB565(initialOceanColor, dangerOceanColor, dangerLevel);
    uint16_t currentBgColor    = lerpRGB565(initialBgColor,    dangerBgColor,    dangerLevel);
    bool bgChanged    = (currentBgColor    != prevBgColor);
    bool oceanChanged = (currentOceanColor != prevOceanColor);
    if (bgChanged || oceanChanged) {
      // Repaint background zones — ocean band last so it overlays cleanly
      if (bgChanged) {
        // Fill everything above and below the ocean band
        gfx->fillRect(0, 0, screenWidth, surferY - 15, currentBgColor);
        gfx->fillRect(0, surferY + 40, screenWidth, screenHeight - (surferY + 40), currentBgColor);
        prevBgColor = currentBgColor;
      }
      gfx->fillRect(0, surferY - 15, screenWidth, 55, currentOceanColor);
      prevOceanColor = currentOceanColor;
    }
    
    // Spawn one shark per interval — cap at 15 active simultaneously so they never stop coming
    if (millis() - lastSpawn > spawnInterval) {
      int activeCount = 0;
      for (int i = 0; i < maxObstacles; i++) if (obstacleActive[i]) activeCount++;
      if (activeCount < 15) {
        for (int i = 0; i < maxObstacles; i++) {
          if (!obstacleActive[i]) {
            obstacleX[i] = random(obstacleWidth, screenWidth - obstacleWidth);
            obstacleY[i] = -obstacleHeight;
            prevObstacleY[i] = obstacleY[i];
            prevObstacleX[i] = obstacleX[i];
            obstacleActive[i] = true;
            lastSpawn = millis();
            break;
          }
        }
      } else {
        // Still reset timer so we keep checking — prevents sharks from stopping
        lastSpawn = millis();
      }
    }

    // Spawn coconut — one at a time, every ~6 seconds
    if (!coconutActive && (millis() - lastCoconutSpawn > coconutInterval)) {
      coconutX = random(coconutRadius + 5, screenWidth - coconutRadius - 5);
      coconutY = -coconutRadius;
      prevCoconutX = coconutX;
      prevCoconutY = coconutY;
      coconutActive = true;
      lastCoconutSpawn = millis();
    }

    // Update coconut
    if (coconutActive) {
      // Erase previous position
      if (prevCoconutY >= -coconutRadius) {
        int16_t eraseX = prevCoconutX - coconutRadius - 1;
        int16_t eraseTop = prevCoconutY - coconutRadius - 1;
        int16_t eraseW = (coconutRadius * 2) + 3;
        int16_t eraseH = (coconutRadius * 2) + 3;
        int16_t boundary = surferY - 15;
        if (eraseTop < boundary && (eraseTop + eraseH) > boundary) {
          int16_t topH = boundary - eraseTop;
          gfx->fillRect(eraseX, eraseTop, eraseW, topH, currentBgColor);
          gfx->fillRect(eraseX, boundary, eraseW, eraseH - topH, currentOceanColor);
        } else {
          uint16_t ec = (prevCoconutY >= boundary) ? currentOceanColor : currentBgColor;
          gfx->fillRect(eraseX, eraseTop, eraseW, eraseH, ec);
        }
      }
      coconutY += obstacleSpeed;
      if (coconutY > screenHeight + coconutRadius) {
        // Missed — reset spawn timer so next coconut comes after another interval
        coconutActive = false;
        lastCoconutSpawn = millis();
      } else {
        // Draw coconut: brown circle with 3 dark pore holes
        gfx->fillCircle(coconutX, coconutY, coconutRadius, coconutColor);
        gfx->fillCircle(coconutX,     coconutY - 3, 2, coconutHoleColor); // top hole
        gfx->fillCircle(coconutX - 3, coconutY + 2, 2, coconutHoleColor); // bottom-left
        gfx->fillCircle(coconutX + 3, coconutY + 2, 2, coconutHoleColor); // bottom-right

        // Collect coconut — AABB with surfer
        int16_t playerLeft   = surferX - surferSize/2;
        int16_t playerRight  = surferX + surferSize/2;
        int16_t playerTop    = surferY - surferSize - 8;
        int16_t playerBottom = surferY + 4;
        if (playerRight > (coconutX - coconutRadius) && playerLeft < (coconutX + coconutRadius) &&
            playerBottom > (coconutY - coconutRadius) && playerTop < (coconutY + coconutRadius)) {
          coconutBonus += 200;
          // Erase coconut immediately on collect
          int16_t eraseX  = coconutX - coconutRadius - 1;
          int16_t eraseTop = coconutY - coconutRadius - 1;
          int16_t eraseW   = (coconutRadius * 2) + 3;
          int16_t eraseH   = (coconutRadius * 2) + 3;
          int16_t boundary = surferY - 15;
          if (eraseTop < boundary && (eraseTop + eraseH) > boundary) {
            int16_t topH = boundary - eraseTop;
            gfx->fillRect(eraseX, eraseTop, eraseW, topH, currentBgColor);
            gfx->fillRect(eraseX, boundary, eraseW, eraseH - topH, currentOceanColor);
          } else {
            uint16_t ec = (coconutY >= boundary) ? currentOceanColor : currentBgColor;
            gfx->fillRect(eraseX, eraseTop, eraseW, eraseH, ec);
          }
          coconutActive = false;
          lastCoconutSpawn = millis();
        }
      }
      prevCoconutX = coconutX;
      prevCoconutY = coconutY;
    }

    // Spawn golden coconut — very rare, every ~25 seconds
    if (!goldenActive && (millis() - lastGoldenSpawn > goldenInterval)) {
      goldenX = random(coconutRadius + 5, screenWidth - coconutRadius - 5);
      goldenY = -coconutRadius;
      prevGoldenX = goldenX;
      prevGoldenY = goldenY;
      goldenActive = true;
      lastGoldenSpawn = millis();
    }

    // Update golden coconut
    if (goldenActive) {
      if (prevGoldenY >= -coconutRadius) {
        int16_t eraseX   = prevGoldenX - coconutRadius - 1;
        int16_t eraseTop = prevGoldenY - coconutRadius - 1;
        int16_t eraseW   = (coconutRadius * 2) + 3;
        int16_t eraseH   = (coconutRadius * 2) + 3;
        int16_t boundary = surferY - 15;
        if (eraseTop < boundary && (eraseTop + eraseH) > boundary) {
          int16_t topH = boundary - eraseTop;
          gfx->fillRect(eraseX, eraseTop, eraseW, topH, currentBgColor);
          gfx->fillRect(eraseX, boundary, eraseW, eraseH - topH, currentOceanColor);
        } else {
          uint16_t ec = (prevGoldenY >= boundary) ? currentOceanColor : currentBgColor;
          gfx->fillRect(eraseX, eraseTop, eraseW, eraseH, ec);
        }
      }
      goldenY += obstacleSpeed;
      if (goldenY > screenHeight + coconutRadius) {
        goldenActive = false;
        lastGoldenSpawn = millis();
      } else {
        // Draw golden coconut: gold circle, subtle outline ring, 3 dark pore holes
        gfx->fillCircle(goldenX, goldenY, coconutRadius, goldenColor);
        gfx->drawCircle(goldenX, goldenY, coconutRadius, goldenShineColor);
        gfx->fillCircle(goldenX,     goldenY - 3, 2, coconutHoleColor);
        gfx->fillCircle(goldenX - 3, goldenY + 2, 2, coconutHoleColor);
        gfx->fillCircle(goldenX + 3, goldenY + 2, 2, coconutHoleColor);

        // Collect
        int16_t playerLeft   = surferX - surferSize/2;
        int16_t playerRight  = surferX + surferSize/2;
        int16_t playerTop    = surferY - surferSize - 8;
        int16_t playerBottom = surferY + 4;
        if (playerRight > (goldenX - coconutRadius) && playerLeft < (goldenX + coconutRadius) &&
            playerBottom > (goldenY - coconutRadius) && playerTop < (goldenY + coconutRadius)) {
          coconutBonus += 500;
          int16_t eraseX   = goldenX - coconutRadius - 1;
          int16_t eraseTop = goldenY - coconutRadius - 1;
          int16_t eraseW   = (coconutRadius * 2) + 3;
          int16_t eraseH   = (coconutRadius * 2) + 3;
          int16_t boundary = surferY - 15;
          if (eraseTop < boundary && (eraseTop + eraseH) > boundary) {
            int16_t topH = boundary - eraseTop;
            gfx->fillRect(eraseX, eraseTop, eraseW, topH, currentBgColor);
            gfx->fillRect(eraseX, boundary, eraseW, eraseH - topH, currentOceanColor);
          } else {
            uint16_t ec = (goldenY >= boundary) ? currentOceanColor : currentBgColor;
            gfx->fillRect(eraseX, eraseTop, eraseW, eraseH, ec);
          }
          goldenActive = false;
          lastGoldenSpawn = millis();
        }
      }
      prevGoldenX = goldenX;
      prevGoldenY = goldenY;
    }

    // Erase previous stick figure + board only when position changed, to avoid flicker
    if (prevSurferX != surferX) {
      // Top: surferY-23 (head top), Bottom: surferY+18 (board tail circle bottom) — height=41
      gfx->fillRect(prevSurferX - surferSize, surferY - surferSize - 11, surferSize * 2, surferSize + 29, currentOceanColor);
    }
    
    // Update and draw shark fins
    for (int i = 0; i < maxObstacles; i++) {
      if (obstacleActive[i]) {
        // Erase previous position — widen by 1px on all sides to prevent dot trails
        if (prevObstacleY[i] >= -obstacleHeight) {
          int16_t eraseTop = prevObstacleY[i] - obstacleHeight - 1;
          int16_t eraseX   = prevObstacleX[i] - obstacleWidth/2 - 1;
          int16_t eraseW   = obstacleWidth + 2;
          int16_t eraseH   = obstacleHeight + 4;
          int16_t boundary = surferY - 15;
          if (eraseTop < boundary && (eraseTop + eraseH) > boundary) {
            // Fin straddles the bg/ocean boundary — erase each zone separately
            int16_t topH = boundary - eraseTop;
            gfx->fillRect(eraseX, eraseTop, eraseW, topH, currentBgColor);
            gfx->fillRect(eraseX, boundary, eraseW, eraseH - topH, currentOceanColor);
          } else {
            uint16_t eraseColor = ((prevObstacleY[i] - obstacleHeight) >= boundary) ? currentOceanColor : currentBgColor;
            gfx->fillRect(eraseX, eraseTop, eraseW, eraseH, eraseColor);
          }
        }
        
        // Update position
        obstacleY[i] += obstacleSpeed;
        
        // Check if off screen
        if (obstacleY[i] > screenHeight) {
          obstacleActive[i] = false;
          score += 10;
        } else {
          // Draw shark fin (triangle)
          int16_t finTop = obstacleY[i] - obstacleHeight;
          int16_t finBottom = obstacleY[i];
          int16_t finLeft = obstacleX[i] - obstacleWidth/2;
          int16_t finRight = obstacleX[i] + obstacleWidth/2;
          int16_t finTip = obstacleX[i];
          
          // Draw filled triangle for shark fin
          gfx->fillTriangle(finTip, finTop, finLeft, finBottom, finRight, finBottom, sharkColor);
          
          // Collision detection with shark fin (triangle approximation)
          // Check if stick figure overlaps with shark fin area
          int16_t playerLeft = surferX - surferSize/2;
          int16_t playerRight = surferX + surferSize/2;
          int16_t playerTop = surferY - surferSize - 8;
          int16_t playerBottom = surferY + 4;
          
          // Simple AABB collision with fin area
          if (playerRight > finLeft && playerLeft < finRight &&
              playerBottom > finTop && playerTop < finBottom) {
            gameOver = true;
          }
        }
        
        prevObstacleY[i] = obstacleY[i];
        prevObstacleX[i] = obstacleX[i];
      }
    }
    
    // Draw stick figure at new position
    // Surfboard under feet (no fins) — drawn first so figure sits on top
    {
      const int16_t bHW = 7;    // half-width of board
      const int16_t bBodyH = 14; // length of straight section
      int16_t bTopY = surferY - 11; // top-cap center
      gfx->fillCircle(surferX, bTopY,          bHW, boardColor); // nose
      gfx->fillRect(surferX - bHW, bTopY,      bHW * 2 + 1, bBodyH, boardColor); // body
      gfx->fillCircle(surferX, bTopY + bBodyH, bHW, boardColor); // tail
      gfx->drawLine(surferX, bTopY - bHW, surferX, bTopY + bBodyH + bHW, 0x7BEF); // stringer — 0x7BEF displays as mid-gray on inverted screen
    }
    // Head
    gfx->fillCircle(surferX, surferY - surferSize, 3, surferColor);
    // Body (vertical line)
    gfx->drawFastVLine(surferX, surferY - surferSize + 3, 8, surferColor);
    // Arms (horizontal line)
    gfx->drawFastHLine(surferX - 4, surferY - surferSize + 5, 9, surferColor);
    // Left leg
    gfx->drawLine(surferX, surferY - surferSize + 11, surferX - 3, surferY + 3, surferColor);
    // Right leg
    gfx->drawLine(surferX, surferY - surferSize + 11, surferX + 3, surferY + 3, surferColor);
    
    prevSurferX = surferX;
    
    // Update score based on time + accumulated coconut bonuses
    score = (millis() - gameStart) / 100 + coconutBonus;
    
    // Only redraw score if it changed
    if (score != prevScore) {
      // Render score with per-character background fill — no fillRect needed, eliminates flash
      gfx->setTextColor(textColor, currentBgColor);
      gfx->setTextSize(2);
      gfx->setCursor(10, 10);
      gfx->print("Score: ");
      gfx->print(score);
      gfx->print("      "); // trailing spaces wipe old digits if score got shorter
      gfx->setTextColor(textColor); // reset to no-bg for other draws
      prevScore = score;
    }
    
    // Redraw exit button and banner every 20 frames — reduces flicker, still recovers quickly if a shark passes through
    frameCount++;
    if (frameCount >= 20) {
      frameCount = 0;
      gfx->fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, exitBgColor);
      gfx->drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, currentTheme.border);
      gfx->setTextColor(exitTextColor);
      gfx->setTextSize(1);
      gfx->setCursor(exitButton.x + 8, exitButton.y + (exitButton.h / 2) - 4);
      gfx->println("Exit");
      gfx->setTextColor(0x07FF);
      gfx->setTextSize(2);
      gfx->setCursor(screenWidth / 2 - 42, 36);
      gfx->println("SHARKS!");
    }
    
    // Handle input
    if (touch.touched()) {
      TS_Point raw = touch.getPoint();
      int16_t touchX = map(raw.x, TOUCH_MIN_X, TOUCH_MAX_X, screenWidth, 0);
      int16_t touchY = map(raw.y, TOUCH_MIN_Y, TOUCH_MAX_Y, screenHeight, 0);
      
      // Check exit button
      if (pointInRect(touchX, touchY, exitButton)) {
        return; // Exit game
      }
      
      // Move surfer based on touch side
      if (touchX < screenWidth / 2) {
        surferX -= surferSpeed;
      } else {
        surferX += surferSpeed;
      }
      
      // Keep surfer on screen
      surferX = constrain(surferX, surferSize, screenWidth - surferSize);

      delay(5);  // Touch responsiveness
    }

    delay(10); // ~100 FPS cap — prevents ESP32 from busy-looping flat out
  }
  
  // Check and update high score
  bool newLocalHighScore = false;
  bool newGlobalHighScore = false;
  
  if (score > highScore) {
    highScore = score;
    saveHighScore(highScore);
    newLocalHighScore = true;
  }
  
  // Check if new global high score
  if (score > globalHighScore && score > 0) {
    newGlobalHighScore = true;
  }
  
  // Game over screen
  gfx->fillScreen(bgColor);
  gfx->setTextColor(currentTheme.error);
  gfx->setTextSize(4);
  gfx->setCursor(screenWidth / 2 - 100, screenHeight / 2 - 60);
  gfx->println("Game Over!");
  
  gfx->setTextColor(textColor);
  gfx->setTextSize(3);
  gfx->setCursor(screenWidth / 2 - 80, screenHeight / 2 - 10);
  gfx->print("Score: ");
  gfx->println(score);
  
  // Show high score
  gfx->setTextColor(currentTheme.accent);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 80, screenHeight / 2 + 25);
  gfx->print("Local Best: ");
  gfx->println(highScore);
  
  // Show global high score
  if (globalHighScore > 0) {
    gfx->setTextColor(currentTheme.buttonPrimary);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 95, screenHeight / 2 + 50);
    gfx->print("World: ");
    gfx->print(globalHighScore);
    gfx->print(" (");
    gfx->print(globalHighScoreName);
    gfx->println(")");
  }
  
  if (newLocalHighScore && score > 0) {
    gfx->setTextColor(currentTheme.success);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 80, screenHeight / 2 + 75);
    gfx->println("NEW LOCAL RECORD!");
  }
  
  if (newGlobalHighScore) {
    gfx->setTextColor(0xF800);  // Red
    gfx->setTextSize(3);
    gfx->setCursor(screenWidth / 2 - 110, screenHeight / 2 + 105);
    gfx->println("WORLD RECORD!");
  }
  
  gfx->setTextColor(currentTheme.textSecondary);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 90, screenHeight / 2 + 140);
  gfx->println("Touch to continue");
  
  // Mandatory 1.5-second wait before accepting touch
  delay(1500);
  while (touch.touched()) delay(20); // Drain any touches during wait

  // Wait for touch to exit
  while (!touch.touched()) {
    delay(50);
  }
  while (touch.touched()) delay(20); // Debounce
  
  // Submit every score to the leaderboard
  if (score > 0) {
    // Load the player name saved during setup
    String playerName = loadPlayerName();
    if (playerName.isEmpty()) playerName = "Player";

    gfx->fillScreen(currentTheme.background);
    gfx->setTextColor(currentTheme.text);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 100, screenHeight / 2 - 10);
    gfx->println("Submitting score...");

    String submitError = submitRecord(playerName, score);

    gfx->fillScreen(currentTheme.background);
    if (submitError.isEmpty()) {
      gfx->setTextColor(currentTheme.success);
      gfx->setTextSize(2);
      gfx->setCursor(screenWidth / 2 - 90, screenHeight / 2 - 20);
      gfx->println("Score submitted!");
      gfx->setTextColor(currentTheme.text);
      gfx->setCursor(screenWidth / 2 - 40, screenHeight / 2 + 10);
      gfx->print(playerName);
      gfx->setCursor(screenWidth / 2 - 40, screenHeight / 2 + 35);
      gfx->print(String(score));
    } else {
      gfx->setTextColor(currentTheme.error);
      gfx->setTextSize(2);
      gfx->setCursor(screenWidth / 2 - 85, screenHeight / 2 - 30);
      gfx->println("Submission failed:");
      gfx->setTextColor(currentTheme.textSecondary);
      gfx->setTextSize(1);
      int16_t errY = screenHeight / 2;
      String err = submitError;
      while (err.length() > 0) {
        int cut = min((int)err.length(), 42);
        gfx->setCursor(10, errY);
        gfx->println(err.substring(0, cut));
        err = err.substring(cut);
        errY += 12;
      }
    }
    
    gfx->setTextColor(currentTheme.textSecondary);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 90, screenHeight / 2 + 80);
    gfx->println("Touch to continue");
    
    // Wait for touch
    while (!touch.touched()) {
      delay(50);
    }
    while (touch.touched()) delay(20); // Debounce
  }
  
  // Show leaderboard after game over
  showLeaderboard();
}

void showLeaderboard() {
  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();
  
  // Use theme colors directly — display hardware handles inversion for both modes
  uint16_t bgColor        = currentTheme.background;
  uint16_t textColor      = currentTheme.text;
  uint16_t accentColor    = currentTheme.accent;
  uint16_t secondaryColor = currentTheme.textSecondary;
  
  // Clear screen with inverted background
  gfx->fillScreen(bgColor);
  
  // Show loading message
  gfx->setTextColor(textColor);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 100, screenHeight / 2 - 10);
  gfx->println("Loading leaderboard...");
  
  // Fetch leaderboard
  Leaderboard leaderboard = fetchLeaderboard();
  
  // Clear screen again
  gfx->fillScreen(bgColor);
  
  // Title
  gfx->setTextColor(accentColor);
  gfx->setTextSize(3);
  gfx->setCursor(screenWidth / 2 - 110, 15);
  gfx->println("WORLD RECORDS");
  
  if (leaderboard.valid && leaderboard.count > 0) {
    // Draw leaderboard entries — textSize 2, lineHeight 22 fits all 10 in 320px height
    int16_t startY = 48;
    int16_t lineHeight = 22;
    
    for (int i = 0; i < leaderboard.count; i++) {
      int16_t y = startY + (i * lineHeight);
      
      // Highlight top 3
      uint16_t entryColor = textColor;
      if (i == 0) entryColor = 0x001F;      // Gold: ~0xFFE0, appears yellow/gold on inverted display
      else if (i == 1) entryColor = 0x39E7; // Silver: ~0xC618, appears silver/gray on inverted display
      else if (i == 2) entryColor = 0x039F; // Bronze: ~0xFC60, appears orange/bronze on inverted display
      
      gfx->setTextColor(entryColor);
      gfx->setTextSize(2);
      
      // Rank
      gfx->setCursor(10, y);
      gfx->print(leaderboard.entries[i].rank);
      gfx->print(".");
      
      // Name (truncate if too long)
      String displayName = leaderboard.entries[i].name;
      if (displayName.length() > 12) {
        displayName = displayName.substring(0, 12);
      }
      gfx->setCursor(45, y);
      gfx->print(displayName);
      
      // Score (right aligned)
      String scoreStr = String(leaderboard.entries[i].score);
      int16_t scoreX = screenWidth - 15 - (scoreStr.length() * 12);
      gfx->setCursor(scoreX, y);
      gfx->print(scoreStr);
    }
  } else {
    // No data available
    gfx->setTextColor(secondaryColor);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 95, screenHeight / 2 - 20);
    gfx->println("No records yet!");
    gfx->setCursor(screenWidth / 2 - 85, screenHeight / 2 + 10);
    gfx->println("Be the first!");
  }
  
  // Instructions
  gfx->setTextColor(secondaryColor);
  gfx->setTextSize(2);
  gfx->setCursor(screenWidth / 2 - 90, screenHeight - 25);
  gfx->println("Touch to continue");
  
  // Wait for touch to exit
  while (!touch.touched()) {
    delay(50);
  }
  while (touch.touched()) delay(20); // Debounce
}
