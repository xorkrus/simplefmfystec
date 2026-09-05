// main.cpp
#define HTTP_UPLOAD_BUFLEN 2048 // Устанавливаем буфер в 4096 байт (кратно 512 байт для SD)

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <SdFat.h>
#include <SPI.h>
#include "config.h"
#include "html.h"

// Буфер для накопления данных при загрузке (буферизуем и пишем "пакетами")
uint8_t uploadBuffer[HTTP_UPLOAD_BUFLEN];
size_t uploadBufferLen = 0;

// Глобальные объекты
AsyncWebServer server(HTTP_PORT);
SdFat sd;
FsFile uploadFile;
bool sdAvailable = false;
unsigned long lastLedToggle = 0;
int ledState = LED_ON;

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

// Прототипы функций
void initWiFi();
bool readSetupIni(String &ssid, String &password);
void startAP();
void initSD();
bool checkBusy();
String getContentType(String filename);
bool deleteRecursive(String path);
void sendJsonError(AsyncWebServerRequest *request, int code, String message);
void updateLed();
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

// ==================== Асинхронный класс для отдачи файлов с SD ====================
class AsyncSdFatResponse : public AsyncAbstractResponse {
private:
    FsFile _file;
public:
    AsyncSdFatResponse(const String& path, const String& contentType, bool download = false, const String& downloadName = "") {
        _code = 200;
        _contentType = contentType;
        _file = sd.open(path.c_str(), O_RDONLY);
        if (!_file) {
            _code = 404;
        } else {
            _contentLength = _file.size();
            if (download) {
                String name = downloadName.length() > 0 ? downloadName : path;
                int slash = name.lastIndexOf('/');
                if (slash >= 0) name = name.substring(slash + 1);
                addHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
            }
        }
    }
    ~AsyncSdFatResponse() {
        if (_file) _file.close();
    }
    bool _sourceValid() const { return !!_file; }
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
        if (!_file) return 0;
        return _file.read(buf, maxLen);
    }
};

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nStarting SD WiFi File Manager (Async)...");

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
  
  // Корневая страница
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (checkBusy()) { request->send(503, "text/plain", "SD card busy"); return; }
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // Список файлов
  server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request){
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    if (!sdAvailable) { sendJsonError(request, 500, "SD not available"); return; }

    String path = request->hasParam("path") ? request->getParam("path")->value() : "/";
    if (!path.startsWith("/")) path = "/" + path;
    if (path.indexOf("..") != -1) { sendJsonError(request, 400, "Invalid path"); return; }

    FsFile dir = sd.open(path.c_str(), O_RDONLY);
    if (!dir || !dir.isDirectory()) {
      sendJsonError(request, 404, "Directory not found");
      if (dir) dir.close();
      return;
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
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
    serializeJson(doc, *response);
    request->send(response);
  });

  // Загрузка файла (Data handler & Response handler)
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"success\":true}");
  }, handleFileUpload);

  // Скачивание
  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    if (!sdAvailable) { sendJsonError(request, 500, "SD not available"); return; }

    if (!request->hasParam("path")) { sendJsonError(request, 400, "Missing path"); return; }
    String path = request->getParam("path")->value();
    if (path.indexOf("..") != -1) { sendJsonError(request, 400, "Invalid path"); return; }

    AsyncSdFatResponse *response = new AsyncSdFatResponse(path, getContentType(path), true);
    request->send(response);
  });

  // JSON API обработчики (Удаление, Переименование, Перемещение, Создание папки)
  server.addHandler(new AsyncCallbackJsonWebHandler("/api/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    JsonObject doc = json.as<JsonObject>();
    String path = doc["path"] | "";
    if (path.length() == 0 || path.indexOf("..") != -1 || !sd.exists(path.c_str())) { sendJsonError(request, 400, "Invalid or missing path"); return; }
    
    bool success;
    FsFile f = sd.open(path.c_str(), O_RDONLY);
    if (f.isDirectory()) {
      f.close();
      success = deleteRecursive(path);
    } else {
      f.close();
      success = sd.remove(path.c_str());
    }
    if (success) request->send(200, "application/json", "{\"success\":true}");
    else sendJsonError(request, 500, "Delete failed");
  }));

  server.addHandler(new AsyncCallbackJsonWebHandler("/api/rename", [](AsyncWebServerRequest *request, JsonVariant &json) {
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    JsonObject doc = json.as<JsonObject>();
    String path = doc["path"] | "";
    String newName = doc["newName"] | "";
    if (path.length() == 0 || newName.length() == 0 || path.indexOf("..") != -1) { sendJsonError(request, 400, "Invalid params"); return; }
    
    int lastSlash = path.lastIndexOf('/');
    String parent = lastSlash == 0 ? "/" : path.substring(0, lastSlash);
    if (parent != "/" && !parent.endsWith("/")) parent += "/";
    String newPath = parent + newName;
    
    if (sd.exists(newPath.c_str())) { sendJsonError(request, 409, "Target exists"); return; }
    if (sd.rename(path.c_str(), newPath.c_str())) request->send(200, "application/json", "{\"success\":true}");
    else sendJsonError(request, 500, "Rename failed");
  }));

  server.addHandler(new AsyncCallbackJsonWebHandler("/api/move", [](AsyncWebServerRequest *request, JsonVariant &json) {
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    JsonObject doc = json.as<JsonObject>();
    String source = doc["source"] | "";
    String dest = doc["destination"] | "";
    if (source.length() == 0 || dest.length() == 0 || source.indexOf("..") != -1) { sendJsonError(request, 400, "Invalid params"); return; }
    
    if (sd.rename(source.c_str(), dest.c_str())) request->send(200, "application/json", "{\"success\":true}");
    else sendJsonError(request, 500, "Move failed");
  }));

  server.addHandler(new AsyncCallbackJsonWebHandler("/api/mkdir", [](AsyncWebServerRequest *request, JsonVariant &json) {
    if (checkBusy()) { sendJsonError(request, 503, "SD busy"); return; }
    JsonObject doc = json.as<JsonObject>();
    String path = doc["path"] | "";
    if (path.length() == 0 || path.indexOf("..") != -1) { sendJsonError(request, 400, "Invalid params"); return; }
    
    if (sd.mkdir(path.c_str())) request->send(200, "application/json", "{\"success\":true}");
    else sendJsonError(request, 500, "Mkdir failed");
  }));

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Async HTTP server started");
  
  if (WiFi.getMode() == WIFI_STA) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    currentLedState = LED_STATE_WIFI_OK;
  } else {
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    currentLedState = LED_STATE_WIFI_OK;
  }
}

