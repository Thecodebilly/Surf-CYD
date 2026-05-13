#pragma once
#ifndef ARDUINO_GFX_LIBRARY_H
#define ARDUINO_GFX_LIBRARY_H

#include "Arduino.h"
#include <emscripten.h>
#include <cstdarg>

// ── RGB565 color constants ────────────────────────────────────────────────────
// These match the values used by the GFX library on Arduino.
// The physical ST7796 display has INVON active (IPS=true), so all colours are
// bitwise-inverted at the hardware level.  The JavaScript bridge applies the
// same XOR 0xFFFF inversion so the browser canvas shows exactly what the
// physical display would show.
#define BLACK       0x0000u
#define NAVY        0x000Fu
#define DARKGREEN   0x03E0u
#define DARKCYAN    0x03EFu
#define MAROON      0x7800u
#define PURPLE      0x780Fu
#define OLIVE       0x7BE0u
#define LIGHTGREY   0xC618u
#define DARKGREY    0x7BEFu
#define BLUE        0x001Fu
#define GREEN       0x07E0u
#define CYAN        0x07FFu
#define RED         0xF800u
#define MAGENTA     0xF81Fu
#define YELLOW      0xFFE0u
#define WHITE       0xFFFFu
#define ORANGE      0xFD20u
#define GREENYELLOW 0xAFE5u
#define PINK        0xF81Fu

// ── Bus / panel stubs  (constructors ignored in emulator) ────────────────────
class Arduino_DataBus {
public:
    virtual ~Arduino_DataBus() {}
};

class Arduino_ESP32SPI : public Arduino_DataBus {
public:
    Arduino_ESP32SPI(int, int, int, int, int) {}
};

// ── Arduino_GFX ───────────────────────────────────────────────────────────────
// All draw calls go through EM_ASM blocks that delegate to the Canvas 2D
// context held in Module.ctx.  The Module.rgb565(c) helper (defined in
// canvas_bridge.js) converts RGB565 + applies the hardware inversion.
class Arduino_GFX {
protected:
    int16_t  _width  = 480;
    int16_t  _height = 320;
    int16_t  _curX   = 0;
    int16_t  _curY   = 0;
    uint16_t _color  = WHITE;
    uint8_t  _tsize  = 1;

public:
    Arduino_GFX(int16_t w, int16_t h) : _width(w), _height(h) {}
    virtual ~Arduino_GFX() {}

    virtual void begin()            {}
    virtual void setRotation(uint8_t) {}

    int16_t width()  const { return _width; }
    int16_t height() const { return _height; }

    // ── Primitives ────────────────────────────────────────────────────────────

