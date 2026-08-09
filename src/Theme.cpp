#include "Theme.h"
#include <Arduino_GFX_Library.h>

bool darkMode = true;

// Values below are the intended on-screen colors; applyTheme() inverts them to
// compensate for the panel's hardware inversion.

// Dark theme — deep ocean night: navy-ink surface, warm off-white text,
// sun-gold accent, muted sky-blue secondary text.
Theme darkTheme = {
  0x0884,     // background     #0A1220 navy ink
  0xF7BE,     // text           #F2F5F7 off-white
  0x7DDB,     // textSecondary  #7FB8D9 muted sky blue
  0xFE27,     // accent         #FFC53D sun gold
  0x13EA,     // buttonPrimary  #157F52 sea green
  0x3A8D,     // buttonSecondary#3B506B slate
  0xB1C5,     // buttonDanger   #B03A2E brick red
  0xA302,     // buttonWarning  #A66312 dark amber
  0x2168,     // buttonKeys     #232F40 dark slate key
  0x2A0C,     // buttonList     #2E4360 blue slate
  0x4B30,     // border         #4E6480 steel hairline
  0x2E6E,     // success        #2ECC71 status green
  0xFB4B,     // error          #FF6B5E coral red
  0x3CDC,     // cloudColor     #3D9BE0 tide-water blue
  0x7DDB,     // periodDirTextColor   = textSecondary (labels)
  0xF7BE,     // periodDirNumberColor = text (numbers wear ink)
  0x1411      // tideButtonColor #16808C teal
};

// Light theme — beach day: paper surface, ink text, ocean-blue secondary,
// deep-orange accent, soft pastel button fills that carry dark ink labels.
Theme lightTheme = {
  0xF7DF,     // background     #F7F9FB paper
  0x1926,     // text           #1C2733 ink
  0x1311,     // textSecondary  #14608F ocean blue
  0xE3C2,     // accent         #E67817 deep orange
  0x8EB6,     // buttonPrimary  #8FD6B0 pastel sea green
  0xC69B,     // buttonSecondary#C3D0DE pastel slate
  0xF513,     // buttonDanger   #F1A29B pastel coral
  0xF60D,     // buttonWarning  #F5C26B pastel amber
  0xEF7E,     // buttonKeys     #E8EDF4 key gray
  0xD71D,     // buttonList     #D5E1EF pastel blue
  0x9537,     // border         #93A5B8 steel hairline
  0x1C69,     // success        #1E8E4F status green
  0xC1E7,     // error          #C43E38 signal red
  0x4D1B,     // cloudColor     #4FA3D9 tide-water blue
  0x1311,     // periodDirTextColor   = textSecondary (labels)
  0x1926,     // periodDirNumberColor = text (numbers wear ink)
  0x0BD0      // tideButtonColor #0F7B87 teal
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
  currentTheme.tideButtonColor = invertColor(currentTheme.tideButtonColor);
}
