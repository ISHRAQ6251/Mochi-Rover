#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// REST API + static asset server with a token-based auth gate.
class WebServerMgr {
public:
    void begin();

private:
    AsyncWebServer _server{HTTP_PORT};
};

extern WebServerMgr webServerMgr;
