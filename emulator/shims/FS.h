#pragma once
#ifndef FS_H
#define FS_H

#include "Arduino.h"
#include <string>
#include <vector>
#include <emscripten.h>

#define FILE_READ  "r"
#define FILE_WRITE "w"

// ── File class ────────────────────────────────────────────────────────────────
// Extends Stream so ArduinoJson's deserializeJson(doc, file) works correctly.
// Backed by localStorage via JavaScript.
class File : public Stream {
public:
    std::string _path;
    std::string _buf;
    size_t      _pos  = 0;
    bool        _write_mode = false;
    bool        _valid = false;

    File() = default;

    explicit operator bool() const { return _valid; }

    // ── Stream interface (required by ArduinoJson) ────────────────────────────
    int    read()      override { if (_pos >= _buf.size()) return -1; return (unsigned char)_buf[_pos++]; }
    int    available() override { return (int)(_buf.size() - _pos); }
    int    peek()      override { if (_pos >= _buf.size()) return -1; return (unsigned char)_buf[_pos]; }

    // ── Print interface (required by ArduinoJson serialization) ──────────────
    size_t write(uint8_t c) override {
        if (_write_mode) _buf += (char)c;
        return 1;
    }
    size_t write(const uint8_t* data, size_t len) {
        if (_write_mode) _buf.append((const char*)data, len);
        return len;
    }
    size_t print(const char* s) {
        if (!s) return 0;
        size_t n = strlen(s);
        if (_write_mode) _buf.append(s, n);
        return n;
    }
    size_t print(const String& s)   { return print(s.c_str()); }
    size_t println(const char* s)   { size_t n = print(s); write('\n'); return n + 1; }
    size_t println(const String& s) { return println(s.c_str()); }

    size_t size() const { return _buf.size(); }

    // ── SPIFFS directory listing ──────────────────────────────────────────────
    File openNextFile();

    // Flush write buffer to localStorage and mark invalid.
    void close();

    const char* name() const { return _path.c_str(); }
};

#endif // FS_H
