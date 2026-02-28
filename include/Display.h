#ifndef DISPLAY_H
#define DISPLAY_H

#include "Types.h"
#include <Arduino_GFX_Library.h>

// Display hardware
extern Arduino_DataBus *bus;
extern Arduino_GFX *gfx;

// Display initialization
void setupDisplay();

// Drawing functions
void drawButton(const Rect &r, const String &label, uint16_t bg, uint16_t fg, uint8_t textSize);
void showStatus(const String &line1, const String &line2, uint16_t color);
void drawGoodSurfGraphic(int16_t x, int16_t y, uint16_t color);
void drawBadSurfGraphic(int16_t x, int16_t y, uint16_t color);
void drawSettingsButton(Rect &settingsButton);
void drawSettingsScreen(Rect &backButton, Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton, Rect &filesButton, Rect &leaderboardButton);
void drawForgetButton(Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton, Rect &tideButton);
void drawForecast(const LocationInfo &location, const SurfForecast &forecast, 
                  Rect &settingsButton, Rect &badSurfGraphicRect,
                  float waveHeightThreshold, float minTide, float maxTide, int tideDirection, bool hasTideFile);
void viewFilesScreen(Rect &backButton);
void drawWelcomeScreen(Rect &setupButton);
void drawNameConfirmScreen(const String &name, Rect &confirmButton);

#endif // DISPLAY_H
