#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include "settings.h"

// Handles station-mode connection, fallback SoftAP configuration mode and
// mDNS advertisement. The actual configuration portal UI lives in the web
// server: when in AP mode the web UI shows the connection settings panel at
// 192.168.4.1.
class WifiHelper {
public:
    void begin();
    void update();          // keeps DNS captive portal alive in AP mode

    // Apply new credentials and reconnect. Returns true if stored.
    bool apply(const char* ssid, const char* pass);

    bool isApMode() const { return _apMode; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    String ip() const;
    bool haveSaved() const { return settings.hasWifi(); }

private:
    void startAp();
    void connectStation();

    bool _apMode = false;
    DNSServer _dns;
};

extern WifiHelper wifiHelper;
