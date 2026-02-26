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
  
  unsigned long score = 0;
  unsigned long prevScore = 0;
  unsigned long lastSpawn = millis();
  unsigned long spawnInterval = 1200;
  unsigned long gameStart = millis();
  bool gameOver = false;
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

  // "AVOID THE SHARKS!" banner — opposite of red = cyan (0x07FF)
  gfx->setTextColor(0x07FF);
  gfx->setTextSize(2);
  // 18 chars * 12px (size 2 char width) = 216px
  gfx->setCursor(screenWidth / 2 - 108, 36);
  gfx->println("AVOID THE SHARKS!");
  
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
    
    // Erase previous stick figure position
    if (prevSurferX != surferX) {
      // Erase previous stick figure with ocean color (rectangular area)
      gfx->fillRect(prevSurferX - surferSize, surferY - surferSize - 8, surferSize * 2, surferSize + 12, currentOceanColor);
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
    
    // Update score based on time
    score = (millis() - gameStart) / 100;
    
    // Only redraw score if it changed
    if (score != prevScore) {
      // Erase old score area
      gfx->fillRect(10, 10, 150, 20, bgColor);
      // Draw new score
      gfx->setTextColor(textColor);
      gfx->setTextSize(2);
      gfx->setCursor(10, 10);
      gfx->print("Score: ");
      gfx->println(score);
      prevScore = score;
    }
    
    // Redraw exit button and banner every frame so sharks can't permanently erase them
    gfx->fillRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, exitBgColor);
    gfx->drawRoundRect(exitButton.x, exitButton.y, exitButton.w, exitButton.h, 6, currentTheme.border);
    gfx->setTextColor(exitTextColor);
    gfx->setTextSize(1);
    gfx->setCursor(exitButton.x + 8, exitButton.y + (exitButton.h / 2) - 4);
    gfx->println("Exit");
    gfx->setTextColor(0x07FF);
    gfx->setTextSize(2);
    gfx->setCursor(screenWidth / 2 - 108, 36);
    gfx->println("AVOID THE SHARKS!");
    
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
      
      delay(5);  // Reduced from 10 for even more responsive controls
    }
    
    delay(10); // ~60 FPS - reduced from 20ms for smoother gameplay
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
  
  // If new global high score, submit with stored player name
  if (newGlobalHighScore) {
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
  
  // Swap schema by mode: dark mode uses theme as-is, light mode uses inverted
  uint16_t bgColor, textColor, accentColor, secondaryColor;
  if (darkMode) {
    bgColor        = currentTheme.background;
    textColor      = currentTheme.text;
    accentColor    = currentTheme.accent;
    secondaryColor = currentTheme.textSecondary;
  } else {
    bgColor        = ~currentTheme.background;
    textColor      = ~currentTheme.text;
    accentColor    = ~currentTheme.accent;
    secondaryColor = ~currentTheme.textSecondary;
  }
  
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
    // Draw leaderboard entries
    int16_t startY = 55;
    int16_t lineHeight = 28;
    
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
