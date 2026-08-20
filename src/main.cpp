#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

// Распиновка FYSETC SD WIFI
#define SD_CS     4
#define MISO_PIN  12
#define MOSI_PIN  13
#define SCLK_PIN  14
#define CS_SENSE  5
#define LED_PIN   2

// Дефолтные настройки
#define DEFAULT_SSID      "xopkland"
#define DEFAULT_PASSWORD  "1234567890987654321"

// Порт сервера
#define HTTP_PORT 80

ESP8266WebServer server(HTTP_PORT);
File fsUploadFile;

String currentSSID = DEFAULT_SSID;
String currentPASS = DEFAULT_PASSWORD;

// Fallback HTML/JS Интерфейс (если нет index.html на карте)
const char fallback_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>SD WIFI 3D Printer</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f4f4f9; }
        .container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        .dropzone { border: 2px dashed #ccc; padding: 20px; text-align: center; margin-bottom: 20px; cursor: pointer; }
        .dropzone.dragover { background: #e1f5fe; border-color: #03a9f4; }
        progress { width: 100%; height: 20px; display: none; margin-top: 10px; }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 10px; border-bottom: 1px solid #ddd; text-align: left; }
        .thumb { width: 40px; height: 40px; object-fit: cover; border-radius: 4px; }
        .btn { padding: 5px 10px; cursor: pointer; margin-right: 5px; }
        .controls { margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h2>Файловый менеджер SD WIFI</h2>
        
        <div class="controls">
            <button class="btn" onclick="mkdir()">Создать папку</button>
            <span id="currentPath" style="font-weight:bold; margin-left:10px;">/</span>
        </div>

        <div class="dropzone" id="dropzone" onclick="document.getElementById('fileInput').click()">
            Перетащите файлы сюда или нажмите для выбора
            <input type="file" id="fileInput" style="display:none" multiple onchange="handleFiles(this.files)">
            <progress id="progressBar" max="100" value="0"></progress>
        </div>

        <table>
            <thead>
                <tr>
                    <th>Превью</th>
                    <th>Имя файла</th>
                    <th>Размер</th>
                    <th>Действия</th>
                </tr>
            </thead>
            <tbody id="fileList"></tbody>
        </table>
    </div>

    <script>
        let currentDir = "/";

        function loadFiles() {
            fetch(`/api/list?dir=${currentDir}`)
                .then(res => res.json())
                .then(data => {
                    const tbody = document.getElementById('fileList');
                    tbody.innerHTML = '';
                    
                    if (currentDir !== "/") {
                        const upDir = currentDir.substring(0, currentDir.lastIndexOf('/')) || "/";
                        tbody.innerHTML += `<tr><td>📁</td><td><a href="#" onclick="changeDir('${upDir}')">..</a></td><td>-</td><td></td></tr>`;
                    }

                    data.forEach(file => {
                        const tr = document.createElement('tr');
                        const thumbHtml = !file.isDir && file.name.endsWith('.gcode') 
                            ? `<img class="thumb" src="/api/thumb?file=${currentDir === '/' ? '' : currentDir}/${file.name}" alt="thumb">`
                            : (file.isDir ? '📁' : '📄');
                        
                        const actions = file.isDir 
                            ? `<button class="btn" onclick="del('${file.name}', true)">Удалить</button>`
                            : `<a class="btn" href="${currentDir === '/' ? '' : currentDir}/${file.name}" download>Скачать</a>
                               <button class="btn" onclick="rename('${file.name}')">Переименовать</button>
                               <button class="btn" onclick="move('${file.name}')">Переместить</button>
                               <button class="btn" onclick="del('${file.name}', false)">Удалить</button>`;

                        const nameHtml = file.isDir 
                            ? `<a href="#" onclick="changeDir('${currentDir === '/' ? '' : currentDir}/${file.name}')">${file.name}</a>`
                            : file.name;

                        tr.innerHTML = `<td>${thumbHtml}</td><td>${nameHtml}</td><td>${file.size || '-'}</td><td>${actions}</td>`;
                        tbody.appendChild(tr);
                    });
                });
        }

        function changeDir(dir) {
            currentDir = dir;
            document.getElementById('currentPath').innerText = currentDir;
            loadFiles();
        }

        function handleFiles(files) {
            if (!files.length) return;
            const file = files[0];
            const formData = new FormData();
            formData.append("file", file, (currentDir === '/' ? '' : currentDir) + '/' + file.name);

            const xhr = new XMLHttpRequest();
            const pb = document.getElementById('progressBar');
            
            xhr.upload.addEventListener('progress', e => {
                if (e.lengthComputable) {
                    pb.style.display = 'block';
                    pb.value = (e.loaded / e.total) * 100;
                }
            });

            xhr.addEventListener('load', () => {
                pb.style.display = 'none';
                pb.value = 0;
                loadFiles();
            });

            xhr.open('POST', '/api/upload', true);
            xhr.send(formData);
        }

        const dropzone = document.getElementById('dropzone');
        dropzone.addEventListener('dragover', e => { e.preventDefault(); dropzone.classList.add('dragover'); });
        dropzone.addEventListener('dragleave', () => dropzone.classList.remove('dragover'));
        dropzone.addEventListener('drop', e => {
            e.preventDefault();
            dropzone.classList.remove('dragover');
            handleFiles(e.dataTransfer.files);
        });

        function del(name, isDir) {
            if (!confirm(`Удалить ${name}?`)) return;
            const path = (currentDir === '/' ? '' : currentDir) + '/' + name;
            fetch(`/api/delete?path=${path}&isDir=${isDir}`, { method: 'POST' }).then(loadFiles);
        }

        function rename(oldName) {
            const newName = prompt("Новое имя файла:", oldName);
            if (!newName) return;
            const oldPath = (currentDir === '/' ? '' : currentDir) + '/' + oldName;
            const newPath = (currentDir === '/' ? '' : currentDir) + '/' + newName;
            fetch(`/api/rename?old=${oldPath}&new=${newPath}`, { method: 'POST' }).then(loadFiles);
        }

        function move(name) {
            const newPath = prompt("Новый путь (например, /folder/file.gcode):", "/" + name);
            if (!newPath) return;
            const oldPath = (currentDir === '/' ? '' : currentDir) + '/' + name;
            fetch(`/api/rename?old=${oldPath}&new=${newPath}`, { method: 'POST' }).then(loadFiles);
        }

        function mkdir() {
            const name = prompt("Имя новой папки:");
            if (!name) return;
            const path = (currentDir === '/' ? '' : currentDir) + '/' + name;
            fetch(`/api/mkdir?path=${path}`, { method: 'POST' }).then(loadFiles);
        }

        window.onload = loadFiles;
    </script>
</body>
</html>
)rawliteral";

void loadConfig() {
    if (SD.exists("/SETUP.INI")) {
        File f = SD.open("/SETUP.INI", FILE_READ);
        bool inWifiSection = false;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line == "[WIFI]") {
                inWifiSection = true;
            } else if (inWifiSection && line.startsWith("SSID=")) {
                currentSSID = line.substring(5);
            } else if (inWifiSection && line.startsWith("PASSWORD=")) {
                currentPASS = line.substring(9);
            } else if (line.startsWith("[")) {
                inWifiSection = false;
            }
        }
        f.close();
        Serial.println("Config loaded.");
    }
}

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    return "text/plain";
}

