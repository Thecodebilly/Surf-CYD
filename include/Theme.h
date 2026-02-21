#ifndef THEME_H
#define THEME_H

#include <Arduino.h>
#include "Types.h"

// Theme management
extern Theme currentTheme;
extern Theme darkTheme;
extern Theme lightTheme;
extern bool darkMode;

void initThemes();
void applyTheme();
uint16_t invertColor(uint16_t color);

#endif // THEME_H
