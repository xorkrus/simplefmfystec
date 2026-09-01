// main.cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "config.h"
#include "html.h"

// Глобальные объекты
ESP8266WebServer server(HTTP_PORT);
File uploadFile;
bool sdAvailable = false;
String currentWifiStatus = "";
unsigned long lastLedToggle = 0;
int ledState = LED_ON;

// Прототипы функций
void setup();
void loop();
void initWiFi();
bool readSetupIni(String &ssid, String &password);
void startAP();
void initSD();
bool checkBusy();
void handleRoot();
void handleApiList();
void handleFileUpload();
void handleApiDownload();
void handleApiDelete();
void handleApiRename();
void handleApiMove();
void handleApiMkdir();
void handleApiThumbnail();
void handleNotFound();
String getContentType(String filename);
bool deleteRecursive(String path);
void sendJsonError(int code, String message);
void updateLed();

// Состояния LED
enum LedState {
  LED_STATE_INIT,
  LED_STATE_WIFI_OK,
  LED_STATE_SD_FAIL,
  LED_STATE_OK,
  LED_STATE_UPLOADING
};
volatile LedState currentLedState = LED_STATE_INIT;
bool uploading = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nStarting SD WiFi File Manager...");

  // Настройка пинов
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);
  // CS_SENSE: используем INPUT, так как на плате может быть внешняя подтяжка
  pinMode(CS_SENSE, INPUT);
  Serial.print("CS_SENSE initial state: ");
  Serial.println(digitalRead(CS_SENSE) ? "HIGH" : "LOW");

  // Инициализация SD
  initSD();
  if (!sdAvailable) {
    currentLedState = LED_STATE_SD_FAIL;
  }

  // Подключение Wi-Fi
  initWiFi();

  // Настройка веб-сервера
  server.on("/", handleRoot);
  server.on("/api/list", HTTP_GET, handleApiList);
  server.on("/api/upload", HTTP_POST, [](){ server.send(200, "application/json", "{\"success\":true}"); }, handleFileUpload);
  server.on("/api/download", HTTP_GET, handleApiDownload);
  server.on("/api/delete", HTTP_POST, handleApiDelete);
  server.on("/api/rename", HTTP_POST, handleApiRename);
  server.on("/api/move", HTTP_POST, handleApiMove);
  server.on("/api/mkdir", HTTP_POST, handleApiMkdir);
  server.on("/api/thumbnail", HTTP_GET, handleApiThumbnail);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  if (WiFi.getMode() == WIFI_STA) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    currentLedState = LED_STATE_WIFI_OK;
  } else {
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    currentLedState = LED_STATE_WIFI_OK;
  }
}

void loop() {
  server.handleClient();
  updateLed();
}

// ==================== Инициализация Wi-Fi ====================
void initWiFi() {
  String ssid, password;
  bool haveConfig = readSetupIni(ssid, password);
  bool connected = false;

  if (haveConfig) {
    Serial.printf("Trying WiFi from SETUP.INI: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\nConnected to WiFi!");
    } else {
      Serial.println("\nFailed to connect with SETUP.INI");
      WiFi.disconnect();
    }
  }

  if (!connected) {
    Serial.printf("Trying fallback WiFi: %s\n", FALLBACK_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(FALLBACK_SSID, FALLBACK_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\nConnected to fallback WiFi!");
    } else {
      Serial.println("\nFailed to connect to fallback, starting AP...");
      WiFi.disconnect();
    }
  }

  if (!connected) {
    startAP();
  }
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("AP started: %s / %s\n", AP_SSID, AP_PASSWORD);
}

// Чтение SETUP.INI с SD
bool readSetupIni(String &ssid, String &password) {
  if (!sdAvailable) return false;
  File f = SD.open(SETUP_INI_FILENAME, FILE_READ);
  if (!f) {
    Serial.println("SETUP.INI not found");
    return false;
  }

  String content;
  while (f.available()) {
    content += (char)f.read();
  }
  f.close();

  // Парсинг
  int sectionStart = content.indexOf("[WIFI]");
  if (sectionStart == -1) return false;
  int sectionEnd = content.indexOf('[', sectionStart + 1);
  if (sectionEnd == -1) sectionEnd = content.length();
  String wifiSection = content.substring(sectionStart, sectionEnd);

  int ssidPos = wifiSection.indexOf("SSID=");
  int passPos = wifiSection.indexOf("PASSWORD=");
  if (ssidPos == -1 || passPos == -1) return false;

  ssid = wifiSection.substring(ssidPos + 5, wifiSection.indexOf('\n', ssidPos));
  ssid.trim();
  password = wifiSection.substring(passPos + 9, wifiSection.indexOf('\n', passPos));
  password.trim();

  if (ssid.length() == 0 || password.length() == 0) return false;
  return true;
}

// ==================== Инициализация SD ====================
void initSD() {
  Serial.print("Initializing SD card...");
  // Настраиваем CS пин
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(100);
  
  // Явно инициализируем SPI
  SPI.begin();
  // Устанавливаем частоту для совместимости
  SPI.setFrequency(4000000);
  
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    Serial.println(" OK");
  } else {
    sdAvailable = false;
    Serial.println(" FAILED");
  }
}