    void fillScreen(uint16_t c) {
        EM_ASM({ Module.ctx.fillStyle = Module.rgb565($0);
                 Module.ctx.fillRect(0, 0, 480, 320); }, c);
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
        EM_ASM({ Module.ctx.fillStyle = Module.rgb565($4);
                 Module.ctx.fillRect($0, $1, $2, $3); }, x, y, w, h, c);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
        EM_ASM({ Module.ctx.strokeStyle = Module.rgb565($4);
                 Module.ctx.lineWidth   = 1;
                 Module.ctx.strokeRect($0 + 0.5, $1 + 0.5, $2, $3); }, x, y, w, h, c);
    }
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
        EM_ASM({
            var x=($0|0); var y=($1|0); var w=($2|0); var h=($3|0); var r=($4|0);
            var ctx = Module.ctx;
            ctx.fillStyle = Module.rgb565($5);
            ctx.beginPath();
            ctx.moveTo(x + r, y);
            ctx.lineTo(x + w - r, y);
            ctx.quadraticCurveTo(x + w, y, x + w, y + r);
            ctx.lineTo(x + w, y + h - r);
            ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
            ctx.lineTo(x + r, y + h);
            ctx.quadraticCurveTo(x, y + h, x, y + h - r);
            ctx.lineTo(x, y + r);
            ctx.quadraticCurveTo(x, y, x + r, y);
            ctx.closePath();
            ctx.fill();
        }, x, y, w, h, r, c);
    }
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
        EM_ASM({
            var x=($0|0); var y=($1|0); var w=($2|0); var h=($3|0); var r=($4|0);
            var ctx = Module.ctx;
            ctx.strokeStyle = Module.rgb565($5);
            ctx.lineWidth   = 1;
            ctx.beginPath();
            ctx.moveTo(x + r, y);
            ctx.lineTo(x + w - r, y);
            ctx.quadraticCurveTo(x + w, y, x + w, y + r);
            ctx.lineTo(x + w, y + h - r);
            ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
            ctx.lineTo(x + r, y + h);
            ctx.quadraticCurveTo(x, y + h, x, y + h - r);
            ctx.lineTo(x, y + r);
            ctx.quadraticCurveTo(x, y, x + r, y);
            ctx.closePath();
            ctx.stroke();
        }, x, y, w, h, r, c);
    }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t c) {
        EM_ASM({ Module.ctx.fillStyle = Module.rgb565($3);
                 Module.ctx.beginPath();
                 Module.ctx.arc($0, $1, $2, 0, 2 * Math.PI);
                 Module.ctx.fill(); }, x, y, r, c);
    }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t c) {
        EM_ASM({ Module.ctx.strokeStyle = Module.rgb565($3);
                 Module.ctx.lineWidth   = 1;
                 Module.ctx.beginPath();
                 Module.ctx.arc($0, $1, $2, 0, 2 * Math.PI);
                 Module.ctx.stroke(); }, x, y, r, c);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) {
        drawLine(x, y, x, y + h - 1, c);
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
        drawLine(x, y, x + w - 1, y, c);
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
        EM_ASM({ var ctx = Module.ctx;
                 ctx.strokeStyle = Module.rgb565($4);
                 ctx.lineWidth   = 1;
                 ctx.beginPath();
                 ctx.moveTo($0 + 0.5, $1 + 0.5);
                 ctx.lineTo($2 + 0.5, $3 + 0.5);
                 ctx.stroke(); }, x0, y0, x1, y1, c);
    }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t c) {
        EM_ASM({ var ctx = Module.ctx;
                 ctx.fillStyle = Module.rgb565($6);
                 ctx.beginPath();
                 ctx.moveTo($0, $1); ctx.lineTo($2, $3); ctx.lineTo($4, $5);
                 ctx.closePath(); ctx.fill(); }, x0, y0, x1, y1, x2, y2, c);
    }
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t c) {
        EM_ASM({ var ctx = Module.ctx;
                 ctx.strokeStyle = Module.rgb565($6);
                 ctx.lineWidth   = 1;
                 ctx.beginPath();
                 ctx.moveTo($0, $1); ctx.lineTo($2, $3); ctx.lineTo($4, $5);
                 ctx.closePath(); ctx.stroke(); }, x0, y0, x1, y1, x2, y2, c);
    }

    // ── Text ──────────────────────────────────────────────────────────────────
    void setCursor(int16_t x, int16_t y)          { _curX = x; _curY = y; }
    void setTextColor(uint16_t fg)                 { _color = fg; }
    void setTextColor(uint16_t fg, uint16_t)       { _color = fg; }
    void setTextSize(uint8_t s)                    { _tsize = s ? s : 1; }

    void print(const char* s) {
        if (!s || !*s) return;
        // Pass colour explicitly so shape draws can't clobber it.
        EM_ASM({ Module.gfxText($0, $1, $2, UTF8ToString($3), $4); },
               _curX, _curY, (int)_tsize, s, (int)_color);
        _curX += (int16_t)(strlen(s) * 6 * _tsize);
    }
    void print(const String& s)  { print(s.c_str()); }
    void print(char c)           { char buf[2]={c,0}; print(buf); }
    void print(int v)            { print(String(v)); }
    void print(unsigned int v)   { print(String((unsigned long)v)); }
    void print(long v)           { print(String(v)); }
    void print(unsigned long v)  { print(String(v)); }
    void print(float v, int d=2) { print(String(v,d)); }

    // println advances cursor to next line
    void println(const char* s) {
        print(s);
        _curX  = 0;
        _curY += (int16_t)(8 * _tsize + 2);
    }
    void println(const String& s)  { println(s.c_str()); }
    void println(char c)           { char buf[2]={c,0}; println(buf); }
    void println(int v)            { println(String(v)); }
    void println(unsigned int v)   { println(String((unsigned long)v)); }
    void println(long v)           { println(String(v)); }
    void println(unsigned long v)  { println(String(v)); }
    void println(float v, int d=2) { println(String(v,d)); }
    void println()                 { _curX=0; _curY+=(int16_t)(8*_tsize+2); }

    void printf(const char* fmt, ...) {
        char buf[256];
        va_list args; va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        print(buf);
    }

    // ── Colour utility (used in Display.cpp) ─────────────────────────────────
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
};

// ── Arduino_ST7796 ────────────────────────────────────────────────────────────
// ST7796 is 320×480; with rotation=1 (landscape) width=480, height=320.
class Arduino_ST7796 : public Arduino_GFX {
public:
    Arduino_ST7796(Arduino_DataBus*, int, uint8_t, bool,
                   int16_t w = 320, int16_t h = 480,
                   int = 0, int = 0, int = 0, int = 0)
        : Arduino_GFX(h, w) {}    // swap w/h for landscape orientation

    void begin() override {
        // Canvas is initialised in JS; nothing to do here.
    }
    void setRotation(uint8_t) override {
        // Rotation is baked into width/height in the constructor.
    }
};

#endif // ARDUINO_GFX_LIBRARY_H
