#include "Theme.h"
#include <Arduino_GFX_Library.h>

bool darkMode = true;

// Dark theme colors
Theme darkTheme = {
  BLACK,      // background
  WHITE,      // text
  CYAN,       // textSecondary (accent text)
  YELLOW,     // accent (sun - yellow on night mode)
  0x2C40,     // buttonPrimary (dark green)
  0x8000,     // buttonSecondary (dark red)
  0x8000,     // buttonDanger (dark red)
  0xCA00,     // buttonWarning (dark orange)
  0x2104,     // buttonKeys (dark gray)
  0x4A49,     // buttonList (dark blue-gray)
  BLACK,      // border
  0x2C40,     // success (dark green)
  0x7FFF,     // error (inverted - cyan)
  0x4DFF,     // cloudColor (light blue on night mode)
  CYAN,       // periodDirTextColor (cyan on night mode)
  0x65C8      // periodDirNumberColor (lighter green on night mode)
};

// Light theme colors
Theme lightTheme = {
  WHITE,      // background
  BLACK,      // text
  BLUE,       // textSecondary
  0xFE80,     // accent (orange-yellow on day mode)
  GREEN,      // buttonPrimary
  RED,        // buttonSecondary
  RED,        // buttonDanger
  0xFD20,     // buttonWarning (orange)
  0xCE79,     // buttonKeys (light gray)
  0xAD55,     // buttonList (lighter gray)
  BLACK,      // border
  GREEN,      // success
  0x07FF,     // error (inverted - cyan)
  0x0016,     // cloudColor (dark blue on day mode)
  0xCA00,     // periodDirTextColor (orange on day mode)
  0x2400      // periodDirNumberColor (darker green on day mode)
};

Theme currentTheme = darkTheme;

void initThemes() {
  // Themes are already initialized as globals
}

uint16_t invertColor(uint16_t color) {
  return color ^ 0xFFFF;
}

void applyTheme() {
  currentTheme = darkMode ? darkTheme : lightTheme;
  // Invert all theme colors to compensate for display hardware inversion
  currentTheme.background = invertColor(currentTheme.background);
  currentTheme.text = invertColor(currentTheme.text);
  currentTheme.textSecondary = invertColor(currentTheme.textSecondary);
  currentTheme.accent = invertColor(currentTheme.accent);
  currentTheme.buttonPrimary = invertColor(currentTheme.buttonPrimary);
  currentTheme.buttonSecondary = invertColor(currentTheme.buttonSecondary);
  currentTheme.buttonDanger = invertColor(currentTheme.buttonDanger);
  currentTheme.buttonWarning = invertColor(currentTheme.buttonWarning);
  currentTheme.buttonKeys = invertColor(currentTheme.buttonKeys);
  currentTheme.buttonList = invertColor(currentTheme.buttonList);
  currentTheme.border = invertColor(currentTheme.border);
  currentTheme.success = invertColor(currentTheme.success);
  currentTheme.error = invertColor(currentTheme.error);
  currentTheme.cloudColor = invertColor(currentTheme.cloudColor);
  currentTheme.periodDirTextColor = invertColor(currentTheme.periodDirTextColor);
  currentTheme.periodDirNumberColor = invertColor(currentTheme.periodDirNumberColor);
}
