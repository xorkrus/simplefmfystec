// ============================================================================
// simplefmfystec – SD WIFI File Manager for FYSETC (ESP8285)
// ============================================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

// ------------------------------- ПИНЫ ---------------------------------------
#define SD_CS      4
#define MISO       12
#define MOSI       13
#define SCLK       14
#define CS_SENSE   5
#define LED        2

// ------------------------------- ГЛОБАЛЬНЫЕ ОБЪЕКТЫ -------------------------
ESP8266WebServer server(80);
File uploadFile;
String uploadPath;
bool uploadSuccess = false;

// ------------------------------- ПРОТОТИПЫ -----------------------------------
void connectWiFi();
void handleRoot();
void handleMobileRoot();
void handleList();
void handleDownload();
void handleUpload();
void handleDelete();
void handleRename();
void handleMove();
void handleMkdir();
void handleThumbnail();
void handleNotFound();
bool initSD();
String getThumbnailPath(const String& gcodePath);
void sendThumbnailPlaceholder();
bool isSDBusy();
void blinkLED(int times, int delayMs);

// ============================================================================
// ВСТРОЕННЫЙ WEB‑ИНТЕРФЕЙС (FALLBACK)
// ============================================================================

// ---- Десктопная версия (index.html) ----
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SD File Manager</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1e1e2f; color: #eee; display: flex; height: 100vh; overflow: hidden; }
    #container { display: flex; width: 100%; height: 100vh; }
    #left { flex: 2; padding: 20px; overflow-y: auto; background: #2a2a3e; border-right: 2px solid #3a3a5a; }
    #right { flex: 1; padding: 20px; display: flex; flex-direction: column; align-items: center; justify-content: center; background: #252538; }
    #right img { max-width: 100%; max-height: 90vh; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.6); background: #333; }
    #path { font-size: 14px; color: #aaa; margin-bottom: 12px; word-break: break-all; }
    #controls { display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 16px; }
    #controls button, #controls input[type="file"] { background: #3b3b5a; border: none; color: #fff; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 14px; transition: 0.2s; }
    #controls button:hover { background: #5a5a7e; }
    #controls input[type="file"] { display: inline-block; padding: 6px 12px; background: #4a4a6a; }
    #fileList { list-style: none; }
    #fileList li { display: flex; align-items: center; padding: 8px 12px; margin: 4px 0; background: #32324a; border-radius: 6px; cursor: pointer; transition: 0.15s; }
    #fileList li:hover { background: #404060; }
    #fileList li .name { flex: 1; margin-left: 10px; }
    #fileList li .size { color: #888; font-size: 13px; margin-right: 12px; }
    #fileList li .actions button { background: none; border: none; color: #9cf; cursor: pointer; font-size: 16px; margin-left: 6px; }
    #fileList li .actions button:hover { color: #fff; }
    .dir::before { content: "📁 "; }
    .file::before { content: "📄 "; }
    .gcode::before { content: "🖨️ "; }
    .dropzone { border: 2px dashed #5a5a7e; border-radius: 12px; padding: 20px; text-align: center; margin: 12px 0; color: #888; transition: 0.3s; }
    .dropzone.dragover { background: #3a3a5a; border-color: #8cf; }
    #progress-bar { width: 100%; height: 6px; background: #2a2a3e; border-radius: 4px; margin: 8px 0; overflow: hidden; display: none; }
    #progress-bar div { height: 100%; width: 0%; background: #6cf; transition: width 0.2s; }
    #status { margin-top: 8px; font-size: 14px; color: #aaa; }
    .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.7); justify-content: center; align-items: center; z-index: 1000; }
    .modal-content { background: #2a2a3e; padding: 24px; border-radius: 12px; min-width: 300px; }
    .modal-content input { width: 100%; padding: 8px; margin: 8px 0; background: #1e1e2f; border: 1px solid #4a4a6a; color: #eee; border-radius: 4px; }
    .modal-content button { margin: 4px; }
    @media (max-width: 800px) {
      #container { flex-direction: column; }
      #left { flex: 2; border-right: none; border-bottom: 2px solid #3a3a5a; }
      #right { flex: 1; min-height: 200px; }
    }
  </style>
</head>
<body>
<div id="container">
  <div id="left">
    <div id="path">/</div>
    <div id="controls">
      <button onclick="refreshList()">🔄 Обновить</button>
      <button onclick="mkdir()">📁 Создать папку</button>
      <label for="fileInput" style="cursor:pointer;">📤 Загрузить</label>
      <input type="file" id="fileInput" multiple style="display:none;" onchange="uploadFiles(this.files)">
      <button onclick="window.location.href='/download?file='+encodeURIComponent(currentPath)">⬇ Скачать</button>
    </div>
    <div id="dropzone" class="dropzone">Перетащите файлы сюда для загрузки</div>
    <div id="progress-bar"><div></div></div>
    <div id="status"></div>
    <ul id="fileList"></ul>
  </div>
  <div id="right">
    <img id="preview" src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='300' height='300'%3E%3Crect width='300' height='300' fill='%23555'/%3E%3C/svg%3E" alt="preview">
  </div>
</div>

<!-- Модальное окно для создания папки -->
<div id="mkdirModal" class="modal">
  <div class="modal-content">
    <h3>Создать папку</h3>
    <input type="text" id="newDirName" placeholder="Имя папки">
    <button onclick="doMkdir()">Создать</button>
    <button onclick="closeModal()">Отмена</button>
  </div>
</div>

<script>
let currentPath = "/";
let currentFiles = [];

function refreshList() {
  fetch('/list?path=' + encodeURIComponent(currentPath))
    .then(r => r.json())
    .then(data => {
      if (!data.success) { status('Ошибка: ' + data.message); return; }
      currentFiles = data.files || [];
      renderList(currentFiles);
    })
    .catch(() => status('Ошибка загрузки списка'));
}

function renderList(files) {
  const ul = document.getElementById('fileList');
  ul.innerHTML = '';
  // Кнопка "Наверх"
  if (currentPath !== "/") {
    const li = document.createElement('li');
    li.className = 'dir';
    li.innerHTML = `<span class="name">..</span>`;
    li.onclick = () => { currentPath = currentPath.substring(0, currentPath.lastIndexOf('/'));
                         if (currentPath === '') currentPath = '/';
                         refreshList(); };
    ul.appendChild(li);
  }
  files.sort((a,b) => (a.isDir === b.isDir) ? a.name.localeCompare(b.name) : (a.isDir ? -1 : 1));
  for (let f of files) {
    const li = document.createElement('li');
    const cls = f.isDir ? 'dir' : (f.name.endsWith('.gcode') || f.name.endsWith('.gco') ? 'gcode' : 'file');
    li.className = cls;
    const sizeStr = f.isDir ? '' : (f.size < 1024 ? f.size + ' B' : (f.size < 1048576 ? (f.size/1024).toFixed(1)+' KB' : (f.size/1048576).toFixed(1)+' MB'));
    li.innerHTML = `<span class="name">${f.name}</span><span class="size">${sizeStr}</span>
      <span class="actions">
        ${!f.isDir ? `<button onclick="downloadFile('${f.name}')" title="Скачать">⬇</button>` : ''}
        <button onclick="renameFile('${f.name}')" title="Переименовать">✏️</button>
        <button onclick="deleteFile('${f.name}')" title="Удалить">🗑</button>
      </span>`;
    li.onclick = (e) => {
      if (e.target.tagName === 'BUTTON') return;
      if (f.isDir) {
        currentPath = currentPath === '/' ? '/' + f.name : currentPath + '/' + f.name;
        refreshList();
      } else {
        showPreview(f.name);
      }
    };
    ul.appendChild(li);
  }
}

function showPreview(filename) {
  const img = document.getElementById('preview');
  if (filename.endsWith('.gcode') || filename.endsWith('.gco')) {
    const path = currentPath === '/' ? '/' + filename : currentPath + '/' + filename;
    img.src = '/thumbnail?file=' + encodeURIComponent(path);
  } else {
    img.src = 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" width="300" height="300"%3E%3Crect width="300" height="300" fill="%23555"/%3E%3C/svg%3E';
  }
}

function downloadFile(name) {
  const path = currentPath === '/' ? '/' + name : currentPath + '/' + name;
  window.location.href = '/download?file=' + encodeURIComponent(path);
}

function deleteFile(name) {
  if (!confirm('Удалить "' + name + '"?')) return;
  const path = currentPath === '/' ? '/' + name : currentPath + '/' + name;
  fetch('/delete', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: path })
  })
  .then(r => r.json())
  .then(data => { if (data.success) refreshList(); else status('Ошибка: ' + data.message); });
}

