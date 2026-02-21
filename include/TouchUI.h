#ifndef TOUCHUI_H
#define TOUCHUI_H

#include "Types.h"
#include <XPT2046_Touchscreen.h>
#include <vector>

// Touch hardware
extern XPT2046_Touchscreen touch;

// Touch initialization
void setupTouch();

// Touch utilities
bool pointInRect(int16_t x, int16_t y, const Rect &r);
TouchPoint getTouchPoint();

// UI interaction functions
String touchKeyboardInput(const String &title, const String &initial, bool secret);
WifiCredentials runWifiSetupTouch();
String runLocationSetupTouch(LocationInfo &cachedLocation);
float runWaveHeightSetupTouch();
int selectLocationFromList(const std::vector<LocationInfo> &locations);
int selectDefaultLocation();

// Main screen touch handling
int handleMainScreenTouch(const Rect &forgetButton, const Rect &forgetLocationButton, 
                          const Rect &themeButton, const Rect &waveButton,
                          String &surfLocation, LocationInfo &cachedLocation, 
                          float &waveHeightThreshold);

#endif // TOUCHUI_H
