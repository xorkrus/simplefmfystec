#include "web_server.h"
#include "config.h"
#include "html_index.h"
#include "html_index_m.h"
#include <FS.h>

WebServer::WebServer(uint16_t port) : server(port) {}

void WebServer::begin(SDManager* sdManager) {
    sd = sdManager;
    setupRoutes();
    server.begin();
}

void WebServer::setupRoutes() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        serveStaticPage(request, "/index.html", INDEX_HTML);
    });
    server.on("/index.html", HTTP_GET, [this](AsyncWebServerRequest *request){
        serveStaticPage(request, "/index.html", INDEX_HTML);
    });
    server.on("/index_m.html", HTTP_GET, [this](AsyncWebServerRequest *request){
        serveStaticPage(request, "/index_m.html", INDEX_M_HTML);
    });

    server.on("/api/files", HTTP_GET, [this](AsyncWebServerRequest *request){ handleFiles(request); });
    server.on("/api/download", HTTP_GET, [this](AsyncWebServerRequest *request){ handleDownload(request); });

    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (index == 0) {
            String body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body);
            handleDelete(request, doc.as<JsonVariant>());
        }
    });
    server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (index == 0) {
            String body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body);
            handleRename(request, doc.as<JsonVariant>());
        }
    });
    server.on("/api/move", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (index == 0) {
            String body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body);
            handleMove(request, doc.as<JsonVariant>());
        }
    });
    server.on("/api/mkdir", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (index == 0) {
            String body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, body);
            handleMkdir(request, doc.as<JsonVariant>());
        }
    });

    server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){}, [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
        handleUpload(request, filename, index, data, len, final);
    });

    server.on("/api/thumbnail", HTTP_GET, [this](AsyncWebServerRequest *request){ handleThumbnail(request); });
}

void WebServer::serveStaticPage(AsyncWebServerRequest *request, const String& pageName, const char* fallbackHtml) {
    if (sd->exists(pageName)) {
        FsFile f = sd->openFile(pageName);
        if (f) {
            size_t size = f.size();
            if (size > 0) {
                uint8_t* buf = (uint8_t*)malloc(size);
                if (buf) {
                    f.read(buf, size);
                    request->send(200, "text/html", String((char*)buf));
                    free(buf);
                } else {
                    request->send(500, "text/plain", "Memory error");
                }
            } else {
                request->send(200, "text/html", "");
            }
            f.close();
            return;
        }
    }
    request->send(200, "text/html", fallbackHtml);
}

void WebServer::handleFiles(AsyncWebServerRequest *request) {
    String path = "/";
    if (request->hasParam("path")) {
        path = request->getParam("path")->value();
        if (!path.startsWith("/")) path = "/" + path;
    }
    String json;
    if (!sd->listDirectory(path, json)) {
        request->send(500, "application/json", "{\"error\":\"Failed to list directory\"}");
        return;
    }
    request->send(200, "application/json", "{\"path\":\"" + path + "\",\"items\":" + json + "}");
}

void WebServer::handleDownload(AsyncWebServerRequest *request) {
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }
    String path = request->getParam("path")->value();
    if (!sd->exists(path)) {
        request->send(404, "text/plain", "File not found");
        return;
    }
    FsFile f = sd->openFile(path);
    if (!f) {
        request->send(500, "text/plain", "Cannot open file");
        return;
    }
    size_t size = f.size();
    if (size == 0) {
        request->send(200, "application/octet-stream", "");
        f.close();
        return;
    }
    uint8_t* buf = (uint8_t*)malloc(size);
    if (!buf) {
        request->send(500, "text/plain", "Memory error");
        f.close();
        return;
    }
    f.read(buf, size);
    AsyncWebServerResponse *resp = request->beginResponse(200, "application/octet-stream", String((char*)buf));
    resp->addHeader("Content-Disposition", "attachment; filename=\"" + String(f.name()) + "\"");
    request->send(resp);
    free(buf);
    f.close();
}

void WebServer::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static FsFile file;
    if (index == 0) {
        String path = "/" + filename;
        file = sd->openFile(path, O_WRONLY | O_CREAT | O_TRUNC);
        if (!file) {
            request->send(500, "text/plain", "Cannot create file");
            return;
        }
    }
    if (file) {
        file.write(data, len);
    }
    if (final) {
        if (file) {
            file.close();
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Upload error");
        }
    }
}

void WebServer::handleDelete(AsyncWebServerRequest *request, JsonVariant& json) {
    if (!json.containsKey("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }
    String path = json["path"].as<String>();
    if (!sd->exists(path)) {
        request->send(404, "text/plain", "Not found");
        return;
    }
    if (sd->removePath(path)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Delete failed");
    }
}

void WebServer::handleRename(AsyncWebServerRequest *request, JsonVariant& json) {
    if (!json.containsKey("path") || !json.containsKey("newname")) {
        request->send(400, "text/plain", "Missing path or newname");
        return;
    }
    String oldPath = json["path"].as<String>();
    String newName = json["newname"].as<String>();
    int lastSlash = oldPath.lastIndexOf('/');
    String dir = (lastSlash >= 0) ? oldPath.substring(0, lastSlash + 1) : "/";
    String newPath = dir + newName;
    if (sd->renamePath(oldPath, newPath)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Rename failed");
    }
}

void WebServer::handleMove(AsyncWebServerRequest *request, JsonVariant& json) {
    if (!json.containsKey("source") || !json.containsKey("dest")) {
        request->send(400, "text/plain", "Missing source or dest");
        return;
    }
    String src = json["source"].as<String>();
    String dst = json["dest"].as<String>();
    if (sd->movePath(src, dst)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Move failed");
    }
}

void WebServer::handleMkdir(AsyncWebServerRequest *request, JsonVariant& json) {
    if (!json.containsKey("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }
    String path = json["path"].as<String>();
    if (sd->createDirectory(path)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Mkdir failed");
    }
}

void WebServer::handleThumbnail(AsyncWebServerRequest *request) {
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing path");
        return;
    }
    String path = request->getParam("path")->value();
    String thumb = sd->getThumbnailPath(path);
    if (thumb.length() > 0) {
        FsFile f = sd->openFile(thumb);
        if (f) {
            size_t size = f.size();
            if (size == 0) {
                request->send(404, "text/plain", "Empty");
                f.close();
                return;
            }
            uint8_t* buf = (uint8_t*)malloc(size);
            if (!buf) {
                request->send(500, "text/plain", "Memory error");
                f.close();
                return;
            }
            f.read(buf, size);
            String contentType = thumb.endsWith(".jpg") ? "image/jpeg" : "image/png";
            request->send(200, contentType, String((char*)buf));
            free(buf);
            f.close();
            return;
        }
    }
    request->send(404, "text/plain", "Thumbnail not found");
}