function renameFile(name) {
  const newName = prompt('Новое имя для "' + name + '":', name);
  if (!newName || newName === name) return;
  const oldPath = currentPath === '/' ? '/' + name : currentPath + '/' + name;
  const newPath = currentPath === '/' ? '/' + newName : currentPath + '/' + newName;
  fetch('/rename', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ oldPath: oldPath, newPath: newPath })
  })
  .then(r => r.json())
  .then(data => { if (data.success) refreshList(); else status('Ошибка: ' + data.message); });
}

function mkdir() {
  document.getElementById('mkdirModal').style.display = 'flex';
}
function closeModal() {
  document.getElementById('mkdirModal').style.display = 'none';
}
function doMkdir() {
  const name = document.getElementById('newDirName').value.trim();
  if (!name) return;
  const path = currentPath === '/' ? '/' + name : currentPath + '/' + name;
  fetch('/mkdir', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path: path })
  })
  .then(r => r.json())
  .then(data => { if (data.success) { refreshList(); closeModal(); } else status('Ошибка: ' + data.message); });
}

// Drag & Drop
const dropzone = document.getElementById('dropzone');
dropzone.addEventListener('dragover', (e) => { e.preventDefault(); dropzone.classList.add('dragover'); });
dropzone.addEventListener('dragleave', () => { dropzone.classList.remove('dragover'); });
dropzone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropzone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) uploadFiles(e.dataTransfer.files);
});