// Проверка занятости шины Marlin
bool checkBusy() {
  // Активный уровень LOW: если Marlin управляет шиной, пин притянут к земле.
  if (digitalRead(CS_SENSE) == LOW) {
    Serial.println("SD bus busy (Marlin active)");
    return true;
  }
  return false;
}

// ==================== Обработчики веб-сервера ====================
void handleRoot() {
  if (checkBusy()) {
    server.send(503, "text/plain", "SD card busy");
    return;
  }

  String userAgent = server.header("User-Agent");
  bool isMobile = userAgent.indexOf("Mobile") != -1 || userAgent.indexOf("Android") != -1;

  // Проверяем наличие пользовательского index.html на SD
  String indexFile = isMobile ? "/index_m.html" : "/index.html";
  if (sdAvailable && SD.exists(indexFile)) {
    File f = SD.open(indexFile, FILE_READ);
    if (f) {
      server.streamFile(f, "text/html");
      f.close();
      return;
    }
  }

  // Fallback встроенный
  if (isMobile) {
    server.send_P(200, "text/html", INDEX_M_HTML);
  } else {
    server.send_P(200, "text/html", INDEX_HTML);
  }
}

void handleApiList() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String path = server.arg("path");
  if (path.length() == 0) path = "/";
  if (!path.startsWith("/")) path = "/" + path;

  // Проверка безопасности пути
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  File dir = SD.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    sendJsonError(404, "Directory not found");
    if (dir) dir.close();
    return;
  }

  DynamicJsonDocument doc(8192);
  doc["path"] = path;
  JsonArray files = doc.createNestedArray("files");

  dir.rewindDirectory();
  File entry;
  while ((entry = dir.openNextFile())) {
    if (strlen(entry.name()) == 0) break;
    JsonObject fileObj = files.createNestedObject();
    fileObj["name"] = entry.name();
    fileObj["size"] = entry.size();
    fileObj["isDir"] = entry.isDirectory();
    String fullPath = path;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += entry.name();
    fileObj["path"] = fullPath;
    entry.close();
  }
  dir.close();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleFileUpload() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploading = true;
    String path = server.arg("path");
    if (path.length() == 0) path = "/";
    if (!path.endsWith("/")) path += "/";
    String filename = upload.filename;
    int slash = filename.lastIndexOf('/');
    if (slash != -1) filename = filename.substring(slash + 1);
    int backslash = filename.lastIndexOf('\\');
    if (backslash != -1) filename = filename.substring(backslash + 1);
    if (filename.length() == 0) filename = "unnamed";
    String fullPath = path + filename;
    if (fullPath.indexOf("..") != -1) {
      uploading = false;
      return;
    }
    uploadFile = SD.open(fullPath.c_str(), FILE_WRITE);
    if (!uploadFile) {
      uploading = false;
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    uploading = false;
  }
}

void handleApiDownload() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String path = server.arg("path");
  if (path.length() == 0) { sendJsonError(400, "Missing path"); return; }
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  if (!SD.exists(path)) {
    sendJsonError(404, "File not found");
    return;
  }

  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) {
    sendJsonError(500, "Cannot open file");
    return;
  }

  String contentType = getContentType(path);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + String(f.name()) + "\"");
  server.streamFile(f, contentType);
  f.close();
}

void handleApiDelete() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, body);
  if (err) { sendJsonError(400, "Invalid JSON"); return; }
  String path = doc["path"] | "";
  if (path.length() == 0) { sendJsonError(400, "Missing path"); return; }
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  if (!SD.exists(path)) { sendJsonError(404, "Path not found"); return; }

  bool success;
  File f = SD.open(path.c_str());
  if (f.isDirectory()) {
    f.close();
    success = deleteRecursive(path);
  } else {
    f.close();
    success = SD.remove(path.c_str());
  }

  if (success) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(500, "Delete failed");
  }
}

void handleApiRename() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, body);
  if (err) { sendJsonError(400, "Invalid JSON"); return; }
  String path = doc["path"] | "";
  String newName = doc["newName"] | "";
  if (path.length() == 0 || newName.length() == 0) { sendJsonError(400, "Missing parameters"); return; }
  if (path.indexOf("..") != -1 || newName.indexOf("/") != -1 || newName.indexOf("\\") != -1) {
    sendJsonError(400, "Invalid name");
    return;
  }

  int lastSlash = path.lastIndexOf('/');
  String parent = lastSlash == 0 ? "/" : path.substring(0, lastSlash);
  if (parent != "/" && !parent.endsWith("/")) parent += "/";
  String newPath = parent + newName;

  if (SD.exists(newPath)) {
    sendJsonError(409, "Target already exists");
    return;
  }

  if (SD.rename(path.c_str(), newPath.c_str())) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(500, "Rename failed");
  }
}