void loop() {
  updateLed();
  // server.handleClient() больше не нужен!
}

// ==================== Обработчик загрузки файла ====================
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (checkBusy() || !sdAvailable) {
    return; // Игнорируем чанки, если карта недоступна или занята
  }

  // Начало загрузки
  if (!index) {
    uploading = true;
    currentLedState = LED_STATE_UPLOADING;
    String path = request->hasParam("path", true) ? request->getParam("path", true)->value() : 
                 (request->hasParam("path") ? request->getParam("path")->value() : "/");
    
    if (!path.endsWith("/")) path += "/";
    int slash = filename.lastIndexOf('/');
    if (slash != -1) filename = filename.substring(slash + 1);
    int backslash = filename.lastIndexOf('\\');
    if (backslash != -1) filename = filename.substring(backslash + 1);
    if (filename.length() == 0) filename = "unnamed";
    
    String fullPath = path + filename;
    uploadFile = sd.open(fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    uploadBufferLen = 0;
  } 
  
  // Запись порций данных в буфер и на карту
  if (uploading && uploadFile && len > 0) {
    size_t srcOffset = 0;
    while (len > 0) {
      size_t space = HTTP_UPLOAD_BUFLEN - uploadBufferLen;
      size_t copyLen = min(space, len);
      
      memcpy(uploadBuffer + uploadBufferLen, data + srcOffset, copyLen);
      uploadBufferLen += copyLen;
      srcOffset += copyLen;
      len -= copyLen;

      // Сбрасываем "пакет" (4096 байт) на SD-карту. Это 8 секторов по 512 байт — оптимально для FAT32.
      if (uploadBufferLen == HTTP_UPLOAD_BUFLEN) {
        uploadFile.write(uploadBuffer, uploadBufferLen);
        uploadBufferLen = 0;
      }
    }
  } 
  
  // Конец загрузки
  if (final) {
    if (uploading && uploadFile) {
      if (uploadBufferLen > 0) {
        uploadFile.write(uploadBuffer, uploadBufferLen); // Дописываем остаток
        uploadBufferLen = 0;
      }
      uploadFile.close();
    }
    uploading = false;
    currentLedState = LED_STATE_WIFI_OK;
  }
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
      delay(500); Serial.print("."); attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true; Serial.println("\nConnected to WiFi!");
    } else {
      Serial.println("\nFailed to connect with SETUP.INI"); WiFi.disconnect();
    }
  }

  if (!connected) {
    Serial.printf("Trying fallback WiFi: %s\n", FALLBACK_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(FALLBACK_SSID, FALLBACK_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500); Serial.print("."); attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true; Serial.println("\nConnected to fallback WiFi!");
    } else {
      Serial.println("\nFailed to connect to fallback, starting AP..."); WiFi.disconnect();
    }
  }

  if (!connected) startAP();
}

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("AP started: %s / %s\n", AP_SSID, AP_PASSWORD);
}