function uploadFiles(files) {
  const formData = new FormData();
  for (let f of files) formData.append('file', f);
  formData.append('path', currentPath);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/upload', true);
  xhr.upload.onprogress = (e) => {
    const pct = e.lengthComputable ? Math.round((e.loaded / e.total) * 100) : 0;
    document.getElementById('progress-bar').style.display = 'block';
    document.querySelector('#progress-bar div').style.width = pct + '%';
  };
  xhr.onload = () => {
    document.getElementById('progress-bar').style.display = 'none';
    if (xhr.status === 200) { refreshList(); status('Загрузка завершена'); } else status('Ошибка загрузки');
  };
  xhr.onerror = () => { document.getElementById('progress-bar').style.display = 'none'; status('Ошибка сети'); };
  xhr.send(formData);
}

function status(msg) {
  document.getElementById('status').textContent = msg;
}

// Инициализация
refreshList();
</script>
</body>
</html>
)rawliteral";

// ---- Мобильная версия (index_m.html) ----
const char index_m_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>SD File Manager (mobile)</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1e1e2f; color: #eee; display: flex; flex-direction: column; height: 100vh; overflow: hidden; }
    #top { flex: 0 0 45vh; display: flex; justify-content: center; align-items: center; background: #252538; border-bottom: 2px solid #3a3a5a; padding: 10px; }
    #top img { max-width: 100%; max-height: 100%; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); background: #333; }
    #bottom { flex: 1; display: flex; flex-direction: column; padding: 12px; overflow: hidden; }
    #path { font-size: 14px; color: #aaa; margin-bottom: 6px; word-break: break-all; }
    #controls { display: flex; gap: 6px; flex-wrap: wrap; margin-bottom: 8px; }
    #controls button, #controls input[type="file"] { background: #3b3b5a; border: none; color: #fff; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 13px; transition: 0.2s; }
    #controls button:hover { background: #5a5a7e; }
    #controls input[type="file"] { display: inline-block; padding: 4px 10px; background: #4a4a6a; }
    #fileList { list-style: none; overflow-y: auto; flex: 1; }
    #fileList li { display: flex; align-items: center; padding: 6px 10px; margin: 3px 0; background: #32324a; border-radius: 6px; cursor: pointer; transition: 0.15s; }
    #fileList li:hover { background: #404060; }
    #fileList li .name { flex: 1; margin-left: 6px; font-size: 15px; }
    #fileList li .size { color: #888; font-size: 12px; margin-right: 8px; }
    #fileList li .actions button { background: none; border: none; color: #9cf; cursor: pointer; font-size: 14px; margin-left: 4px; }
    #fileList li .actions button:hover { color: #fff; }
    .dir::before { content: "📁 "; }
    .file::before { content: "📄 "; }
    .gcode::before { content: "🖨️ "; }
    .dropzone { border: 2px dashed #5a5a7e; border-radius: 8px; padding: 12px; text-align: center; margin: 6px 0; color: #888; font-size: 13px; transition: 0.3s; }
    .dropzone.dragover { background: #3a3a5a; border-color: #8cf; }
    #progress-bar { width: 100%; height: 4px; background: #2a2a3e; border-radius: 4px; margin: 4px 0; overflow: hidden; display: none; }
    #progress-bar div { height: 100%; width: 0%; background: #6cf; transition: width 0.2s; }
    #status { font-size: 12px; color: #aaa; margin-top: 4px; }
    .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.7); justify-content: center; align-items: center; z-index: 1000; }
    .modal-content { background: #2a2a3e; padding: 20px; border-radius: 12px; min-width: 260px; }
    .modal-content input { width: 100%; padding: 8px; margin: 8px 0; background: #1e1e2f; border: 1px solid #4a4a6a; color: #eee; border-radius: 4px; }
    .modal-content button { margin: 4px; }
  </style>
</head>
<body>
<div id="top">
  <img id="preview" src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='300' height='300'%3E%3Crect width='300' height='300' fill='%23555'/%3E%3C/svg%3E" alt="preview">
</div>
<div id="bottom">
  <div id="path">/</div>
  <div id="controls">
    <button onclick="refreshList()">🔄</button>
    <button onclick="mkdir()">📁</button>
    <label for="fileInput" style="cursor:pointer;">📤</label>
    <input type="file" id="fileInput" multiple style="display:none;" onchange="uploadFiles(this.files)">
    <button onclick="window.location.href='/download?file='+encodeURIComponent(currentPath)">⬇</button>
  </div>
  <div id="dropzone" class="dropzone">Перетащите файлы</div>
  <div id="progress-bar"><div></div></div>
  <div id="status"></div>
  <ul id="fileList"></ul>
</div>

<!-- Модалка mkdir -->
<div id="mkdirModal" class="modal">
  <div class="modal-content">
    <h4>Создать папку</h4>
    <input type="text" id="newDirName" placeholder="Имя папки">
    <button onclick="doMkdir()">Создать</button>
    <button onclick="closeModal()">Отмена</button>
  </div>
</div>

<script>
let currentPath = "/";

function refreshList() {
  fetch('/list?path=' + encodeURIComponent(currentPath))
    .then(r => r.json())
    .then(data => {
      if (!data.success) { status('Ошибка: ' + data.message); return; }
      renderList(data.files || []);
    })
    .catch(() => status('Ошибка загрузки'));
}

function renderList(files) {
  const ul = document.getElementById('fileList');
  ul.innerHTML = '';
  if (currentPath !== "/") {
    const li = document.createElement('li');
    li.className = 'dir';
    li.innerHTML = `<span class="name">..</span>`;
    li.onclick = () => { currentPath = currentPath.substring(0, currentPath.lastIndexOf('/'));
                         if (currentPath === '') currentPath = '/';
                         refreshList(); };
    ul.appendChild(li);
  }
  files.sort((a,b) => (a.isDir === b.isDir) ? a.name.localeCompare(b.name) : (a.isDir ? -1 : 1));
  for (let f of files) {
    const li = document.createElement('li');
    const cls = f.isDir ? 'dir' : (f.name.endsWith('.gcode') || f.name.endsWith('.gco') ? 'gcode' : 'file');
    li.className = cls;
    const sizeStr = f.isDir ? '' : (f.size < 1024 ? f.size + ' B' : (f.size < 1048576 ? (f.size/1024).toFixed(1)+' KB' : (f.size/1048576).toFixed(1)+' MB'));
    li.innerHTML = `<span class="name">${f.name}</span><span class="size">${sizeStr}</span>
      <span class="actions">
        ${!f.isDir ? `<button onclick="downloadFile('${f.name}')" title="Скачать">⬇</button>` : ''}
        <button onclick="renameFile('${f.name}')" title="Переименовать">✏️</button>
        <button onclick="deleteFile('${f.name}')" title="Удалить">🗑</button>
      </span>`;
    li.onclick = (e) => {
      if (e.target.tagName === 'BUTTON') return;
      if (f.isDir) {
        currentPath = currentPath === '/' ? '/' + f.name : currentPath + '/' + f.name;
        refreshList();
      } else {
        showPreview(f.name);
      }
    };
    ul.appendChild(li);
  }
}

function showPreview(filename) {
  const img = document.getElementById('preview');
  if (filename.endsWith('.gcode') || filename.endsWith('.gco')) {
    const path = currentPath === '/' ? '/' + filename : currentPath + '/' + filename;
    img.src = '/thumbnail?file=' + encodeURIComponent(path);
  } else {
    img.src = 'data:image/svg+xml,%3Csvg xmlns="http://www.w3.org/2000/svg" width="300" height="300"%3E%3Crect width="300" height="300" fill="%23555"/%3E%3C/svg%3E';
  }
}

// Остальные функции идентичны десктопной версии (downloadFile, deleteFile, renameFile, mkdir, uploadFiles, статус и т.д.)
// Для краткости здесь приведены только ссылки на те же функции, но в реальном коде они должны быть скопированы.
// В данном проекте для экономии места они дублируются в полном коде. Ниже показан минимум.
function downloadFile(name) { const path = currentPath === '/' ? '/' + name : currentPath + '/' + name; window.location.href = '/download?file=' + encodeURIComponent(path); }
function deleteFile(name) { if (!confirm('Удалить "'+name+'"?')) return; const path = currentPath === '/' ? '/' + name : currentPath + '/' + name; fetch('/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path:path})}).then(r=>r.json()).then(d=>{if(d.success)refreshList();else status('Ошибка: '+d.message);}); }
function renameFile(name) { const newName = prompt('Новое имя:', name); if(!newName||newName===name)return; const oldPath = currentPath === '/' ? '/' + name : currentPath + '/' + name; const newPath = currentPath === '/' ? '/' + newName : currentPath + '/' + newName; fetch('/rename',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({oldPath,newPath})}).then(r=>r.json()).then(d=>{if(d.success)refreshList();else status('Ошибка: '+d.message);}); }
function mkdir() { document.getElementById('mkdirModal').style.display = 'flex'; }
function closeModal() { document.getElementById('mkdirModal').style.display = 'none'; }
function doMkdir() { const name = document.getElementById('newDirName').value.trim(); if(!name)return; const path = currentPath === '/' ? '/' + name : currentPath + '/' + name; fetch('/mkdir',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path})}).then(r=>r.json()).then(d=>{if(d.success){refreshList();closeModal();}else status('Ошибка: '+d.message);}); }
function uploadFiles(files) { const fd = new FormData(); for(let f of files) fd.append('file', f); fd.append('path', currentPath); const xhr = new XMLHttpRequest(); xhr.open('POST', '/upload', true); xhr.upload.onprogress = (e) => { const pct = e.lengthComputable ? Math.round((e.loaded/e.total)*100) : 0; document.getElementById('progress-bar').style.display='block'; document.querySelector('#progress-bar div').style.width=pct+'%'; }; xhr.onload = () => { document.getElementById('progress-bar').style.display='none'; if(xhr.status===200){refreshList();status('OK');}else status('Ошибка'); }; xhr.onerror = () => { document.getElementById('progress-bar').style.display='none'; status('Сеть'); }; xhr.send(fd); }
function status(msg) { document.getElementById('status').textContent = msg; }
refreshList();
</script>
</body>
</html>
)rawliteral";

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

