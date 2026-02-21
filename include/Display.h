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
void drawForgetButton(Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton);
void drawForecast(const LocationInfo &location, const SurfForecast &forecast, 
                  Rect &forgetButton, Rect &forgetLocationButton, Rect &themeButton, Rect &waveButton,
                  float waveHeightThreshold);

#endif // DISPLAY_H
