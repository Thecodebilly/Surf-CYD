// shim_impl.cpp – compiled once; provides all shim global instances and
// the SPIFFS / FS method bodies that need emscripten JS calls.

#include "Arduino.h"
#include "WiFi.h"
#include "SPIFFS.h"
#include "FS.h"
#include "SPI.h"
#include <emscripten.h>
#include <cstring>
#include <vector>
#include <string>

// ── HTTP fetch via browser's native fetch() API ──────────────────────────────
// Defined here (once) so the EM_ASYNC_JS symbol only appears in one TU.
// HTTPClient.h declares the extern prototype; all source files link to this.
EM_ASYNC_JS(char*, _js_http_fetch,
    (const char* jsMethod, const char* jsUrl,
     const char* jsHeaders, const char* jsBody),
{
    var method = UTF8ToString(jsMethod);
    var url    = UTF8ToString(jsUrl);
    var opts   = { method: method };

    var hdrStr = UTF8ToString(jsHeaders);
    if (hdrStr && hdrStr.length) {
        opts.headers = {};
        hdrStr.split('\n').forEach(function(line) {
            var sep = line.indexOf(':');
            if (sep > 0) {
                opts.headers[line.slice(0, sep).trim()] = line.slice(sep + 1).trim();
            }
        });
    }

    var body = UTF8ToString(jsBody);
    if (body && body.length) opts.body = body;

    try {
        var resp = await fetch(url, opts);
        Module._lastFetchStatus = resp.status;
        var text = await resp.text();
        var len  = lengthBytesUTF8(text) + 1;
        var ptr  = _malloc(len);
        stringToUTF8(text, ptr, len);
        return ptr;
    } catch (e) {
        Module._lastFetchStatus = 0;
        console.error('[HTTPClient] fetch error:', url, e.message || String(e));
        return 0;
    }
});

// ── Global singletons ─────────────────────────────────────────────────────────
HardwareSerial Serial;
WiFiClass       WiFi;
SPIFFSClass     SPIFFS;
SPIClass        SPI;
ESP32Class      ESP;

// ── SPIFFS: directory listing state ──────────────────────────────────────────
// Used by viewFilesScreen() which calls SPIFFS.open("/") then openNextFile().
static std::vector<std::string> s_spiffsKeys;
static size_t s_spiffsKeyIdx = 0;

// Build a list of all stored SPIFFS paths from localStorage.
static void _loadSpiffsKeyList() {
    s_spiffsKeys.clear();
    s_spiffsKeyIdx = 0;

    // Ask JS for a pipe-delimited list of all SPIFFS keys.
    char* raw = (char*)EM_ASM_PTR({
        var kl = JSON.parse(localStorage.getItem('spiffs:__keys__') || '[]');
        var joined = kl.join('|');
        var len = lengthBytesUTF8(joined) + 1;
        var ptr = _malloc(len);
        stringToUTF8(joined, ptr, len);
        return ptr;
    });

    if (raw && raw[0]) {
        std::string all(raw);
        size_t pos = 0;
        while (pos < all.size()) {
            auto next = all.find('|', pos);
            if (next == std::string::npos) next = all.size();
            s_spiffsKeys.push_back(all.substr(pos, next - pos));
            pos = next + 1;
        }
    }
    if (raw) free(raw);
}

// ── File::openNextFile ─────────────────────────────────────────────────────────
File File::openNextFile() {
    if (s_spiffsKeyIdx >= s_spiffsKeys.size()) return File();
    std::string path = s_spiffsKeys[s_spiffsKeyIdx++];
    return SPIFFS.open(path.c_str(), FILE_READ);
}

// ── File::close ───────────────────────────────────────────────────────────────
void File::close() {
    if (_write_mode && _valid) {
        // Persist buffer to localStorage and record the key in the key list.
        const char* path = _path.c_str();
        const char* buf  = _buf.c_str();
        EM_ASM({
            var path = UTF8ToString($0);
            var data = UTF8ToString($1);
            var key  = 'spiffs:' + path;
            localStorage.setItem(key, data);
            // Maintain key list
            var kl = JSON.parse(localStorage.getItem('spiffs:__keys__') || '[]');
            if (kl.indexOf(path) === -1) {
                kl.push(path);
                localStorage.setItem('spiffs:__keys__', JSON.stringify(kl));
            }
        }, path, buf);
    }
    _valid = false;
    _buf.clear();
    _pos = 0;
}

// ── SPIFFSClass::open ─────────────────────────────────────────────────────────
File SPIFFSClass::open(const char* path, const char* mode) {
    bool isWrite = (mode && (mode[0] == 'w' || mode[0] == 'W'));

    // Special case: opening the root "/" returns a directory-listing File.
    if (strcmp(path, "/") == 0) {
        _loadSpiffsKeyList();
        File f;
        f._path  = "/";
        f._valid = true;
        return f;
    }

    File f;
    f._path       = path;
    f._write_mode = isWrite;
    f._pos        = 0;

    if (!isWrite) {
        // Load existing content from localStorage.
        char* data = (char*)EM_ASM_PTR({
            var key  = 'spiffs:' + UTF8ToString($0);
            var val  = localStorage.getItem(key);
            if (val === null) return 0;
            var len = lengthBytesUTF8(val) + 1;
            var ptr = _malloc(len);
            stringToUTF8(val, ptr, len);
            return ptr;
        }, path);

        if (!data) { f._valid = false; return f; }
        f._buf = std::string(data);
        free(data);
    }
    f._valid = true;
    return f;
}