bool isSDBusy() {
  return digitalRead(CS_SENSE) == LOW;
}

void blinkLED(int times, int delayMs) {
  pinMode(LED, OUTPUT);
  for (int i=0; i<times; i++) {
    digitalWrite(LED, HIGH);
    delay(delayMs);
    digitalWrite(LED, LOW);
    delay(delayMs);
  }
}

bool initSD() {
  SPI.begin(SCLK, MISO, MOSI, SD_CS);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed");
    return false;
  }
  Serial.println("SD initialized");
  return true;
}

// Чтение SETUP.INI и извлечение SSID/PASSWORD
bool readWiFiFromSD(String &ssid, String &password) {
  if (!SD.exists("/SETUP.INI")) return false;
  File f = SD.open("/SETUP.INI", FILE_READ);
  if (!f) return false;
  bool inWifi = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("[WIFI]")) {
      inWifi = true;
      continue;
    }
    if (inWifi && line.startsWith("SSID=")) {
      ssid = line.substring(5);
      ssid.trim();
    } else if (inWifi && line.startsWith("PASSWORD=")) {
      password = line.substring(9);
      password.trim();
    } else if (inWifi && line.startsWith("[")) {
      inWifi = false; // другая секция
    }
  }
  f.close();
  return (ssid.length() > 0 && password.length() > 0);
}

