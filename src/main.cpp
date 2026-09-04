// main.cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SdFat.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "config.h"
#include "html.h"

// Глобальные объекты
ESP8266WebServer server(HTTP_PORT);
SdFat sd;
FsFile uploadFile;
bool sdAvailable = false;
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
String getFileName(FsFile &f);
void streamFsFile(FsFile &f, const String &contentType, const String &downloadName = "");

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

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);
  pinMode(CS_SENSE, INPUT);
  Serial.print("CS_SENSE initial state: ");
  Serial.println(digitalRead(CS_SENSE) ? "HIGH" : "LOW");

  initSD();
  if (!sdAvailable) {
    currentLedState = LED_STATE_SD_FAIL;
  }

  initWiFi();
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
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

bool readSetupIni(String &ssid, String &password) {
  if (!sdAvailable) return false;
  FsFile f = sd.open(SETUP_INI_FILENAME, O_RDONLY);
  if (!f) {
    Serial.println("SETUP.INI not found");
    return false;
  }

  String content;
  while (f.available()) {
    content += (char)f.read();
  }
  f.close();

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
  Serial.println("Initializing SD card...");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin();
  //SPI.setFrequency(4000000);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  bool initOk = false;
  const uint32_t speeds[] = {SD_SCK_MHZ(16), SD_SCK_MHZ(8), SD_SCK_MHZ(4), SD_SCK_MHZ(2), SD_SCK_MHZ(1)};
  for (int i = 0; i < 5 && !initOk; i++) {
    Serial.printf("Attempt with %d MHz: ", 4 >> i);
    if (sd.begin(SD_CS, speeds[i])) {
      initOk = true;
      Serial.println("OK");
      break;
    } else {
      Serial.println("failed");
      delay(200);
      digitalWrite(SD_CS, HIGH);
      delay(100);
    }
  }

  if (initOk) {
    sdAvailable = true;
    Serial.println("SD card initialized successfully.");
  } else {
    sdAvailable = false;
    Serial.println("SD card initialization FAILED.");
  }
}

bool checkBusy() {
  if (digitalRead(CS_SENSE) == LOW) {
    Serial.println("SD bus busy (Marlin active)");
    return true;
  }
  return false;
}

// ==================== Вспомогательная отправка файла ====================
String getFileName(FsFile &f) {
  char name[256];
  if (f.getName(name, sizeof(name))) {
    return String(name);
  }
  return "";
}

void streamFsFile(FsFile &f, const String &contentType, const String &downloadName) {
  String name = downloadName.length() > 0 ? downloadName : getFileName(f);
  server.sendHeader("Content-Type", contentType);
  server.sendHeader("Content-Length", String(f.size()));
  if (downloadName.length() > 0) {
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  }
  server.send(200, contentType, ""); // отправляет пустое тело? Лучше использовать client
  // Более надёжный способ — отправка через client
  WiFiClient client = server.client();
  const size_t bufSize = 8192;
  uint8_t buf[bufSize];
  size_t n;
  while ((n = f.read(buf, bufSize)) > 0) {
    client.write(buf, n);
  }
  f.close();
}

// ==================== Обработчики веб-сервера ====================
void handleRoot() {
  if (checkBusy()) {
    server.send(503, "text/plain", "SD card busy");
    return;
  }

  String userAgent = server.header("User-Agent");
  bool isMobile = userAgent.indexOf("Mobile") != -1 || userAgent.indexOf("Android") != -1;

  String indexFile = isMobile ? "/index_m.html" : "/index.html";
  if (sdAvailable && sd.exists(indexFile.c_str())) {
    FsFile f = sd.open(indexFile, O_RDONLY);
    if (f) {
      streamFsFile(f, "text/html");
      return;
    }
  }

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
  if (path.indexOf("..") != -1) { sendJsonError(400, "Invalid path"); return; }

  FsFile dir = sd.open(path.c_str(), O_RDONLY);
  if (!dir || !dir.isDirectory()) {
    sendJsonError(404, "Directory not found");
    if (dir) dir.close();
    return;
  }

  DynamicJsonDocument doc(8192);
  doc["path"] = path;
  JsonArray files = doc.createNestedArray("files");

  dir.rewindDirectory();
  FsFile entry;
  char nameBuf[256];
  while ((entry = dir.openNextFile())) {
    if (!entry.getName(nameBuf, sizeof(nameBuf))) continue;
    if (strlen(nameBuf) == 0) continue;

    JsonObject fileObj = files.createNestedObject();
    fileObj["name"] = nameBuf;
    fileObj["size"] = entry.size();
    fileObj["isDir"] = entry.isDirectory();
    String fullPath = path;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += nameBuf;
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
    uploadFile = sd.open(fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
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

  if (!sd.exists(path.c_str())) {
    sendJsonError(404, "File not found");
    return;
  }

  FsFile f = sd.open(path.c_str(), O_RDONLY);
  if (!f) {
    sendJsonError(500, "Cannot open file");
    return;
  }

  String contentType = getContentType(path);
  streamFsFile(f, contentType, getFileName(f));
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

  if (!sd.exists(path.c_str())) { sendJsonError(404, "Path not found"); return; }

  bool success;
  FsFile f = sd.open(path.c_str(), O_RDONLY);
  if (f.isDirectory()) {
    f.close();
    success = deleteRecursive(path);
  } else {
    f.close();
    success = sd.remove(path.c_str());
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

  if (sd.exists(newPath.c_str())) {
    sendJsonError(409, "Target already exists");
    return;
  }

  if (sd.rename(path.c_str(), newPath.c_str())) {
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

  if (!sd.exists(source.c_str())) { sendJsonError(404, "Source not found"); return; }
  if (sd.exists(destination.c_str())) { sendJsonError(409, "Destination already exists"); return; }

  String destParent = destination;
  int lastSlash = destParent.lastIndexOf('/');
  if (lastSlash > 0) {
    destParent = destParent.substring(0, lastSlash);
    if (!sd.exists(destParent.c_str())) {
      sendJsonError(404, "Destination directory not found");
      return;
    }
  }

  if (sd.rename(source.c_str(), destination.c_str())) {
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

  if (sd.exists(path.c_str())) { sendJsonError(409, "Already exists"); return; }

  if (sd.mkdir(path.c_str())) {
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
    if (sd.exists(candidates[i].c_str())) {
      FsFile f = sd.open(candidates[i].c_str(), O_RDONLY);
      if (f) {
        String ct = getContentType(candidates[i]);
        streamFsFile(f, ct);
        return;
      }
    }
  }

  if (sd.exists("/logo.jpg")) {
    FsFile f = sd.open("/logo.jpg", O_RDONLY);
    if (f) {
      streamFsFile(f, "image/jpeg");
      return;
    }
  }
  if (sd.exists("/logo.png")) {
    FsFile f = sd.open("/logo.png", O_RDONLY);
    if (f) {
      streamFsFile(f, "image/png");
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
    if (path.length() > 0 && sd.exists(path.c_str())) {
      FsFile f = sd.open(path.c_str(), O_RDONLY);
      if (f) {
        String ct = getContentType(path);
        streamFsFile(f, ct);
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
  FsFile dir = sd.open(path.c_str(), O_RDONLY);
  if (!dir.isDirectory()) {
    dir.close();
    return sd.remove(path.c_str());
  }

  dir.rewindDirectory();
  FsFile entry;
  char nameBuf[256];
  while ((entry = dir.openNextFile())) {
    if (!entry.getName(nameBuf, sizeof(nameBuf))) {
      entry.close();
      continue;
    }
    String entryName = String(nameBuf);
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
  return sd.rmdir(path.c_str());
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
