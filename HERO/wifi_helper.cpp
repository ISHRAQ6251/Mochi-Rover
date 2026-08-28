#include "wifi_helper.h"
#include "config.h"
#include <ESPmDNS.h>

WifiHelper wifiHelper;

void WifiHelper::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);

    if (settings.hasWifi()) {
        connectStation();
    }
    if (WiFi.status() != WL_CONNECTED) {
        startAp();
    }

    // mDNS so the UI can use http://hero.local
    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
    }
}

void WifiHelper::update() {
    if (_apMode) {
        _dns.processNextRequest();
        return;
    }
    static uint32_t lastCheck = 0;
    if (WiFi.status() != WL_CONNECTED && settings.hasWifi()) {
        if (millis() - lastCheck > 30000UL) {
            lastCheck = millis();
            WiFi.reconnect();
        }
    }
}

String WifiHelper::ip() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    if (_apMode) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

bool WifiHelper::apply(const char* ssid, const char* pass) {
    settings.setWifi(ssid, pass);
    _apMode = false;
    _dns.stop();
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.data.wifiSSID, settings.data.wifiPass);  // non-blocking
    return true;
}

void WifiHelper::connectStation() {
    WiFi.begin(settings.data.wifiSSID, settings.data.wifiPass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000UL) {
        delay(100);
    }
}

void WifiHelper::startAp() {
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(200);
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(53, "*", WiFi.softAPIP());
}