// Подключение к WiFi или создание AP
void connectWiFi() {
  String ssid, password;
  bool gotFromSD = false;

  // 1. Пытаемся прочитать SETUP.INI
  if (initSD()) {
    if (readWiFiFromSD(ssid, password)) {
      gotFromSD = true;
      Serial.printf("WiFi from SD: %s / %s\n", ssid.c_str(), password.c_str());
    }
  }

  // 2. Fallback, если не удалось или SD нет
  if (!gotFromSD) {
    ssid = "xopkland";
    password = "1234567890987654321";
    Serial.println("Using fallback WiFi credentials");
  }

  // 3. Пытаемся подключиться
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    blinkLED(1, 100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED, HIGH); // светим постоянно при успешном подключении
    return;
  }

  // 4. Создаём точку доступа
  Serial.println("\nStarting AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("sd-card-3dp", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  digitalWrite(LED, HIGH);
}

// ============================================================================
// ОБРАБОТЧИКИ HTTP
// ============================================================================

void handleRoot() {
  // Проверяем наличие index.html на SD
  if (SD.exists("/index.html")) {
    File f = SD.open("/index.html", FILE_READ);
    if (f) {
      server.send(200, "text/html", f.readString());
      f.close();
      return;
    }
  }
  // Иначе отправляем встроенный (десктоп)
  server.send(200, "text/html", FPSTR(index_html));
}

