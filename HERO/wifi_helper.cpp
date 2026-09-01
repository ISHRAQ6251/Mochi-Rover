#include "wifi_helper.h"
#include "config.h"
#include <ESPmDNS.h>

WifiHelper wifiHelper;

void WifiHelper::begin() {
    WiFi.mode(WIFI_AP);
    WiFi.setHostname(HOSTNAME);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(200);

    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(53, "*", WiFi.softAPIP());

    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
    }
}

void WifiHelper::update() {
    _dns.processNextRequest();
}

String WifiHelper::ip() const {
    return WiFi.softAPIP().toString();
}
