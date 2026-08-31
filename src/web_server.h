#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "sd_manager.h"

class WebServer {
public:
    void begin(SDManager* sd);
    void handleClient();
private:
    AsyncWebServer server;
    SDManager* sd;
    void setupRoutes();
    void handleFiles(AsyncWebServerRequest *request);
    void handleDownload(AsyncWebServerRequest *request);
    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleDelete(AsyncWebServerRequest *request, JsonVariant& json);
    void handleRename(AsyncWebServerRequest *request, JsonVariant& json);
    void handleMove(AsyncWebServerRequest *request, JsonVariant& json);
    void handleMkdir(AsyncWebServerRequest *request, JsonVariant& json);
    void handleThumbnail(AsyncWebServerRequest *request);
    void serveStaticPage(AsyncWebServerRequest *request, const String& pageName, const char* fallbackHtml);
};

#endif
