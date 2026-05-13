#pragma once
#ifndef XPT2046_TOUCHSCREEN_H
#define XPT2046_TOUCHSCREEN_H

#include "Arduino.h"
#include <emscripten.h>

// Touch calibration constants from Config.h — guard against redefinition when
// Config.h is included before this header.
#ifndef TOUCH_MIN_X
#define TOUCH_MIN_X 240
#define TOUCH_MAX_X 3860
#define TOUCH_MIN_Y 280
#define TOUCH_MAX_Y 3860
#endif

struct TS_Point {
    int16_t x = 0, y = 0, z = 0;
};

class XPT2046_Touchscreen {
public:
    XPT2046_Touchscreen(int, int) {}
    void begin() {}

    // Returns true while the mouse button is held or a latched click is pending.
    bool touched() {
        return EM_ASM_INT({ return (Module.touchLatch || Module.mouseDown) ? 1 : 0; }) != 0;
    }

    // Returns raw calibration-space coordinates that map() will convert back to
    // the actual pixel position on the 480×320 canvas.
    //
    //   mapped_x = map(raw_x, TOUCH_MIN_X, TOUCH_MAX_X, width, 0)   (inverted)
    //   raw_x    = TOUCH_MIN_X + (width  - pixel_x) * (TOUCH_MAX_X - TOUCH_MIN_X) / width
    //   raw_y    = TOUCH_MIN_Y + (height - pixel_y) * (TOUCH_MAX_Y - TOUCH_MIN_Y) / height
    TS_Point getPoint() {
        int px = EM_ASM_INT({ return Module.touchX; });
        int py = EM_ASM_INT({ return Module.touchY; });
        // Clear the one-shot latch now that the point has been consumed.
        EM_ASM({ Module.touchLatch = false; });
        TS_Point p;
        p.x = (int16_t)(TOUCH_MIN_X + (int)(480 - px) * (TOUCH_MAX_X - TOUCH_MIN_X) / 480);
        p.y = (int16_t)(TOUCH_MIN_Y + (int)(320 - py) * (TOUCH_MAX_Y - TOUCH_MIN_Y) / 320);
        p.z = 1;
        return p;
    }
};

#endif // XPT2046_TOUCHSCREEN_H