bool readSetupIni(String &ssid, String &password) {
  if (!sdAvailable) return false;
  FsFile f = sd.open(SETUP_INI_FILENAME, O_RDONLY);
  if (!f) return false;

  String content;
  while (f.available()) content += (char)f.read();
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

  return (ssid.length() > 0 && password.length() > 0);
}

// ==================== Инициализация SD ====================
void initSD() {
  Serial.println("Initializing SD card...");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin();
  
  // Устанавливаем 8 МГц по вашему запросу. Это повысит надежность при помехах от шаговых двигателей.
  if (sd.begin(SD_CS, SD_SCK_MHZ(8))) { 
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

// ==================== Вспомогательные функции ====================
String getContentType(String filename) {
  if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".json")) return "application/json";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".gcode")) return "text/plain";
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
    if (!entry.getName(nameBuf, sizeof(nameBuf))) { entry.close(); continue; }
    String entryName = String(nameBuf);
    if (entryName == "." || entryName == "..") { entry.close(); continue; }
    
    String fullPath = path;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += entryName;
    entry.close();
    
    if (!deleteRecursive(fullPath)) { dir.close(); return false; }
  }
  dir.close();
  return sd.rmdir(path.c_str());
}

void sendJsonError(AsyncWebServerRequest *request, int code, String message) {
  DynamicJsonDocument doc(128);
  doc["error"] = message;
  String response;
  serializeJson(doc, response);
  request->send(code, "application/json", response);
}

// ==================== Управление светодиодом ====================
void updateLed() {
  unsigned long now = millis();
  unsigned long interval;
  bool shouldToggle = false;

  switch (currentLedState) {
    case LED_STATE_INIT: interval = 100; break;
    case LED_STATE_WIFI_OK: interval = 500; break;
    case LED_STATE_SD_FAIL: interval = 100; break;
    case LED_STATE_OK: digitalWrite(LED_PIN, LED_ON); return;
    case LED_STATE_UPLOADING: interval = 50; break;
    default: return;
  }
  
  shouldToggle = (now - lastLedToggle >= interval);
  if (shouldToggle) {
    lastLedToggle = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
