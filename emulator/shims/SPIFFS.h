#pragma once
#ifndef SPIFFS_H
#define SPIFFS_H

#include "Arduino.h"
#include "FS.h"
#include <emscripten.h>

// ── SPIFFS shim ───────────────────────────────────────────────────────────────
// Files are stored in localStorage under the key "spiffs:<path>".
// A second localStorage entry "spiffs:__keys__" holds a JSON array of all
// stored paths so we can implement exists() and directory listing.

class SPIFFSClass {
public:
    bool begin(bool = false) { return true; }

    bool exists(const char* path) {
        return EM_ASM_INT({
            var key = 'spiffs:' + UTF8ToString($0);
            return (localStorage.getItem(key) !== null) ? 1 : 0;
        }, path);
    }
    bool exists(const String& path) { return exists(path.c_str()); }

    File open(const char* path, const char* mode = "r");
    File open(const String& path, const char* mode = "r") { return open(path.c_str(), mode); }

    bool remove(const char* path) {
        EM_ASM({
            var key = 'spiffs:' + UTF8ToString($0);
            localStorage.removeItem(key);
            // Remove from key list
            var kl = JSON.parse(localStorage.getItem('spiffs:__keys__') || '[]');
            var path = UTF8ToString($0);
            var idx = kl.indexOf(path);
            if (idx >= 0) { kl.splice(idx, 1); localStorage.setItem('spiffs:__keys__', JSON.stringify(kl)); }
        }, path);
        return true;
    }
    bool remove(const String& path) { return remove(path.c_str()); }
};

extern SPIFFSClass SPIFFS;

#endif // SPIFFS_H
