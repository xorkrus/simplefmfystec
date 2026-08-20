#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>

// ============================================================================
// Настройки сети Wi-Fi
// ============================================================================
const char* ssid     = "YOUR_SSID";     // Замените на имя вашей сети
const char* password = "YOUR_PASSWORD"; // Замените на пароль

// ============================================================================
// Настройки распиновки FYSETC SD-WiFi
// ============================================================================
#define SD_CS_PIN    13
#define SD_SCK_PIN   14
#define SD_MISO_PIN  2
#define SD_MOSI_PIN  15
#define SD_MUX_PIN   12 

// Создаем объекты SPI и Веб-сервера на порту 80
SPIClass sdSPI(HSPI);
AsyncWebServer server(80);

// Переменная статуса SD
bool sdInitialized = false;

// HTML-шаблон веб-интерфейса
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>FYSETC SD-WiFi Manager</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f9; color: #333; }
    h2 { color: #0056b3; }
    .card { background: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; }
    ul { list-style-type: none; padding: 0; }
    li { padding: 8px 0; border-bottom: 1px solid #ddd; display: flex; justify-content: space-between; align-items: center; }
    a { color: #007bff; text-decoration: none; }
    a:hover { text-decoration: underline; }
    .btn { background: #28a745; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }
    .btn-danger { background: #dc3545; padding: 4px 8px; font-size: 12px; }
    input[type=file] { margin-bottom: 10px; }
  </style>
</head>
<body>
  <h2>FYSETC SD-WiFi Control Panel</h2>
  
  <div class="card">
    <h3>Загрузить файл на SD-карту</h3>
    <form method="POST" action="/upload" enctype="multipart/form-data">
      <input type="file" name="upload" required><br>
      <input type="submit" value="Загрузить" class="btn">
    </form>
  </div>

  <div class="card">
    <h3>Файлы на SD-карте</h3>
    <div id="file-list">Загрузка списка файлов...</div>
  </div>

<script>
function loadFiles() {
  fetch('/list')
    .then(response => response.json())
    .then(data => {
      let html = '<ul>';
      if(data.length === 0) {
        html += '<li>Файлы отсутствуют</li>';
      } else {
        data.forEach(file => {
          html += `<li>
            <span><a href="/download?file=${encodeURIComponent(file.name)}">${file.name}</a> (${(file.size/1024).toFixed(1)} KB)</span>
            <button class="btn btn-danger" onclick="deleteFile('${file.name}')">Удалить</button>
          </li>`;
        });
      }
      html += '</ul>';
      document.getElementById('file-list').innerHTML = html;
    });
}

function deleteFile(filename) {
  if(confirm('Удалить файл ' + filename + '?')) {
    fetch('/delete?file=' + encodeURIComponent(filename), { method: 'DELETE' })
      .then(() => loadFiles());
  }
}

loadFiles();
</script>
</body>
</html>
)rawliteral";

// Инициализация SD-карты
bool initSDCard() {
    pinMode(SD_MUX_PIN, OUTPUT);
    digitalWrite(SD_MUX_PIN, LOW); 
    delay(100);

    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, sdSPI, 20000000)) {
        digitalWrite(SD_MUX_PIN, HIGH);
        delay(100);
        if (!SD.begin(SD_CS_PIN, sdSPI, 20000000)) {
            Serial.println("[SD] Ошибка инициализации!");
            return false;
        }
    }
    Serial.println("[SD] Карточка успешно инициализирована.");
    return true;
}

// Настройка эндпоинтов веб-сервера
void setupWebServer() {
    // Главная страница
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    // Список файлов в JSON
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        if (sdInitialized) {
            File root = SD.open("/");
            File file = root.openNextFile();
            bool first = true;
            while (file) {
                if (!file.isDirectory()) {
                    if (!first) json += ",";
                    json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
                    first = false;
                }
                file = root.openNextFile();
            }
            root.close();
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // Скачивание файла
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("file")) {
            String filename = "/" + request->getParam("file")->value();
            if (SD.exists(filename)) {
                request->send(SD, filename, "application/octet-stream");
                return;
            }
        }
        request->send(404, "text/plain", "Файл не найден");
    });

    // Удаление файла
    server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (request->hasParam("file")) {
            String filename = "/" + request->getParam("file")->value();
            if (SD.exists(filename)) {
                SD.remove(filename);
                request->send(200, "text/plain", "Удалено");
                return;
            }
        }
        request->send(400, "text/plain", "Ошибка удаления");
    });

    // Загрузка файлов на SD через POST multipart/form-data
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<h3>Файл загружен!</h3><a href='/'>Назад</a>");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.printf("[Upload] Старт загрузки: %s\n", filename.c_str());
            request->_tempFile = SD.open(filename, FILE_WRITE);
        }
        if (request->_tempFile) {
            request->_tempFile.write(data, len);
        }
        if (final) {
            if (request->_tempFile) {
                request->_tempFile.close();
                Serial.println("[Upload] Загрузка завершена!");
            }
        }
    });

    server.begin();
    Serial.println("[Web] HTTP Сервер запущен.");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Подключение к Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Подключение");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Подключено!");
    Serial.print("[WiFi] IP-адрес для входа в браузер: http://");
    Serial.println(WiFi.localIP());

    // Инициализация SD
    sdInitialized = initSDCard();

    // Запуск сервера
    setupWebServer();
}

void loop() {
    // AsyncWebServer работает в фоновом режиме на обработчиках ESP32
    delay(1000);
}
