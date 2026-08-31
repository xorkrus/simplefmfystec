#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>

// Пины
#define SD_CS      4
#define CS_SENSE   5
#define LED        2

// Fallback WiFi
#define FALLBACK_SSID     "xopkland"
#define FALLBACK_PASSWORD "1234567890987654321"

// AP параметры
#define AP_SSID     "sd-card-3dp"
#define AP_PASSWORD "12345678"

ESP8266WebServer server(80);

const uint8_t grayPixelPng[] PROGMEM = {
  0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
  0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x62, 0x00, 0x01, 0x00, 0x00,
  0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

bool sd_ok = false;
String wifi_ssid = "";
String wifi_pass = "";

// Встроенные веб-страницы (fallback)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SD Card Manager</title>
<style>
  :root { --bg: #f5f5f5; --panel: #fff; --text: #333; --accent: #4a90d9; --hover: #e9f0fa; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); height: 100vh; display: flex; }
  .container { display: flex; width: 100%; height: 100vh; }
  .file-manager { flex: 1; padding: 20px; display: flex; flex-direction: column; }
  .preview-pane { width: 300px; background: var(--panel); border-left: 1px solid #ddd; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 20px; }
  .preview-pane img { max-width: 100%; max-height: 80vh; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
  .toolbar { display: flex; gap: 10px; margin-bottom: 15px; flex-wrap: wrap; }
  .toolbar button, .toolbar .upload-btn { padding: 8px 16px; background: var(--accent); color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
  .toolbar button:hover, .toolbar .upload-btn:hover { opacity: 0.9; }
  .toolbar input[type="file"] { display: none; }
  .path-bar { background: var(--panel); padding: 8px 12px; border-radius: 4px; margin-bottom: 10px; font-size: 14px; }
  .file-list { flex: 1; overflow-y: auto; background: var(--panel); border-radius: 4px; }
  .file-item { display: flex; align-items: center; padding: 10px 15px; border-bottom: 1px solid #eee; cursor: pointer; }
  .file-item:hover { background: var(--hover); }
  .file-item .icon { margin-right: 10px; width: 24px; text-align: center; }
  .file-item .name { flex: 1; }
  .file-item .actions { display: flex; gap: 5px; }
  .file-item .actions button { background: none; border: none; cursor: pointer; font-size: 16px; }
  .drop-zone { border: 2px dashed #ccc; border-radius: 4px; padding: 20px; text-align: center; margin-bottom: 15px; color: #888; transition: 0.3s; }
  .drop-zone.dragover { border-color: var(--accent); background: #f0f7ff; }
  .progress { display: none; height: 6px; background: #e0e0e0; border-radius: 3px; margin: 10px 0; overflow: hidden; }
  .progress-bar { height: 100%; width: 0; background: var(--accent); transition: width 0.2s; }
</style>
</head>
<body>
<div class="container">
  <div class="file-manager">
    <div class="path-bar" id="pathBar">/</div>
    <div class="drop-zone" id="dropZone">Перетащите файлы сюда или нажмите для выбора</div>
    <input type="file" id="fileInput" multiple>
    <div class="progress" id="uploadProgress"><div class="progress-bar" id="uploadProgressBar"></div></div>
    <div class="toolbar">
      <button id="btnUpload" class="upload-btn">Загрузить</button>
      <button id="btnNewFolder">Новая папка</button>
      <button id="btnRefresh">Обновить</button>
    </div>
    <div class="file-list" id="fileList"></div>
  </div>
  <div class="preview-pane" id="previewPane">
    <img id="previewImage" src="" alt="Предпросмотр" style="display:none;">
    <div id="previewPlaceholder">Выберите .gcode файл</div>
  </div>
</div>
<script>
let currentPath = '/';
const fileList = document.getElementById('fileList');
const pathBar = document.getElementById('pathBar');
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const uploadProgress = document.getElementById('uploadProgress');
const uploadProgressBar = document.getElementById('uploadProgressBar');
const previewImage = document.getElementById('previewImage');
const previewPlaceholder = document.getElementById('previewPlaceholder');

function apiUrl(path) { return '/api' + path; }

async function fetchJSON(url, options = {}) {
  const res = await fetch(url, options);
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}

async function loadFiles() {
  try {
    const data = await fetchJSON(apiUrl('/list?dir=' + encodeURIComponent(currentPath)));
    renderFiles(data);
  } catch(e) { alert('Ошибка загрузки списка: ' + e.message); }
}

function renderFiles(data) {
  fileList.innerHTML = '';
  if (currentPath !== '/') {
    const parent = document.createElement('div');
    parent.className = 'file-item';
    parent.innerHTML = '<span class="icon">📁</span><span class="name">..</span>';
    parent.onclick = () => { currentPath = currentPath.split('/').slice(0, -2).join('/') + '/'; loadFiles(); };
    fileList.appendChild(parent);
  }
  data.files.forEach(file => {
    const item = document.createElement('div');
    item.className = 'file-item';
    const isDir = file.type === 'dir';
    item.innerHTML = `<span class="icon">${isDir ? '📁' : '📄'}</span><span class="name">${file.name}</span>
      <span class="actions">
        <button data-action="download" title="Скачать">⬇️</button>
        <button data-action="rename" title="Переименовать">✏️</button>
        <button data-action="delete" title="Удалить">🗑️</button>
      </span>`;
    item.querySelector('.name').onclick = () => {
      if (isDir) { currentPath += file.name + '/'; loadFiles(); }
      else { showPreview(file.path); }
    };
    item.querySelector('[data-action="download"]').onclick = (e) => { e.stopPropagation(); window.location.href = apiUrl('/download?path=' + encodeURIComponent(file.path)); };
    item.querySelector('[data-action="rename"]').onclick = (e) => { e.stopPropagation(); renameFile(file.path); };
    item.querySelector('[data-action="delete"]').onclick = (e) => { e.stopPropagation(); deleteFile(file.path); };
    fileList.appendChild(item);
  });
}

async function showPreview(path) {
  try {
    const res = await fetch(apiUrl('/thumbnail?path=' + encodeURIComponent(path)));
    if (res.ok) {
      const blob = await res.blob();
      previewImage.src = URL.createObjectURL(blob);
      previewImage.style.display = 'block';
      previewPlaceholder.style.display = 'none';
    } else {
      previewImage.style.display = 'none';
      previewPlaceholder.style.display = 'block';
    }
  } catch(e) { /* ignore */ }
}

async function deleteFile(path) {
  if (!confirm('Удалить ' + path + '?')) return;
  await fetch(apiUrl('/delete?path=' + encodeURIComponent(path)), { method: 'POST' });
  loadFiles();
}

async function renameFile(path) {
  const newName = prompt('Новое имя:', path.split('/').pop());
  if (!newName) return;
  const newPath = path.substring(0, path.lastIndexOf('/') + 1) + newName;
  await fetch(apiUrl('/rename'), { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({oldPath:path, newPath:newPath}) });
  loadFiles();
}

async function createFolder() {
  const name = prompt('Имя новой папки:');
  if (!name) return;
  await fetch(apiUrl('/mkdir'), { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({path: currentPath + name}) });
  loadFiles();
}

function uploadFiles(files) {
  if (!files.length) return;
  uploadProgress.style.display = 'block';
  const formData = new FormData();
  for (let f of files) formData.append('files', f, currentPath + f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', apiUrl('/upload'));
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded * 100 / e.total);
      uploadProgressBar.style.width = pct + '%';
    }
  };
  xhr.onload = () => {
    uploadProgress.style.display = 'none';
    uploadProgressBar.style.width = '0';
    loadFiles();
  };
  xhr.onerror = () => alert('Ошибка загрузки');
  xhr.send(formData);
}

// Обработчики
dropZone.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', () => { uploadFiles(fileInput.files); fileInput.value = ''; });
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => { e.preventDefault(); dropZone.classList.remove('dragover'); uploadFiles(e.dataTransfer.files); });
document.getElementById('btnUpload').addEventListener('click', () => fileInput.click());
document.getElementById('btnNewFolder').addEventListener('click', createFolder);
document.getElementById('btnRefresh').addEventListener('click', loadFiles);

loadFiles();
</script>
</body>
</html>
)rawliteral";

const char INDEX_M_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SD Card Manager (Mobile)</title>
<style>
  :root { --bg: #f5f5f5; --panel: #fff; --text: #333; --accent: #4a90d9; --hover: #e9f0fa; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); display: flex; flex-direction: column; height: 100vh; }
  .preview-pane { height: 30vh; background: var(--panel); display: flex; align-items: center; justify-content: center; padding: 10px; border-bottom: 1px solid #ddd; }
  .preview-pane img { max-width: 100%; max-height: 100%; border-radius: 8px; }
  .file-manager { flex: 1; padding: 10px; display: flex; flex-direction: column; }
  .path-bar { background: var(--panel); padding: 8px 12px; border-radius: 4px; margin-bottom: 10px; font-size: 14px; }
  .drop-zone { border: 2px dashed #ccc; border-radius: 4px; padding: 15px; text-align: center; margin-bottom: 10px; color: #888; }
  .drop-zone.dragover { border-color: var(--accent); background: #f0f7ff; }
  .toolbar { display: flex; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
  .toolbar button { padding: 8px 12px; background: var(--accent); color: white; border: none; border-radius: 4px; font-size: 14px; }
  .file-list { flex: 1; overflow-y: auto; background: var(--panel); border-radius: 4px; }
  .file-item { display: flex; align-items: center; padding: 12px 15px; border-bottom: 1px solid #eee; }
  .file-item:active { background: var(--hover); }
  .file-item .icon { margin-right: 10px; }
  .file-item .name { flex: 1; word-break: break-all; }
  .file-item .actions button { background: none; border: none; font-size: 18px; margin-left: 5px; }
  .progress { display: none; height: 6px; background: #e0e0e0; border-radius: 3px; margin: 10px 0; }
  .progress-bar { height: 100%; background: var(--accent); }
</style>
</head>
<body>
<div class="preview-pane" id="previewPane">
  <img id="previewImage" style="display:none;">
  <div id="previewPlaceholder">Выберите .gcode файл</div>
</div>
<div class="file-manager">
  <div class="path-bar" id="pathBar">/</div>
  <div class="drop-zone" id="dropZone">Нажмите для загрузки файлов</div>
  <input type="file" id="fileInput" multiple style="display:none">
  <div class="progress" id="uploadProgress"><div class="progress-bar" id="uploadProgressBar"></div></div>
  <div class="toolbar">
    <button id="btnNewFolder">📁</button>
    <button id="btnRefresh">🔄</button>
  </div>
  <div class="file-list" id="fileList"></div>
</div>
<script>
let currentPath = '/';
const fileList = document.getElementById('fileList');
const pathBar = document.getElementById('pathBar');
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const uploadProgress = document.getElementById('uploadProgress');
const uploadProgressBar = document.getElementById('uploadProgressBar');
const previewImage = document.getElementById('previewImage');
const previewPlaceholder = document.getElementById('previewPlaceholder');

function apiUrl(path) { return '/api' + path; }

async function fetchJSON(url, options = {}) {
  const res = await fetch(url, options);
  if (!res.ok) throw new Error(await res.text());
  return res.json();
}

async function loadFiles() {
  try {
    const data = await fetchJSON(apiUrl('/list?dir=' + encodeURIComponent(currentPath)));
    renderFiles(data);
  } catch(e) { alert('Ошибка: ' + e.message); }
}

function renderFiles(data) {
  fileList.innerHTML = '';
  if (currentPath !== '/') {
    const parent = document.createElement('div');
    parent.className = 'file-item';
    parent.innerHTML = '<span class="icon">📁</span><span class="name">..</span>';
    parent.onclick = () => { currentPath = currentPath.split('/').slice(0, -2).join('/') + '/'; loadFiles(); };
    fileList.appendChild(parent);
  }
  data.files.forEach(file => {
    const item = document.createElement('div');
    item.className = 'file-item';
    const isDir = file.type === 'dir';
    item.innerHTML = `<span class="icon">${isDir ? '📁' : '📄'}</span><span class="name">${file.name}</span>
      <span class="actions">
        <button data-action="download">⬇️</button>
        <button data-action="rename">✏️</button>
        <button data-action="delete">🗑️</button>
      </span>`;
    item.querySelector('.name').onclick = () => {
      if (isDir) { currentPath += file.name + '/'; loadFiles(); }
      else { showPreview(file.path); }
    };
    item.querySelector('[data-action="download"]').onclick = (e) => { e.stopPropagation(); window.location.href = apiUrl('/download?path=' + encodeURIComponent(file.path)); };
    item.querySelector('[data-action="rename"]').onclick = (e) => { e.stopPropagation(); renameFile(file.path); };
    item.querySelector('[data-action="delete"]').onclick = (e) => { e.stopPropagation(); deleteFile(file.path); };
    fileList.appendChild(item);
  });
}

async function showPreview(path) {
  try {
    const res = await fetch(apiUrl('/thumbnail?path=' + encodeURIComponent(path)));
    if (res.ok) {
      const blob = await res.blob();
      previewImage.src = URL.createObjectURL(blob);
      previewImage.style.display = 'block';
      previewPlaceholder.style.display = 'none';
    }
  } catch(e) {}
}

async function deleteFile(path) {
  if (!confirm('Удалить ' + path + '?')) return;
  await fetch(apiUrl('/delete?path=' + encodeURIComponent(path)), { method: 'POST' });
  loadFiles();
}

async function renameFile(path) {
  const newName = prompt('Новое имя:', path.split('/').pop());
  if (!newName) return;
  const newPath = path.substring(0, path.lastIndexOf('/') + 1) + newName;
  await fetch(apiUrl('/rename'), { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({oldPath:path, newPath:newPath}) });
  loadFiles();
}

function uploadFiles(files) {
  if (!files.length) return;
  uploadProgress.style.display = 'block';
  const formData = new FormData();
  for (let f of files) formData.append('files', f, currentPath + f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', apiUrl('/upload'));
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      uploadProgressBar.style.width = Math.round(e.loaded * 100 / e.total) + '%';
    }
  };
  xhr.onload = () => { uploadProgress.style.display = 'none'; uploadProgressBar.style.width = '0'; loadFiles(); };
  xhr.send(formData);
}

dropZone.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', () => { uploadFiles(fileInput.files); fileInput.value = ''; });
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => { e.preventDefault(); dropZone.classList.remove('dragover'); uploadFiles(e.dataTransfer.files); });
document.getElementById('btnNewFolder').addEventListener('click', async () => {
  const name = prompt('Имя папки:');
  if (!name) return;
  await fetch(apiUrl('/mkdir'), { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({path: currentPath + name}) });
  loadFiles();
});
document.getElementById('btnRefresh').addEventListener('click', loadFiles);

loadFiles();
</script>
</body>
</html>
)rawliteral";

// Вспомогательные функции
void setLED(bool on) {
  digitalWrite(LED, on ? LOW : HIGH); // активный low
}

void blinkLED(int times, int delayMs = 200) {
  for (int i = 0; i < times; i++) {
    setLED(true); delay(delayMs);
    setLED(false); delay(delayMs);
  }
}

bool readSetupIni(String &ssid, String &pass) {
  if (!SD.exists("/SETUP.INI")) return false;
  File f = SD.open("/SETUP.INI", "r");
  if (!f) return false;
  String content = f.readString();
  f.close();
  // Простейший парсинг
  int wifiSection = content.indexOf("[WIFI]");
  if (wifiSection < 0) return false;
  int ssidPos = content.indexOf("SSID=", wifiSection);
  int passPos = content.indexOf("PASSWORD=", wifiSection);
  if (ssidPos < 0 || passPos < 0) return false;
  int ssidEnd = content.indexOf('\n', ssidPos);
  int passEnd = content.indexOf('\n', passPos);
  if (ssidEnd < 0) ssidEnd = content.length();
  if (passEnd < 0) passEnd = content.length();
  ssid = content.substring(ssidPos + 5, ssidEnd);
  pass = content.substring(passPos + 9, passEnd);
  ssid.trim(); pass.trim();
  return (ssid.length() > 0);
}

bool connectToWiFi(String ssid, String pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { // 15 sec
    delay(500);
    attempts++;
    setLED(!digitalRead(LED)); // мигаем
  }
  return WiFi.status() == WL_CONNECTED;
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  // Статический IP для AP
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
}

void initWiFi() {
  String ssid, pass;
  bool connected = false;
  // 1. Попытка из SETUP.INI
  if (readSetupIni(ssid, pass)) {
    if (connectToWiFi(ssid, pass)) {
      connected = true;
    }
  }
  // 2. Fallback
  if (!connected) {
    if (connectToWiFi(FALLBACK_SSID, FALLBACK_PASSWORD)) {
      connected = true;
    }
  }
  // 3. AP mode
  if (!connected) {
    startAPMode();
    Serial.println("AP mode started");
  } else {
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  }
}

// Обработчики API
void handleList() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String dir = server.arg("dir");
  if (dir.length() == 0) dir = "/";
  if (!dir.endsWith("/")) dir += "/";
  if (!SD.exists(dir)) { server.send(404, "text/plain", "Directory not found"); return; }
  File root = SD.open(dir);
  if (!root || !root.isDirectory()) { server.send(500, "text/plain", "Failed to open directory"); return; }
  String json = "{\"files\":[";
  File entry = root.openNextFile();
  bool first = true;
  while (entry) {
    String name = entry.name();
    if (name != "." && name != "..") {
      if (!first) json += ",";
      first = false;
      bool isDir = entry.isDirectory();
      String fullPath = dir + name;
      json += "{\"name\":\"" + name + "\",\"path\":\"" + fullPath + "\",\"type\":\"" + (isDir ? "dir" : "file") + "\",\"size\":" + String(entry.size()) + "}";
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
  json += "]}";
  server.send(200, "application/json", json);
}

File uploadFile;

void handleFileUpload() {
  if (!sd_ok) return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    // filename содержит полный путь (мы передали path + name)
    if (!filename.startsWith("/")) filename = "/" + filename;
    // Создаём промежуточные директории
    int slash = filename.lastIndexOf('/');
    if (slash > 0) {
      String dir = filename.substring(0, slash);
      if (!SD.exists(dir)) {
        // Создаём рекурсивно
        String current = "";
        int start = 1;
        while (true) {
          int next = dir.indexOf('/', start);
          if (next == -1) {
            current += dir.substring(start);
            if (current.length() > 0) SD.mkdir(current);
            break;
          } else {
            current += dir.substring(start, next + 1);
            if (!SD.exists(current)) SD.mkdir(current);
            start = next + 1;
          }
        }
      }
    }
    uploadFile = SD.open(filename, "w");
    if (!uploadFile) {
      Serial.println("Failed to open upload file: " + filename);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    setLED(false); // выключаем LED после загрузки
  }
}

void handleUpload() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  setLED(true); // индикация загрузки
  server.send(200, "text/plain", "OK");
}

void handleDownload() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String path = server.arg("path");
  if (path.length() == 0 || !SD.exists(path)) { server.send(404, "text/plain", "File not found"); return; }
  File file = SD.open(path, "r");
  if (!file) { server.send(500, "text/plain", "Failed to open file"); return; }
  String contentType = "application/octet-stream";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".gcode")) contentType = "text/plain";
  server.streamFile(file, contentType);
  file.close();
}

void handleDelete() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String path = server.arg("path");
  if (path.length() == 0) { server.send(400, "text/plain", "No path"); return; }
  if (SD.exists(path)) {
    if (SD.remove(path)) server.send(200, "text/plain", "OK");
    else server.send(500, "text/plain", "Delete failed");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void handleRename() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String body = server.arg("plain");
  // Простейший парсинг JSON: {"oldPath":"...", "newPath":"..."}
  int oldPos = body.indexOf("\"oldPath\":\"");
  int newPos = body.indexOf("\"newPath\":\"");
  if (oldPos < 0 || newPos < 0) { server.send(400, "text/plain", "Invalid JSON"); return; }
  int oldStart = oldPos + 11;
  int oldEnd = body.indexOf('"', oldStart);
  int newStart = newPos + 11;
  int newEnd = body.indexOf('"', newStart);
  if (oldEnd < 0 || newEnd < 0) { server.send(400, "text/plain", "Invalid JSON"); return; }
  String oldPath = body.substring(oldStart, oldEnd);
  String newPath = body.substring(newStart, newEnd);
  if (oldPath.length() == 0 || newPath.length() == 0) { server.send(400, "text/plain", "Empty paths"); return; }
  if (!SD.exists(oldPath)) { server.send(404, "text/plain", "Source not found"); return; }
  if (SD.rename(oldPath, newPath)) server.send(200, "text/plain", "OK");
  else server.send(500, "text/plain", "Rename failed");
}

void handleMkdir() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String body = server.arg("plain");
  int pathPos = body.indexOf("\"path\":\"");
  if (pathPos < 0) { server.send(400, "text/plain", "Invalid JSON"); return; }
  int pathStart = pathPos + 8;
  int pathEnd = body.indexOf('"', pathStart);
  if (pathEnd < 0) { server.send(400, "text/plain", "Invalid JSON"); return; }
  String path = body.substring(pathStart, pathEnd);
  if (path.length() == 0) { server.send(400, "text/plain", "Empty path"); return; }
  if (SD.mkdir(path)) server.send(200, "text/plain", "OK");
  else server.send(500, "text/plain", "Mkdir failed");
}

void handleThumbnail() {
  if (!sd_ok) { server.send(500, "text/plain", "SD not available"); return; }
  String path = server.arg("path");
  if (path.length() == 0) { server.send(400, "text/plain", "No path"); return; }
  // Если запрошен .gcode, ищем миниатюру
  String thumbPath = "";
  if (path.endsWith(".gcode") || path.endsWith(".gco")) {
    // Ищем рядом файл с тем же именем, но .jpg или .png
    String base = path.substring(0, path.lastIndexOf('.'));
    if (SD.exists(base + ".jpg")) thumbPath = base + ".jpg";
    else if (SD.exists(base + ".png")) thumbPath = base + ".png";
    // Если нет, ищем logo в корне
    if (thumbPath.length() == 0) {
      if (SD.exists("/logo.jpg")) thumbPath = "/logo.jpg";
      else if (SD.exists("/logo.png")) thumbPath = "/logo.png";
    }
  }
  if (thumbPath.length() > 0 && SD.exists(thumbPath)) {
    File file = SD.open(thumbPath, "r");
    if (file) {
      String contentType = thumbPath.endsWith(".jpg") ? "image/jpeg" : "image/png";
      server.streamFile(file, contentType);
      file.close();
      return;
    }
  }
  server.send_P(200, "image/png", (const char*)grayPixelPng, sizeof(grayPixelPng));
}

void serveIndex() {
  // Определяем мобильный по User-Agent
  String ua = server.header("User-Agent");
  bool isMobile = ua.indexOf("Mobile") >= 0 || ua.indexOf("Android") >= 0 || ua.indexOf("iPhone") >= 0;
  String indexPath = isMobile ? "/index_m.html" : "/index.html";
  if (sd_ok && SD.exists(indexPath)) {
    File file = SD.open(indexPath, "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
      return;
    }
  }
  // Fallback встроенный
  server.send_P(200, "text/html", isMobile ? INDEX_M_HTML : INDEX_HTML);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  setLED(false); // выключен
  pinMode(CS_SENSE, INPUT_PULLUP);

  // Инициализация SD
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    sd_ok = false;
    blinkLED(5, 100); // быстрая индикация ошибки SD
  } else {
    sd_ok = true;
    Serial.println("SD initialized.");
  }

  // WiFi
  initWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    setLED(true); // постоянное свечение при подключении
  } else {
    // AP mode
    setLED(true);
  }

  // Маршруты API
  server.on("/api/list", HTTP_GET, handleList);
  server.on("/api/upload", HTTP_POST, handleUpload, handleFileUpload);
  server.on("/api/download", HTTP_GET, handleDownload);
  server.on("/api/delete", HTTP_POST, handleDelete);
  server.on("/api/rename", HTTP_POST, handleRename);
  server.on("/api/mkdir", HTTP_POST, handleMkdir);
  server.on("/api/thumbnail", HTTP_GET, handleThumbnail);

  // Корневой маршрут
  server.on("/", HTTP_GET, serveIndex);
  // Также отдаём index.html напрямую
  server.on("/index.html", HTTP_GET, []() {
    if (sd_ok && SD.exists("/index.html")) {
      File file = SD.open("/index.html", "r");
      if (file) { server.streamFile(file, "text/html"); file.close(); return; }
    }
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/index_m.html", HTTP_GET, []() {
    if (sd_ok && SD.exists("/index_m.html")) {
      File file = SD.open("/index_m.html", "r");
      if (file) { server.streamFile(file, "text/html"); file.close(); return; }
    }
    server.send_P(200, "text/html", INDEX_M_HTML);
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
