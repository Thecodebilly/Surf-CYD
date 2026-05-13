#pragma once
#ifndef WIFI_H
#define WIFI_H

#include "Arduino.h"

// The emulator runs in a browser — the network is always available.
#define WL_CONNECTED  3
#define WL_IDLE_STATUS 0
#define WIFI_STA 1

class IPAddress {
public:
    IPAddress() {}
    String toString() const { return "127.0.0.1"; }
    operator String() const { return toString(); }
};

class WiFiClass {
public:
    void mode(int) {}
    void begin(const char*, const char*) {}
    int  status() { return WL_CONNECTED; }  // always connected in browser
    bool disconnect(bool, bool) { return true; }
    IPAddress localIP() { return IPAddress(); }
};

extern WiFiClass WiFi;

#endif // WIFI_H
