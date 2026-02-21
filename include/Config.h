#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Default surf locations
extern const char *DEFAULT_LOCATION_1_NAME;
extern const float DEFAULT_LOCATION_1_LAT;
extern const float DEFAULT_LOCATION_1_LON;
extern const char *DEFAULT_LOCATION_2_NAME;
extern const float DEFAULT_LOCATION_2_LAT;
extern const float DEFAULT_LOCATION_2_LON;

// File paths for persistent storage
extern const char *WIFI_FILE;
extern const char *LOCATION_FILE;
extern const char *THEME_FILE;
extern const char *WAVE_PREF_FILE;

// TFT Display pins
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 27
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14

// Touch screen pins
#define TOUCH_CS 33
#define TOUCH_IRQ 36

// Touch calibration values for CYD displays (may need minor tuning per panel)
#define TOUCH_MIN_X 240
#define TOUCH_MAX_X 3860
#define TOUCH_MIN_Y 280
#define TOUCH_MAX_Y 3860

// Timing
static const uint32_t REFRESH_INTERVAL_MS = 900000; // 15 minutes

// API URLs
static const char *GEOCODE_URL = "https://geocoding-api.open-meteo.com/v1/search";
static const char *MARINE_URL = "https://marine-api.open-meteo.com/v1/marine";

#endif // CONFIG_H
