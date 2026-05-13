#pragma once
#ifndef ARDUINO_H
#define ARDUINO_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <cstdarg>
#include <string>
#include <algorithm>
#include <emscripten.h>
#include <emscripten/emscripten.h>

// ── Identifier for ArduinoJson Arduino-mode detection ────────────────────────
#define ARDUINO 100

// ── AVR/ESP progmem stubs (needed by ArduinoJson Arduino mode) ───────────────
#ifndef PROGMEM
#define PROGMEM
#endif
#define pgm_read_byte(p)  (*((const uint8_t*)(p)))
#define pgm_read_word(p)  (*((const uint16_t*)(p)))
#define pgm_read_dword(p) (*((const uint32_t*)(p)))
#define pgm_read_ptr(p)   (*((const void* const*)(p)))
struct __FlashStringHelper {};
#define F(s) (s)

// ── Pin / digital constants ───────────────────────────────────────────────────
#define HIGH 1
#define LOW  0
#define OUTPUT 1
#define INPUT  0

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

// ── Math constants ────────────────────────────────────────────────────────────
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define DEG_TO_RAD 0.017453292519943f
#define RAD_TO_DEG 57.29577951308232f

// ── Timing ────────────────────────────────────────────────────────────────────
inline uint32_t millis() {
    return (uint32_t)(emscripten_get_now());
}
inline uint32_t micros() {
    return (uint32_t)(emscripten_get_now() * 1000);
}
inline void delay(uint32_t ms) {
    emscripten_sleep(ms);
}
inline void delayMicroseconds(uint32_t us) {
    emscripten_sleep(us / 1000 + 1);
}

// ── Math helpers ──────────────────────────────────────────────────────────────
template<typename T> inline T abs(T v) { return v < 0 ? -v : v; }
template<typename T> inline T min(T a, T b) { return a < b ? a : b; }
template<typename T> inline T max(T a, T b) { return a > b ? a : b; }
inline long constrain(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline long map(long x, long inLow, long inHigh, long outLow, long outHigh) {
    return outLow + (x - inLow) * (outHigh - outLow) / (inHigh - inLow);
}

inline float sq(float x) { return x * x; }

// ── Random ────────────────────────────────────────────────────────────────────
inline void randomSeed(unsigned long s) { srand((unsigned)s); }
inline long random(long maxVal) { return rand() % maxVal; }
inline long random(long minVal, long maxVal) { return minVal + rand() % (maxVal - minVal); }

// ── String class ─────────────────────────────────────────────────────────────
class String {
    std::string _s;
public:
    String() {}
    String(const char* c)         : _s(c ? c : "") {}
    String(const std::string& s)  : _s(s) {}
    String(std::string&& s)       : _s(std::move(s)) {}
    String(char c)                { _s = std::string(1, c); }
    String(int v)                 { char buf[32]; snprintf(buf, 32, "%d", v); _s = buf; }
    String(unsigned int v)        { char buf[32]; snprintf(buf, 32, "%u", v); _s = buf; }
    String(long v)                { char buf[32]; snprintf(buf, 32, "%ld", v); _s = buf; }
    String(unsigned long v)       { char buf[32]; snprintf(buf, 32, "%lu", v); _s = buf; }
    String(long long v)           { char buf[32]; snprintf(buf, 32, "%lld", v); _s = buf; }
    String(unsigned long long v)  { char buf[32]; snprintf(buf, 32, "%llu", v); _s = buf; }
    String(bool b)                { _s = b ? "1" : "0"; }
    String(float v, int dec = 2)  { char buf[64]; snprintf(buf, 64, "%.*f", dec, (double)v); _s = buf; }
    String(double v, int dec = 2) { char buf[64]; snprintf(buf, 64, "%.*f", dec, v); _s = buf; }

    size_t   length()  const { return _s.size(); }
    bool     isEmpty() const { return _s.empty(); }
    const char* c_str() const { return _s.c_str(); }
    void     reserve(size_t n) { _s.reserve(n); }

    // Assignment
    String& operator=(const String& o) { _s = o._s; return *this; }
    String& operator=(const char* c)   { _s = c ? c : ""; return *this; }

    // Concatenation
    String& operator+=(const String& o) { _s += o._s; return *this; }
    String& operator+=(const char* c)   { if(c) _s += c; return *this; }
    String& operator+=(char c)          { _s += c; return *this; }
    String& operator+=(int v)           { *this += String(v); return *this; }
    String& operator+=(float v)         { *this += String(v); return *this; }

    String operator+(const String& o) const { return String(_s + o._s); }
    String operator+(const char* c)   const { return String(_s + (c ? c : "")); }
    String operator+(char c)          const { return String(_s + c); }
    String operator+(int v)           const { return *this + String(v); }
    String operator+(float v)         const { return *this + String(v); }

    // Comparisons
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator==(const char* c)   const { return c && _s == c; }
    bool operator!=(const String& o) const { return _s != o._s; }
    bool operator!=(const char* c)   const { return !(*this == c); }
    bool operator<(const String& o)  const { return _s < o._s; }
    bool operator>(const String& o)  const { return _s > o._s; }
    bool operator<=(const String& o) const { return _s <= o._s; }
    bool operator>=(const String& o) const { return _s >= o._s; }

    // Element access
    char charAt(int i) const { return (i >= 0 && i < (int)_s.size()) ? _s[i] : 0; }
    char operator[](int i) const { return charAt(i); }

    // Search
    int indexOf(char c, int from = 0) const {
        auto p = _s.find(c, from); return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(const String& sub, int from = 0) const {
        auto p = _s.find(sub._s, from); return p == std::string::npos ? -1 : (int)p;
    }
    int lastIndexOf(char c) const {
        auto p = _s.rfind(c); return p == std::string::npos ? -1 : (int)p;
    }
    int lastIndexOf(const String& sub) const {
        auto p = _s.rfind(sub._s); return p == std::string::npos ? -1 : (int)p;
    }

    // Substrings
    String substring(int start, int end = -1) const {
        int len = (int)_s.size();
        if (start < 0) start = 0;
        if (start > len) return String();
        if (end < 0 || end > len) end = len;
        return String(_s.substr(start, end - start));
    }

    // Mutation
    void toLowerCase() { for (auto& c : _s) c = (char)::tolower((unsigned char)c); }
    void toUpperCase() { for (auto& c : _s) c = (char)::toupper((unsigned char)c); }
    void trim() {
        auto s = _s.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) { _s = ""; return; }
        auto e = _s.find_last_not_of(" \t\r\n");
        _s = _s.substr(s, e - s + 1);
    }
    void remove(int start, int count = -1) {
        if (count < 0) _s.erase(start);
        else _s.erase(start, count);
    }
    void replace(char from, char to) {
        for (auto& c : _s) if (c == from) c = to;
    }
    void replace(const String& from, const String& to) {
        size_t pos = 0;
        while ((pos = _s.find(from._s, pos)) != std::string::npos) {
            _s.replace(pos, from._s.size(), to._s);
            pos += to._s.size();
        }
    }
    bool concat(const String& o) { _s += o._s; return true; }
    bool concat(const char* c)   { if(c) _s += c; return true; }

    // Conversion
    float  toFloat() const { return (float)atof(_s.c_str()); }
    int    toInt()   const { return atoi(_s.c_str()); }
    long   toLong()  const { return atol(_s.c_str()); }

    // Checks
    bool startsWith(const String& prefix) const {
        return _s.size() >= prefix._s.size() && _s.compare(0, prefix._s.size(), prefix._s) == 0;
    }
    bool endsWith(const String& suffix) const {
        if (_s.size() < suffix._s.size()) return false;
        return _s.compare(_s.size() - suffix._s.size(), suffix._s.size(), suffix._s) == 0;
    }

    // Implicit conversion to bool for use in conditions
    explicit operator bool() const { return !_s.empty(); }
};

// Free-function concatenation operators
inline String operator+(const char* a, const String& b)  { return String(a) + b; }
inline String operator+(char a, const String& b)         { return String(a) + b; }

// strlen / isalnum / isdigit forwards that take char
using ::strlen;
using ::isalnum;
using ::isdigit;
using ::isspace;

// ── Arduino Print / Stream base classes (needed by ArduinoJson) ──────────────
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* buf, size_t n) {
        size_t written = 0;
        while (n--) written += write(*buf++);
        return written;
    }
    size_t print(const char* s) {
        if (!s) return 0;
        return write((const uint8_t*)s, strlen(s));
    }
    size_t print(const String& s)  { return print(s.c_str()); }
    size_t println(const char* s)  { size_t n = print(s); n += write('\n'); return n; }
    size_t println(const String& s){ return println(s.c_str()); }
    size_t println()               { return write('\n'); }
};