void handleMobileRoot() {
  if (SD.exists("/index_m.html")) {
    File f = SD.open("/index_m.html", FILE_READ);
    if (f) {
      server.send(200, "text/html", f.readString());
      f.close();
      return;
    }
  }
  server.send(200, "text/html", FPSTR(index_m_html));
}

void handleList() {
  String path = server.arg("path");
  if (path.isEmpty()) path = "/";
  if (!path.startsWith("/")) path = "/" + path;

  if (isSDBusy()) {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"SD bus busy\"}");
    return;
  }

  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"Directory not found\"}");
    return;
  }

  StaticJsonDocument<4096> doc;
  doc["success"] = true;
  JsonArray arr = doc.createNestedArray("files");

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    JsonObject obj = arr.createNestedObject();
    obj["name"] = String(entry.name());
    obj["isDir"] = entry.isDirectory();
    obj["size"] = entry.size();
    // Можно добавить время модификации, но не обязательно
    entry.close();
  }
  dir.close();

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleDownload() {
  String filePath = server.arg("file");
  if (filePath.isEmpty()) {
    server.send(400, "text/plain", "Missing file parameter");
    return;
  }
  if (!filePath.startsWith("/")) filePath = "/" + filePath;

  if (isSDBusy()) {
    server.send(503, "text/plain", "SD bus busy");
    return;
  }

  if (!SD.exists(filePath)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File f = SD.open(filePath, FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "Cannot open file");
    return;
  }

  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + String(f.name()) + "\"");
  server.streamFile(f, "application/octet-stream");
  f.close();
}