bool serveFile(String path) {
    if (!SD.exists(path)) return false;
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) return false;
    server.streamFile(file, getContentType(path));
    file.close();
    return true;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // Выключаем светодиод

    // Пин для проверки, не использует ли сейчас принтер SD-карту
    pinMode(CS_SENSE, INPUT_PULLUP);

    // Подготовка пина выбора чипа SD
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    
    // Инициализация стандартного аппаратного SPI (пины 12, 13, 14 используются автоматически)
    SPI.begin();
    delay(100); // Даем карте время на "пробуждение"

    // Проверяем, не занята ли шина принтером (CS_SENSE)
    if (digitalRead(CS_SENSE) == LOW) {
        Serial.println("Warning: Marlin is currently using the SD card!");
    }

    // Инициализация SD
    bool sd_mounted = SD.begin(SD_CS);
    if (!sd_mounted) {
        Serial.println("SD Card Mount Failed! Check FAT32 format and printer state.");
        // Не делаем return; идем дальше, чтобы хотя бы поднять Wi-Fi
    } else {
        Serial.println("SD Card Mounted successfully!");
        loadConfig();
    }

    // Попытка STA (подключение к роутеру)
    WiFi.mode(WIFI_STA);
    WiFi.begin(currentSSID.c_str(), currentPASS.c_str());
    Serial.print("Connecting to WiFi");
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
    } else {
        // Fallback AP (Режим точки доступа)
        Serial.println("\nWiFi failed. Starting AP.");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("sd-card-3dp", "12345678");
        Serial.println("AP IP: " + WiFi.softAPIP().toString());
    }

    digitalWrite(LED_PIN, LOW); // Включаем светодиод (устройство готово)

    // Если SD не смонтировалась, на все запросы отдаем ошибку 
    if (!sd_mounted) {
        server.onNotFound([]() {
            server.send(500, "text/plain", "SD Card Mount Failed. Format to FAT32 or turn off printer access.");
        });
        server.begin();
        return;
    }

    // --- МАРШРУТЫ (остаются те же самые) ---
    server.on("/", HTTP_GET, []() {
        if (!serveFile("/index.html")) {
            server.send_P(200, "text/html", fallback_html);
        }
    });

    server.on("/api/list", HTTP_GET, []() {
        String dirPath = server.hasArg("dir") ? server.arg("dir") : "/";
        File dir = SD.open(dirPath);
        DynamicJsonDocument doc(2048);
        JsonArray array = doc.to<JsonArray>();

        while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            JsonObject obj = array.createNestedObject();
            obj["name"] = String(entry.name());
            obj["isDir"] = entry.isDirectory();
            if (!entry.isDirectory()) obj["size"] = entry.size();
            entry.close();
        }
        dir.close();
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "OK");
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            if (SD.exists(filename)) SD.remove(filename);
            fsUploadFile = SD.open(filename, FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (fsUploadFile) fsUploadFile.close();
        }
    });

    server.on("/api/delete", HTTP_POST, []() {
        String path = server.arg("path");
        bool isDir = server.arg("isDir") == "true";
        bool res = isDir ? SD.rmdir(path) : SD.remove(path);
        server.send(res ? 200 : 500, "text/plain", res ? "OK" : "Error");
    });

    server.on("/api/rename", HTTP_POST, []() {
        String oldPath = server.arg("old");
        String newPath = server.arg("new");
        bool res = SD.rename(oldPath, newPath);
        server.send(res ? 200 : 500, "text/plain", res ? "OK" : "Error");
    });

    server.on("/api/mkdir", HTTP_POST, []() {
        String path = server.arg("path");
        bool res = SD.mkdir(path);
        server.send(res ? 200 : 500, "text/plain", res ? "OK" : "Error");
    });

    server.on("/api/thumb", HTTP_GET, []() {
        String file = server.arg("file"); 
        int dot = file.lastIndexOf('.');
        String base = (dot > 0) ? file.substring(0, dot) : file;
        
        String jpgPath = base + ".jpg";
        String pngPath = base + ".png";
        
        if (serveFile(jpgPath)) return;
        if (serveFile(pngPath)) return;
        if (serveFile("/logo.jpg")) return;
        if (serveFile("/logo.png")) return;

        String svg = "<svg xmlns='http://www.w3.org/2000/svg' width='300' height='300'><rect width='300' height='300' fill='#ddd'/><text x='50%' y='50%' dominant-baseline='middle' text-anchor='middle' font-family='sans-serif' font-size='20'>No preview</text></svg>";
        server.send(200, "image/svg+xml", svg);
    });

    server.onNotFound([]() {
        if (!serveFile(server.uri())) {
            server.send(404, "text/plain", "Not Found");
        }
    });

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}
