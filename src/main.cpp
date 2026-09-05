// main.cpp
#define HTTP_UPLOAD_BUFLEN 8192
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SdFat.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "config.h"
#include "html.h"
// Буфер для накопления данных при загрузке
uint8_t uploadBuffer[16384];
size_t uploadBufferLen = 0;

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
void handleApiTestRead();
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
  server.on("/api/testread", HTTP_GET, handleApiTestRead);
  server.onNotFound(handleNotFound);

  server.keepAlive(false);
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
  yield();
  updateLed();
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  delay(1000);
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
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);

  bool initOk = false;
  const uint32_t speeds[] = {SD_SCK_MHZ(16)};
  for (int i = 0; i < 1 && !initOk; i++) {
    Serial.printf("Attempt with 16 MHz: ");
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
    WiFiClient client = server.client();
    if (!client || !f) return;

    String headers = "HTTP/1.1 200 OK\r\n";
    headers += "Content-Type: " + contentType + "\r\n";
    headers += "Content-Length: " + String(f.size()) + "\r\n";
    if (downloadName.length() > 0) {
        headers += "Content-Disposition: attachment; filename=\"" + name + "\"\r\n";
    }
    headers += "Connection: close\r\n";
    headers += "\r\n";

    client.write(headers.c_str(), headers.length());

    const size_t bufSize = 8192;
    uint8_t buf[bufSize];
    size_t n;
    while ((n = f.read(buf, bufSize)) > 0) {
        client.write(buf, n);
    }
    f.close();
    client.stop();
}

// ==================== Обработчики веб-сервера ====================
/*
void handleRoot() {
  if (checkBusy()) {
    server.send(503, "text/plain", "SD card busy");
    return;
  }

  server.send_P(200, "text/html", INDEX_HTML);
}
*/
void handleRoot() {
    if (checkBusy()) { server.send(503, "text/plain", "SD busy"); return; }
    server.sendHeader("Content-Type", "text/html");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "");  // отправляем заголовки с пустым телом

    const char* ptr = INDEX_HTML;
    size_t len = strlen_P(ptr);
    size_t chunkSize = 1024;
    while (len > 0) {
        size_t toSend = min(len, chunkSize);
        char buf[chunkSize];
        memcpy_P(buf, ptr, toSend);
        server.sendContent(buf, toSend);
        ptr += toSend;
        len -= toSend;
        yield();  // даём стеку обработать сетевые события
    }
    server.sendContent("");
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

  DynamicJsonDocument doc(4096);
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

void handleApiTestRead() {
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

  uint32_t start = micros();
  const size_t bufSize = 512;
  uint8_t buf[bufSize];
  size_t total = 0;
  while (f.read(buf, bufSize) > 0) {
    total += bufSize;
  }
  uint32_t elapsed = micros() - start;
  f.close();

  DynamicJsonDocument doc(256);
  doc["path"] = path;
  doc["size"] = f.size();
  doc["time_us"] = elapsed;
  doc["time_ms"] = elapsed / 1000.0;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  // Убрано, чтобы не раздавать файлы напрямую
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