void handleUpload() {
  // Загрузка файла через multipart/form-data
  // Используем глобальные переменные uploadFile и uploadPath
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String path = server.arg("path");
    if (path.isEmpty()) path = "/";
    if (!path.startsWith("/")) path = "/" + path;
    uploadPath = path;
    String filename = upload.filename;
    // Защита от path traversal
    filename.replace("..", "");
    filename.replace("/", "");
    String fullPath = uploadPath;
    if (!fullPath.endsWith("/")) fullPath += "/";
    fullPath += filename;

    if (isSDBusy()) {
      server.send(503, "text/plain", "SD bus busy");
      uploadFile = File();
      return;
    }

    uploadFile = SD.open(fullPath, FILE_WRITE);
    if (!uploadFile) {
      server.send(500, "text/plain", "Cannot create file");
      uploadFile = File();
    }
    uploadSuccess = false;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      uploadSuccess = true;
    }
    server.send(200, "text/plain", "Upload successful");
  } else {
    if (uploadFile) uploadFile.close();
    server.send(500, "text/plain", "Upload error");
  }
}

// Вспомогательная функция для парсинга JSON тела
bool parseJsonBody(StaticJsonDocument<256> &doc) {
  if (!server.hasArg("plain")) return false;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  return !err;
}

void handleDelete() {
  StaticJsonDocument<256> doc;
  if (!parseJsonBody(doc)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  const char* path = doc["path"];
  if (!path) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing path\"}");
    return;
  }
  String p = String(path);
  if (!p.startsWith("/")) p = "/" + p;

  if (isSDBusy()) {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"SD bus busy\"}");
    return;
  }

  if (!SD.exists(p)) {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"File not found\"}");
    return;
  }

  bool ok = SD.remove(p);
  if (!ok) {
    // Попробуем удалить как папку (пустую)
    ok = SD.rmdir(p);
  }
  server.send(200, "application/json", ok ? "{\"success\":true}" : "{\"success\":false,\"message\":\"Delete failed\"}");
}

void handleRename() {
  StaticJsonDocument<256> doc;
  if (!parseJsonBody(doc)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  const char* oldPath = doc["oldPath"];
  const char* newPath = doc["newPath"];
  if (!oldPath || !newPath) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing paths\"}");
    return;
  }
  String oldP = String(oldPath), newP = String(newPath);
  if (!oldP.startsWith("/")) oldP = "/" + oldP;
  if (!newP.startsWith("/")) newP = "/" + newP;

  if (isSDBusy()) {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"SD bus busy\"}");
    return;
  }

  if (!SD.exists(oldP)) {
    server.send(404, "application/json", "{\"success\":false,\"message\":\"Source not found\"}");
    return;
  }

  bool ok = SD.rename(oldP.c_str(), newP.c_str());
  server.send(200, "application/json", ok ? "{\"success\":true}" : "{\"success\":false,\"message\":\"Rename failed\"}");
}

