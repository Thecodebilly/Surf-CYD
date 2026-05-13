#pragma once
#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "Arduino.h"
#include <emscripten.h>
#include <vector>
#include <string>

#define HTTP_CODE_OK      200
#define HTTP_CODE_CREATED 201
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1

// ── Native browser fetch (defined once in shim_impl.cpp via EM_ASYNC_JS) ─────
// Returns a malloc'd UTF-8 response body (caller must free()), or 0 on failure.
// Sets Module._lastFetchStatus to the HTTP status code.
#ifdef __cplusplus
extern "C"
#endif
char* _js_http_fetch(const char* jsMethod, const char* jsUrl,
                     const char* jsHeaders, const char* jsBody);

// ── HTTPClient shim ───────────────────────────────────────────────────────────
class HTTPClient {
    std::string              _url;
    std::string              _responseBody;
    int                      _statusCode = 0;
    int                      _timeout    = 10000;
    std::vector<std::string> _headerKV;   // alternating key, value

    int _sendRequest(const char* method, const char* body = nullptr) {
        // Encode headers as "Key: Value\n..." for the JS side
        std::string hdrStr;
        for (size_t i = 0; i + 1 < _headerKV.size(); i += 2)
            hdrStr += _headerKV[i] + ": " + _headerKV[i+1] + "\n";

        char* resp = _js_http_fetch(
            method,
            _url.c_str(),
            hdrStr.c_str(),
            body ? body : ""
        );

        _statusCode = (int)EM_ASM_INT({ return Module._lastFetchStatus | 0; });

        if (resp) {
            _responseBody = std::string(resp);
            free(resp);
        } else {
            _responseBody.clear();
            if (_statusCode == 0) _statusCode = -1;
        }
        return _statusCode;
    }

public:
    void begin(const String& url) { _url = url.c_str(); _responseBody.clear(); }
    void begin(const char* url)   { _url = url;         _responseBody.clear(); }
    void setTimeout(int ms)       { _timeout = ms; }
    void setFollowRedirects(int)  {}

    void addHeader(const String& key, const String& value) {
        _headerKV.push_back(key.c_str());
        _headerKV.push_back(value.c_str());
    }
    void addHeader(const char* key, const char* value) {
        _headerKV.push_back(key);
        _headerKV.push_back(value);
    }

    int GET() { return _sendRequest("GET"); }

    int POST(const String& body) {
        bool hasCT = false;
        for (size_t i = 0; i < _headerKV.size(); i += 2)
            if (_headerKV[i] == "Content-Type") { hasCT = true; break; }
        if (!hasCT) {
            _headerKV.push_back("Content-Type");
            _headerKV.push_back("application/json");
        }
        return _sendRequest("POST", body.c_str());
    }

    String getString()      { return String(_responseBody.c_str()); }
    int    getResponseCode(){ return _statusCode; }

    void end() {
        _url.clear();
        _responseBody.clear();
        _headerKV.clear();
        _statusCode = 0;
    }
};

#endif // HTTPCLIENT_H