class Printable {
public:
    virtual ~Printable() {}
    virtual size_t printTo(Print& p) const = 0;
};

class Stream : public Print {
public:
    virtual int    available() = 0;
    virtual int    read()      = 0;
    virtual int    peek()      = 0;
    size_t readBytes(char* buf, size_t len) {
        size_t n = 0;
        while (n < len) { int c = read(); if (c < 0) break; buf[n++] = (char)c; }
        return n;
    }
    size_t readBytes(uint8_t* buf, size_t len) { return readBytes((char*)buf, len); }
};

// ── Serial ────────────────────────────────────────────────────────────────────
class HardwareSerial {
public:
    void begin(int) {}
    void print(const char* s)     { if(s) fputs(s, stdout); fflush(stdout); }
    void print(const String& s)   { fputs(s.c_str(), stdout); fflush(stdout); }
    void print(int v)             { printf("%d", v); fflush(stdout); }
    void print(unsigned long v)   { printf("%lu", v); fflush(stdout); }
    void print(float v, int d=2)  { printf("%.*f", d, (double)v); fflush(stdout); }
    void print(char c)            { putchar(c); fflush(stdout); }
    void println(const char* s)   { if(s) puts(s); else puts(""); fflush(stdout); }
    void println(const String& s) { puts(s.c_str()); fflush(stdout); }
    void println(int v)           { printf("%d\n", v); fflush(stdout); }
    void println(unsigned long v) { printf("%lu\n", v); fflush(stdout); }
    void println()                { puts(""); fflush(stdout); }
    void printf(const char* fmt, ...) {
        va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args); fflush(stdout);
    }
};
extern HardwareSerial Serial;

// ── NTP / time ────────────────────────────────────────────────────────────────
inline void configTime(long, long, const char*, const char* = nullptr) {
    // Browser has real wall-clock time via time() automatically
}

// ── ESP.restart() ─────────────────────────────────────────────────────────────
class ESP32Class {
public:
    void restart() {
        EM_ASM({ location.reload(); });
    }
    uint32_t getFreeHeap() { return 1024 * 1024; }
};
extern ESP32Class ESP;

#endif // ARDUINO_H
