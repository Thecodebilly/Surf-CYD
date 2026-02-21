#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

struct LocationInfo {
  float latitude = 0.0f;
  float longitude = 0.0f;
  String displayName = "";
  bool valid = false;
};

struct SurfForecast {
  float waveHeight = 0.0f;
  float wavePeriod = 0.0f;
  float waveDirection = 0.0f;
  String timeLabel = "";
  bool valid = false;
};

struct WifiCredentials {
  String ssid;
  String password;
  bool valid = false;
};

struct TouchPoint {
  int16_t x = -1;
  int16_t y = -1;
  bool pressed = false;
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct Theme {
  uint16_t background;
  uint16_t text;
  uint16_t textSecondary;
  uint16_t accent;
  uint16_t buttonPrimary;
  uint16_t buttonSecondary;
  uint16_t buttonDanger;
  uint16_t buttonWarning;
  uint16_t buttonKeys;
  uint16_t buttonList;
  uint16_t border;
  uint16_t success;
  uint16_t error;
  uint16_t cloudColor;
  uint16_t periodDirTextColor;
  uint16_t periodDirNumberColor;
};

#endif // TYPES_H
