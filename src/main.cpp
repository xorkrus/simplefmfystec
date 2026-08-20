/*
 * Simple File Manager for FYSETC SD WIFI Card (ESP8285)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SdFat.h>

// Pin definitions
#define SD_CS 4
#define MISO_PIN 12
#define MOSI_PIN 13
#define SCLK_PIN 14
#define LED_PIN 2

#define HTTP_PORT 80

#define DEFAULT_SSID "xopkland"
#define DEFAULT_PASSWORD "1234567890987654321"
#define AP_SSID "sd-card-3dp"
#define AP_PASSWORD "12345678"

#define MAX_UPLOAD_SIZE (500UL * 1024 * 1024) // 500 MB
#define MIN_FREE_SPACE (10UL * 1024 * 1024)   // 10 MB minimum free space

ESP8266WebServer server(HTTP_PORT);

String wifiSSID = "";
String wifiPassword = "";
bool apMode = false;

extern const char fallbackHTML[];

// SdFat instance
SdFat sd;
SdFile root;

void initSDCard();
void parseSetupIni();
void connectToWiFi();
void startAPMode();
void handleRoot();
void handleListDir();
void handleDeleteFile();
void handleRenameFile();
void handleMoveFile();
void handleCreateDir();
void handleDownload();
void handleUpload();
void handleThumbnail();
void handleNotFound();
String getContentType(String filename);
String urlDecode(String input);
void sendDefaultThumbnail();
bool isValidPath(const String& path);
bool isPathSafe(const String& path);
uint32_t getFreeSpace();
bool fileExists(const char* path);
bool dirExists(const char* path);

#include "html.h"

void setup() {
    Serial.begin(115200);
    delay(100);
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // GPIO15 must be LOW for SD card to work on ESP8266
    pinMode(15, OUTPUT);
    digitalWrite(15, LOW);
    
    initSDCard();
    parseSetupIni();
    connectToWiFi();
    
    server.on("/", handleRoot);
    server.on("/list", handleListDir);
    server.on("/delete", handleDeleteFile);
    server.on("/rename", handleRenameFile);
    server.on("/move", handleMoveFile);
    server.on("/mkdir", handleCreateDir);
    server.on("/download", handleDownload);
    server.on("/upload", HTTP_POST, [](){ server.send(200, "text/plain", "OK"); }, handleUpload);
    server.on("/thumb", handleThumbnail);
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("HTTP server started");
    
    digitalWrite(LED_PIN, HIGH);
}

void loop() {
    server.handleClient();
    yield();
}

void initSDCard() {
    Serial.println("Initializing SD card...");
    
    // Configure SPI pins
    SPI.pins(SCLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
    
    // Try different SPI speeds
    uint32_t speeds[] = {400000, 1000000, 4000000, 8000000, 16000000};
    bool initialized = false;
    
    for (int i = 0; i < 5 && !initialized; i++) {
        Serial.print("Trying SPI speed: ");
        Serial.print(speeds[i]);
        Serial.println(" Hz");
        
        if (sd.begin(SD_CS, SD_SCK_HZ(speeds[i]))) {
            initialized = true;
            Serial.print("SD card initialized successfully at ");
            Serial.print(speeds[i]);
            Serial.println(" Hz");
            
            // Print card info
            Serial.print("Card type: ");
            switch (sd.card()->type()) {
                case SD_CARD_TYPE_SD1:
                    Serial.println("SD1");
                    break;
                case SD_CARD_TYPE_SD2:
                    Serial.println("SD2");
                    break;
                case SD_CARD_TYPE_SDHC:
                    Serial.println("SDHC");
                    break;
                default:
                    Serial.println("Unknown");
            }
            
            // Print volume info
            uint32_t volumeSize = sd.vol()->blocksPerCluster() * sd.vol()->clusterCount() / 2048;
            Serial.print("Volume size (MB): ");
            Serial.println(volumeSize);
            
        } else {
            Serial.print("Failed at ");
            Serial.print(speeds[i]);
            Serial.println(" Hz");
            sd.initErrorPrint(&Serial);
            delay(100);
        }
    }
    
    if (!initialized) {
        Serial.println("SD card initialization failed!");
        Serial.println("Check:");
        Serial.println("- SD card is inserted");
        Serial.println("- SD card is formatted as FAT16/FAT32");
        Serial.println("- Wiring is correct");
        Serial.println("- CS pin is correct");
        return;
    }
    
    // Open root directory
    if (!root.open("/")) {
        Serial.println("Failed to open root directory!");
        return;
    }
    
    Serial.println("SD card ready.");
}

void parseSetupIni() {
    SdFile setupFile;
    if (!setupFile.open("/SETUP.INI", O_READ)) {
        Serial.println("SETUP.INI not found, using defaults");
        wifiSSID = DEFAULT_SSID;
        wifiPassword = DEFAULT_PASSWORD;
        return;
    }
    
    String line;
    bool inWifiSection = false;
    char c;
    
    while (setupFile.available()) {
        c = setupFile.read();
        if (c == '\n' || c == '\r') {
            if (line.length() > 0) {
                line.trim();
                
                if (line.startsWith("[WIFI]")) { 
                    inWifiSection = true; 
                } else if (line.startsWith("[") && !line.startsWith("[WIFI]")) { 
                    inWifiSection = false; 
                } else if (inWifiSection) {
                    if (line.startsWith("SSID=")) { 
                        wifiSSID = line.substring(5); 
                        wifiSSID.trim(); 
                    } else if (line.startsWith("PASSWORD=")) { 
                        wifiPassword = line.substring(9); 
                        wifiPassword.trim(); 
                    }
                }
                line = "";
            }
        } else {
            line += c;
        }
    }
    
    // Process last line if no newline
    if (line.length() > 0 && inWifiSection) {
        line.trim();
        if (line.startsWith("SSID=")) { 
            wifiSSID = line.substring(5); 
            wifiSSID.trim(); 
        } else if (line.startsWith("PASSWORD=")) { 
            wifiPassword = line.substring(9); 
            wifiPassword.trim(); 
        }
    }
    
    setupFile.close();
    Serial.print("WiFi SSID: "); 
    Serial.println(wifiSSID);
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: "); 
    Serial.println(wifiSSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: "); 
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi failed, starting AP mode");
        startAPMode();
    }
}

void startAPMode() {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    Serial.print("AP Mode: "); 
    Serial.print(AP_SSID);
    Serial.print(" Pass: "); 
    Serial.println(AP_PASSWORD);
    Serial.print("AP IP: "); 
    Serial.println(WiFi.softAPIP());
    
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(200);
    }
    digitalWrite(LED_PIN, HIGH);
}

bool isValidPath(const String& path) {
    if (path.isEmpty()) return false;
    if (!path.startsWith("/")) return false;
    return true;
}

bool isPathSafe(const String& path) {
    if (path.indexOf("..") >= 0) return false;
    if (path.indexOf("//") >= 0) return false;
    if (path.length() > 255) return false;
    return true;
}

bool fileExists(const char* path) {
    SdFile file;
    return file.open(path, O_READ);
}

bool dirExists(const char* path) {
    SdFile dir;
    return dir.open(path, O_READ) && dir.isDir();
}

uint32_t getFreeSpace() {
    uint32_t freeClusters = sd.vol()->freeClusterCount();
    uint32_t bytesPerCluster = sd.vol()->blocksPerCluster() * 512;
    return freeClusters * bytesPerCluster;
}

void handleRoot() {
    if (fileExists("/index.html")) {
        SdFile file;
        if (file.open("/index.html", O_READ)) {
            server.streamFile(file, "text/html");
            file.close();
            return;
        }
    }
    server.send(200, "text/html", fallbackHTML);
}

void handleListDir() {
    String path = server.arg("path");
    if (path.isEmpty()) path = "/";
    
    if (!isValidPath(path) || !isPathSafe(path)) {
        server.send(400, "application/json", "{\"error\":\"Invalid path\"}");
        return;
    }
    
    SdFile dir;
    if (!dir.open(path.c_str(), O_READ) || !dir.isDir()) {
        server.send(404, "application/json", "{\"error\":\"Path not found\"}");
        return;
    }
    
    String json = "{\"dirs\":[";
    String files = "";
    bool firstDir = true;
    
    SdFile entry;
    while (entry.openNext(&dir, O_READ)) {
        char name[256];
        entry.getName(name, sizeof(name));
        
        if (entry.isDir()) {
            if (!firstDir) json += ",";
            json += "\"" + String(name) + "\"";
            firstDir = false;
        } else {
            if (files.length() > 0) files += ",";
            files += "{\"name\":\"" + String(name) + "\",\"size\":" + String(entry.fileSize()) + "}";
        }
        entry.close();
    }
    dir.close();
    
    json += "],\"files\":[" + files + "]}";
    server.send(200, "application/json", json);
}

void handleDeleteFile() {
    String path = "/" + urlDecode(server.arg("path"));
    
    while (path.startsWith("//")) path = path.substring(1);
    if (path.endsWith("/") && path.length() > 1) {
        path = path.substring(0, path.length() - 1);
    }
    
    if (!isValidPath(path) || !isPathSafe(path)) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    
    if (path == "/") {
        server.send(403, "text/plain", "Cannot delete root directory");
        return;
    }
    
    SdFile file;
    if (!file.open(path.c_str(), O_READ)) {
        server.send(404, "text/plain", "Not found");
        return;
    }
    
    bool deleted = false;
    if (file.isDir()) {
        file.close();
        deleted = sd.rmdir(path.c_str());
    } else {
        file.close();
        deleted = sd.remove(path.c_str());
    }
    
    if (deleted) {
        server.send(200, "text/plain", "Deleted");
    } else {
        server.send(500, "text/plain", "Delete failed");
    }
}

void handleRenameFile() {
    String oldPath = "/" + urlDecode(server.arg("old"));
    String newPath = "/" + urlDecode(server.arg("new"));
    
    while (oldPath.startsWith("//")) oldPath = oldPath.substring(1);
    while (newPath.startsWith("//")) newPath = newPath.substring(1);
    
    if (!isValidPath(oldPath) || !isValidPath(newPath) || 
        !isPathSafe(oldPath) || !isPathSafe(newPath)) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    
    SdFile file;
    if (!file.open(oldPath.c_str(), O_READ)) {
        server.send(404, "text/plain", "Source not found");
        return;
    }
    file.close();
    
    if (sd.rename(oldPath.c_str(), newPath.c_str())) {
        server.send(200, "text/plain", "Renamed");
    } else {
        server.send(500, "text/plain", "Rename failed");
    }
}

void handleMoveFile() {
    String src = "/" + urlDecode(server.arg("src"));
    String dst = "/" + urlDecode(server.arg("dst"));
    
    while (src.startsWith("//")) src = src.substring(1);
    while (dst.startsWith("//")) dst = dst.substring(1);
    
    if (!isValidPath(src) || !isValidPath(dst) || 
        !isPathSafe(src) || !isPathSafe(dst)) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    
    SdFile file;
    if (!file.open(src.c_str(), O_READ)) {
        server.send(404, "text/plain", "Source not found");
        return;
    }
    file.close();
    
    int lastSlash = dst.lastIndexOf('/');
    if (lastSlash > 0) {
        String destDir = dst.substring(0, lastSlash);
        if (!dirExists(destDir.c_str())) {
            sd.mkdir(destDir.c_str(), true);
        }
    }
    
    if (sd.rename(src.c_str(), dst.c_str())) {
        server.send(200, "text/plain", "Moved");
    } else {
        server.send(500, "text/plain", "Move failed");
    }
}

void handleCreateDir() {
    String path = "/" + urlDecode(server.arg("path"));
    
    while (path.startsWith("//")) path = path.substring(1);
    
    if (!isValidPath(path) || !isPathSafe(path)) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    
    if (sd.mkdir(path.c_str(), true)) {
        server.send(200, "text/plain", "Created");
    } else {
        server.send(500, "text/plain", "Create failed");
    }
}

void handleDownload() {
    String path = "/" + urlDecode(server.arg("file"));
    
    while (path.startsWith("//")) path = path.substring(1);
    
    if (!isValidPath(path) || !isPathSafe(path)) {
        server.send(400, "text/plain", "Invalid path");
        return;
    }
    
    SdFile file;
    if (!file.open(path.c_str(), O_READ)) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    
    String contentType = getContentType(path);
    String fileName = path.substring(path.lastIndexOf('/') + 1);
    
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
    server.streamFile(file, contentType);
    file.close();
}

void handleUpload() {
    static SdFile fsUploadFile;
    static String uploadPath;
    static uint32_t totalUploaded = 0;
    
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        uploadPath = server.arg("path");
        if (uploadPath.isEmpty()) uploadPath = "/";
        
        if (!isValidPath(uploadPath) || !isPathSafe(uploadPath)) {
            server.send(400, "text/plain", "Invalid path");
            return;
        }
        
        String filename = uploadPath;
        if (!filename.endsWith("/")) filename += "/";
        filename += upload.filename;
        
        while (filename.indexOf("//") >= 0) filename.replace("//", "/");
        if (!filename.startsWith("/")) filename = "/" + filename;
        
        uint32_t freeSpace = getFreeSpace();
        if (freeSpace < MIN_FREE_SPACE) {
            server.send(507, "text/plain", "Insufficient free space");
            return;
        }
        
        Serial.print("Upload: "); 
        Serial.println(filename);
        Serial.print("File size: "); 
        Serial.println(upload.totalSize);
        
        if (upload.totalSize > MAX_UPLOAD_SIZE) {
            server.send(413, "text/plain", "File too large");
            return;
        }
        
        if (upload.totalSize > freeSpace) {
            server.send(507, "text/plain", "Not enough space for this file");
            return;
        }
        
        if (!fsUploadFile.open(filename.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {
            server.send(500, "text/plain", "Cannot create file");
            return;
        }
        totalUploaded = 0;
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile.isOpen()) {
            size_t written = fsUploadFile.write(upload.buf, upload.currentSize);
            totalUploaded += written;
            
            if (totalUploaded > MAX_UPLOAD_SIZE) {
                fsUploadFile.close();
                sd.remove(uploadPath.c_str());
                server.send(413, "text/plain", "File too large");
                return;
            }
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile.isOpen()) {
            fsUploadFile.close();
            Serial.println("Upload complete");
            Serial.print("Total uploaded: ");
            Serial.println(totalUploaded);
        }
    }
}

void handleThumbnail() {
    String filePath = server.arg("file");
    
    if (filePath == "default") {
        sendDefaultThumbnail();
        return;
    }
    
    filePath = "/" + urlDecode(filePath);
    
    while (filePath.startsWith("//")) filePath = filePath.substring(1);
    
    if (!isValidPath(filePath) || !isPathSafe(filePath)) {
        sendDefaultThumbnail();
        return;
    }
    
    if (!fileExists(filePath.c_str())) {
        sendDefaultThumbnail();
        return;
    }
    
    String baseName = filePath;
    int dotPos = baseName.lastIndexOf('.');
    if (dotPos > 0) baseName = baseName.substring(0, dotPos);
    
    String thumbPath;
    if (fileExists((baseName + ".jpg").c_str())) {
        thumbPath = baseName + ".jpg";
    } else if (fileExists((baseName + ".png").c_str())) {
        thumbPath = baseName + ".png";
    } else if (fileExists("/logo.jpg")) {
        thumbPath = "/logo.jpg";
    } else if (fileExists("/logo.png")) {
        thumbPath = "/logo.png";
    }
    
    if (!thumbPath.isEmpty() && fileExists(thumbPath.c_str())) {
        SdFile thumb;
        if (thumb.open(thumbPath.c_str(), O_READ)) {
            server.sendHeader("Content-Length", String(thumb.fileSize()));
            server.streamFile(thumb, getContentType(thumbPath));
            thumb.close();
            return;
        }
    }
    
    sendDefaultThumbnail();
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".svg")) return "image/svg+xml";
    else if (filename.endsWith(".gcode") || filename.endsWith(".gco") || filename.endsWith(".g")) return "text/plain";
    return "application/octet-stream";
}

String urlDecode(String input) {
    String decoded = "";
    char buffer[3];
    
    for (unsigned int i = 0; i < input.length(); i++) {
        if (input[i] == '%') {
            if (i + 2 < input.length()) {
                buffer[0] = input[i + 1];
                buffer[1] = input[i + 2];
                buffer[2] = '\0';
                decoded += char(strtol(buffer, nullptr, 16));
                i += 2;
            }
        } else if (input[i] == '+') {
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void sendDefaultThumbnail() {
    static const unsigned char placeholder[] PROGMEM = {
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x0B, 0x0C, 0x0E, 0x0C, 0x0A, 0x10,
        0x0E, 0x0D, 0x0E, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28, 0x1A, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
        0x25, 0x1B, 0x28, 0x3A, 0x33, 0x3D, 0x3C, 0x39, 0x33, 0x38, 0x37, 0x40, 0x48, 0x5C, 0x4E, 0x40,
        0x44, 0x57, 0x45, 0x37, 0x38, 0x52, 0x72, 0x54, 0x57, 0x5F, 0x61, 0x65, 0x68, 0x65, 0x3E, 0x4D,
        0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x63, 0x64, 0x58, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x11, 0x11,
        0x14, 0x13, 0x14, 0x27, 0x16, 0x16, 0x27, 0x50, 0x35, 0x2D, 0x35, 0x50, 0x50, 0x50, 0x50, 0x50,
        0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
        0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
        0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50,
        0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00,
        0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4,
        0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
        0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13,
        0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15,
        0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46,
        0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
        0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
        0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04,
        0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11,
        0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08,
        0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A,
        0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
        0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93,
        0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
        0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00,
        0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0xFB, 0xD5, 0x00, 0x01, 0x85, 0x20, 0x40, 0x01, 0x00,
        0x01, 0xFF, 0xD9
    };
    
    server.sendHeader("Content-Length", String(sizeof(placeholder)));
    server.sendHeader("Content-Type", "image/jpeg");
    server.sendContent_P((const char*)placeholder, sizeof(placeholder));
}
