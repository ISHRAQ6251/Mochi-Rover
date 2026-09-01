#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>

// Always runs as its own hotspot (SoftAP). The phone joins HERO / hero1234
// and opens http://192.168.4.1 - no lab router, no provisioning.
class WifiHelper {
public:
    void begin();
    void update();          // captive DNS so phones auto-open the cockpit

    bool isApMode() const { return true; }
    bool isConnected() const { return true; }
    String ip() const;

private:
    DNSServer _dns;
};

extern WifiHelper wifiHelper;