void handleApiMove() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, body);
  if (err) { sendJsonError(400, "Invalid JSON"); return; }
  String source = doc["source"] | "";
  String destination = doc["destination"] | "";
  if (source.length() == 0 || destination.length() == 0) { sendJsonError(400, "Missing parameters"); return; }
  if (source.indexOf("..") != -1 || destination.indexOf("..") != -1) {
    sendJsonError(400, "Invalid path");
    return;
  }

  if (!SD.exists(source)) { sendJsonError(404, "Source not found"); return; }
  if (SD.exists(destination)) { sendJsonError(409, "Destination already exists"); return; }

  String destParent = destination;
  int lastSlash = destParent.lastIndexOf('/');
  if (lastSlash > 0) {
    destParent = destParent.substring(0, lastSlash);
    if (!SD.exists(destParent)) {
      sendJsonError(404, "Destination directory not found");
      return;
    }
  }

  if (SD.rename(source.c_str(), destination.c_str())) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(500, "Move failed");
  }
}

void handleApiMkdir() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, body);
  if (err) { sendJsonError(400, "Invalid JSON"); return; }
  String path = doc["path"] | "";
  if (path.length() == 0) { sendJsonError(400, "Missing path"); return; }
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  if (SD.exists(path)) { sendJsonError(409, "Already exists"); return; }

  if (SD.mkdir(path.c_str())) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    sendJsonError(500, "Mkdir failed");
  }
}

void handleApiThumbnail() {
  if (checkBusy()) { sendJsonError(503, "SD busy"); return; }
  if (!sdAvailable) { sendJsonError(500, "SD not available"); return; }

  String path = server.arg("path");
  if (path.length() == 0) { sendJsonError(400, "Missing path"); return; }
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  String base = path;
  int dot = base.lastIndexOf('.');
  if (dot != -1) base = base.substring(0, dot);
  String candidates[4] = {base + ".jpg", base + ".jpeg", base + ".png", base + ".gif"};
  for (int i = 0; i < 4; i++) {
    if (SD.exists(candidates[i])) {
      File f = SD.open(candidates[i].c_str(), FILE_READ);
      if (f) {
        String ct = getContentType(candidates[i]);
        server.streamFile(f, ct);
        f.close();
        return;
      }
    }
  }

  if (SD.exists("/logo.jpg")) {
    File f = SD.open("/logo.jpg", FILE_READ);
    if (f) {
      server.streamFile(f, "image/jpeg");
      f.close();
      return;
    }
  }
  if (SD.exists("/logo.png")) {
    File f = SD.open("/logo.png", FILE_READ);
    if (f) {
      server.streamFile(f, "image/png");
      f.close();
      return;
    }
  }

  server.send(404, "application/json", "{\"error\":\"No thumbnail\"}");
}

void handleNotFound() {
  String uri = server.uri();
  if (sdAvailable && !checkBusy()) {
    String path = uri;
    path.replace("%20", " ");
    if (path.startsWith("/")) path = path.substring(1);
    if (path.length() > 0 && SD.exists(path.c_str())) {
      File f = SD.open(path.c_str(), FILE_READ);
      if (f) {
        String ct = getContentType(path);
        server.streamFile(f, ct);
        f.close();
        return;
      }
    }
  }
  server.send(404, "text/plain", "Not found");
}

// ==================== Вспомогательные функции ====================
String getContentType(String filename) {
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".txt")) return "text/plain";
  if (filename.endsWith(".gcode")) return "text/plain";
  if (filename.endsWith(".zip")) return "application/zip";
  return "application/octet-stream";
}

bool deleteRecursive(String path) {
  File dir = SD.open(path.c_str());
  if (!dir.isDirectory()) {
    dir.close();
    return SD.remove(path.c_str());
  }

  dir.rewindDirectory();
  File entry;
  while ((entry = dir.openNextFile())) {
    String entryName = entry.name();
    if (entryName == "." || entryName == "..") {
      entry.close();
      continue;
    }
    String fullPath = path;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += entryName;
    entry.close();
    if (!deleteRecursive(fullPath)) {
      dir.close();
      return false;
    }
  }
  dir.close();
  return SD.rmdir(path.c_str());
}

void sendJsonError(int code, String message) {
  DynamicJsonDocument doc(128);
  doc["error"] = message;
  String response;
  serializeJson(doc, response);
  server.send(code, "application/json", response);
}

// ==================== Управление светодиодом ====================
void updateLed() {
  unsigned long now = millis();
  unsigned long interval;
  bool shouldToggle = false;

  switch (currentLedState) {
    case LED_STATE_INIT:
      interval = 100;
      shouldToggle = (now - lastLedToggle >= interval);
      break;
    case LED_STATE_WIFI_OK:
      interval = 500;
      shouldToggle = (now - lastLedToggle >= interval);
      break;
    case LED_STATE_SD_FAIL:
      interval = 100;
      shouldToggle = (now - lastLedToggle >= interval);
      break;
    case LED_STATE_OK:
      digitalWrite(LED_PIN, LED_ON);
      return;
    case LED_STATE_UPLOADING:
      interval = 50;
      shouldToggle = (now - lastLedToggle >= interval);
      break;
    default:
      return;
  }

  if (shouldToggle) {
    lastLedToggle = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}

void setLedState(LedState state) {
  currentLedState = state;
}