void handleMove() {
  // Перемещение аналогично переименованию (если на одном томе)
  // Используем rename, но если между разными томами – не поддерживается.
  // Упрощённо: используем rename.
  handleRename(); // переименование и перемещение – одна операция в FAT
}

void handleMkdir() {
  StaticJsonDocument<256> doc;
  if (!parseJsonBody(doc)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  const char* path = doc["path"];
  if (!path) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing path\"}");
    return;
  }
  String p = String(path);
  if (!p.startsWith("/")) p = "/" + p;

  if (isSDBusy()) {
    server.send(503, "application/json", "{\"success\":false,\"message\":\"SD bus busy\"}");
    return;
  }

  bool ok = SD.mkdir(p);
  server.send(200, "application/json", ok ? "{\"success\":true}" : "{\"success\":false,\"message\":\"Mkdir failed\"}");
}

String getThumbnailPath(const String& gcodePath) {
  // Ищем файл с тем же именем, но .jpg или .png
  String base = gcodePath.substring(0, gcodePath.lastIndexOf('.'));
  String jpg = base + ".jpg";
  String png = base + ".png";
  if (SD.exists(jpg)) return jpg;
  if (SD.exists(png)) return png;
  // Ищем логотип в корне
  if (SD.exists("/logo.jpg")) return "/logo.jpg";
  if (SD.exists("/logo.png")) return "/logo.png";
  return ""; // нет миниатюры
}

void sendThumbnailPlaceholder() {
  // Отдаём серый SVG
  server.send(200, "image/svg+xml", "<svg xmlns='http://www.w3.org/2000/svg' width='300' height='300'><rect width='300' height='300' fill='#555'/></svg>");
}

void handleThumbnail() {
  String file = server.arg("file");
  if (file.isEmpty()) {
    sendThumbnailPlaceholder();
    return;
  }
  if (!file.startsWith("/")) file = "/" + file;

  if (isSDBusy()) {
    sendThumbnailPlaceholder();
    return;
  }

  // Проверяем, существует ли сам файл (для проверки)
  if (!SD.exists(file)) {
    sendThumbnailPlaceholder();
    return;
  }

  String thumbPath = getThumbnailPath(file);
  if (thumbPath.isEmpty() || !SD.exists(thumbPath)) {
    sendThumbnailPlaceholder();
    return;
  }

  File f = SD.open(thumbPath, FILE_READ);
  if (!f) {
    sendThumbnailPlaceholder();
    return;
  }

  // Определяем MIME по расширению
  String contentType = "image/jpeg";
  if (thumbPath.endsWith(".png")) contentType = "image/png";

  server.sendHeader("Content-Type", contentType);
  server.streamFile(f, contentType);
  f.close();
}

void handleNotFound() {
  // Если запрошен файл с SD (например, картинка) – разрешим?
  // Но для безопасности не разрешаем прямой доступ к SD, только через API.
  // Отдаём 404.
  server.send(404, "text/plain", "Not Found");
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== simplefmfystec ===\n");

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(CS_SENSE, INPUT_PULLUP);

  // Инициализация SD и подключение WiFi
  connectWiFi();

  // Настройка маршрутов
  server.on("/", HTTP_GET, []() {
    // Определяем мобильный User-Agent
    String ua = server.header("User-Agent");
    if (ua.indexOf("Mobile") != -1 || ua.indexOf("Android") != -1 || ua.indexOf("iPhone") != -1) {
      handleMobileRoot();
    } else {
      handleRoot();
    }
  });
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/index_m.html", HTTP_GET, handleMobileRoot);
  server.on("/list", HTTP_GET, handleList);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/upload", HTTP_POST, [](){ server.send(200, "text/plain", "Upload finished"); }, handleUpload);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/rename", HTTP_POST, handleRename);
  server.on("/move", HTTP_POST, handleMove);
  server.on("/mkdir", HTTP_POST, handleMkdir);
  server.on("/thumbnail", HTTP_GET, handleThumbnail);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  // Можно добавить мигание LED при активности SD, но не обязательно.
}
