#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SdFat.h>

// Pin definitions for FYSETC board
#define SD_CS 4
#define MISO_PIN 12
#define MOSI_PIN 13
#define SCLK_PIN 14
#define CS_SENSE 5
#define LED_PIN 2

// Объект для работы с SD через SdFat
SdFat sd;

// Server
ESP8266WebServer server(HTTP_PORT);

// Custom index.html flag
bool hasCustomIndex = false;

// Base64 encoded 1x1 gray pixel for C++ redirect
const char placeholderImg[] PROGMEM = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";

// ==========================================
// FALLBACK WEB INTERFACE
// ==========================================
const char fallback_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>SD File Manager</title>
<style>
body{font-family:sans-serif;background:#f4f4f4;margin:0;padding:20px}
.toolbar{background:#fff;padding:15px;border-radius:8px;margin-bottom:20px;display:flex;gap:10px;align-items:center;box-shadow:0 2px 5px rgba(0,0,0,0.1)}
button{padding:8px 15px;border:none;border-radius:5px;cursor:pointer;background:#0275d8;color:#fff}
button:hover{background:#025aa5}
input[type=text]{padding:8px;border:1px solid #ccc;border-radius:5px;flex:1}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:20px}
.card{background:#fff;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);overflow:hidden;position:relative}
.card img{width:100%;height:200px;object-fit:cover;background:#eee;display:block}
.card .info{padding:10px;word-break:break-all;font-size:14px}
.card .actions{position:absolute;top:5px;right:5px;display:none;background:rgba(255,255,255,0.8);padding:5px;border-radius:5px}
.card:hover .actions{display:block}
.actions button{background:#dc3545;padding:5px;margin:2px;font-size:12px}
.dropzone{border:2px dashed #ccc;border-radius:8px;padding:40px;text-align:center;color:#666;margin-bottom:20px;display:none}
.progress{width:100%;background:#ddd;height:10px;border-radius:5px;margin-top:10px;display:none}
.progress div{height:100%;background:#0275d8;width:0%;transition:width 0.3s}
.modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:100;align-items:center;justify-content:center}
.modal-content{background:#fff;padding:20px;border-radius:8px;min-width:300px}
.dir-link{color:#0275d8;text-decoration:none;cursor:pointer;font-weight:bold;display:inline-block;margin-bottom:10px}
</style>
</head><body>
<div class="toolbar">
  <span class="dir-link" onclick="navigate('/')" style="margin-right:10px">/</span>
  <span id="breadcrumb" class="dir-link"></span>
  <div style="flex:1"></div>
  <button onclick="showModal('mkdir')">Создать папку</button>
  <button onclick="document.getElementById('fileInput').click()">Загрузить файл</button>
  <input type="file" id="fileInput" multiple style="display:none" onchange="uploadFiles(this.files)">
</div>
<div id="dropzone" class="dropzone" ondrop="dropHandler(event)" ondragover="event.preventDefault()" ondragleave="this.style.display='none'">Перетащите файлы сюда для загрузки</div>
<div class="progress" id="progressBar"><div id="progress"></div></div>
<div class="grid" id="fileGrid"></div>

<div id="modal" class="modal">
  <div class="modal-content">
    <h3 id="modalTitle">Действие</h3>
    <input type="hidden" id="modalAction">
    <input type="hidden" id="modalOldPath">
    <input type="text" id="modalInput" style="width:100%;margin:10px 0;box-sizing:border-box">
    <div style="text-align:right">
      <button onclick="closeModal()" style="background:#ccc;color:#000">Отмена</button>
      <button onclick="submitModal()">Ок</button>
    </div>
  </div>
</div>

<script>
let currentPath = '/';
const placeholder = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==';

document.addEventListener('dragover', e => { e.preventDefault(); document.getElementById('dropzone').style.display='block'; });
document.addEventListener('drop', e => { e.preventDefault(); document.getElementById('dropzone').style.display='none'; if(e.dataTransfer.files.length) uploadFiles(e.dataTransfer.files); });

function loadFiles() {
  document.getElementById('breadcrumb').innerText = currentPath !== '/' ? currentPath : '';
  fetch('/api/list?dir=' + currentPath).then(r=>r.json()).then(files => {
    const grid = document.getElementById('fileGrid');
    grid.innerHTML = '';
    files.sort((a,b) => (a.isDir === b.isDir) ? a.name.localeCompare(b.name) : a.isDir ? -1 : 1 );
    
    files.forEach(f => {
      const fullPath = currentPath === '/' ? '/' + f.name : currentPath + '/' + f.name;
      const card = document.createElement('div');
      card.className = 'card';
      
      let imgSrc = placeholder;
      if (!f.isDir) {
         if (f.name.toLowerCase().endsWith('.gcode')) imgSrc = '/thumb?path=' + fullPath;
         else if (f.name.toLowerCase().match(/\.(jpg|jpeg|png|gif|bmp)$/)) imgSrc = '/api/download?path=' + fullPath;
      } else {
         imgSrc = placeholder;
      }

      card.innerHTML = `
        <img src="${imgSrc}" onerror="this.src='${placeholder}'">
        <div class="info">
          <div style="font-weight:bold;cursor:pointer" onclick="${f.isDir ? `navigate('${fullPath}')` : `window.open('/api/download?path=${fullPath}')`}">${f.name}</div>
          <div style="font-size:12px;color:#666">${f.isDir ? 'Папка' : (f.size/1024).toFixed(2)+' KB'}</div>
        </div>
        <div class="actions">
          ${!f.isDir ? `<button onclick="window.open('/api/download?path=${fullPath}')">⬇</button>` : ''}
          <button onclick="showModal('rename', '${fullPath}', '${f.name}')">✏️</button>
          <button onclick="showModal('move', '${fullPath}', '${f.name}')">📦</button>
          <button onclick="apiAction('delete', '${fullPath}')">🗑</button>
        </div>`;
      grid.appendChild(card);
    });
  });
}

function navigate(path) { currentPath = path; loadFiles(); }

function uploadFiles(files) {
  const pb = document.getElementById('progressBar');
  const p = document.getElementById('progress');
  pb.style.display = 'block'; p.style.width = '0%';
  let completed = 0;
  for(let i=0; i<files.length; i++) {
    let xhr = new XMLHttpRequest(); let fd = new FormData(); fd.append('file', files[i]);
    xhr.open('POST', '/api/upload?path=' + currentPath, true);
    xhr.upload.onprogress = e => { if(e.lengthComputable) p.style.width = ((completed + e.loaded/e.total) / files.length * 100) + '%'; };
    xhr.onload = () => { completed++; p.style.width = (completed / files.length * 100) + '%'; if(completed === files.length) { setTimeout(() => { pb.style.display='none'; loadFiles(); }, 500); } };
    xhr.send(fd);
  }
}

function apiAction(action, path1, path2) {
  fetch('/api/action?action=' + action + '&path1=' + encodeURIComponent(path1) + '&path2=' + encodeURIComponent(path2||''), {method:'POST'})
  .then(r => { if(r.ok) loadFiles(); else alert('Ошибка'); });
}

function showModal(type, oldPath, name) {
  document.getElementById('modal').style.display = 'flex';
  document.getElementById('modalAction').value = type;
  document.getElementById('modalOldPath').value = oldPath || '';
  const inp = document.getElementById('modalInput');
  if(type === 'mkdir') { document.getElementById('modalTitle').innerText = 'Новая папка'; inp.value = ''; }
  else if(type === 'rename') { document.getElementById('modalTitle').innerText = 'Переименовать'; inp.value = name; }
  else if(type === 'move') { document.getElementById('modalTitle').innerText = 'Переместить в (полный путь)'; inp.value = currentPath; }
}

function closeModal() { document.getElementById('modal').style.display = 'none'; }

function submitModal() {
  const action = document.getElementById('modalAction').value;
  const oldPath = document.getElementById('modalOldPath').value;
  const val = document.getElementById('modalInput').value;
  if(!val) return;
  if(action === 'mkdir') { let newPath = currentPath === '/' ? '/' + val : currentPath + '/' + val; apiAction('mkdir', newPath); }
  else if(action === 'rename') { let newPath = currentPath === '/' ? '/' + val : currentPath + '/' + val; apiAction('rename', oldPath, newPath); }
  else if(action === 'move') { let fileName = oldPath.split('/').pop(); let newPath = val.endsWith('/') ? val + fileName : val + '/' + fileName; apiAction('move', oldPath, newPath); }
  closeModal();
}
loadFiles();
</script>
</body></html>
)rawliteral";


void parseSetupIni(String &ssid, String &pass) {
  FsFile f = sd.open("/SETUP.INI", O_READ);
  if (!f) return;
  
  bool inWifi = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.equalsIgnoreCase("[WIFI]")) { inWifi = true; continue; }
    if (line.startsWith("[")) inWifi = false;
    
    if (inWifi) {
      if (line.startsWith("SSID=")) ssid = line.substring(5);
      if (line.startsWith("PASSWORD=")) pass = line.substring(9);
    }
  }
  f.close();
}

void handleThumbnail() {
  String path = server.arg("path");
  if (path.endsWith(".gcode") || path.endsWith(".GCODE")) {
    String thumbPath = path.substring(0, path.lastIndexOf('.')) + ".jpg";
    if (sd.exists(thumbPath)) {
      FsFile f = sd.open(thumbPath, O_READ);
      server.stream(f, "image/jpeg", HTTP_GET, f.size());
      f.close(); return;
    }
    thumbPath = path.substring(0, path.lastIndexOf('.')) + ".png";
    if (sd.exists(thumbPath)) {
      FsFile f = sd.open(thumbPath, O_READ);
      server.stream(f, "image/png", HTTP_GET, f.size());
      f.close(); return;
    }
  }
  
  if (sd.exists("/logo.jpg")) {
    FsFile f = sd.open("/logo.jpg", O_READ);
    server.stream(f, "image/jpeg", HTTP_GET, f.size());
    f.close(); return;
  }
  if (sd.exists("/logo.png")) {
    FsFile f = sd.open("/logo.png", O_READ);
    server.stream(f, "image/png", HTTP_GET, f.size());
    f.close(); return;
  }
  
  server.sendHeader("Location", FPSTR(placeholderImg), true);
  server.send(302, "text/plain", "");
}

void handleList() {
  String dir = server.arg("dir");
  if (dir == "") dir = "/";
  if (!dir.startsWith("/")) dir = "/" + dir;
  
  FsFile d = sd.open(dir, O_READ);
  if (!d || !d.isDirectory()) {
    if(d) d.close();
    server.send(400, "application/json", "{\"error\":\"Dir not found\"}");
    return;
  }

  String json = "[";
  FsFile f = d.openNextFile();
  while (f) {
    if (json != "[") json += ",";
    
    // Правильный способ получить имя файла в новой версии SdFat
    char fname[256];
    f.getName(fname, sizeof(fname));
    
    json += "{\"name\":\"" + String(fname) + "\",\"size\":" + String(f.size()) + ",\"isDir\":" + String(f.isDirectory() ? "true" : "false") + "}";
    f = d.openNextFile();
  }
  f.close();
  d.close();
  json += "]";
  server.send(200, "application/json", json);
}

void handleFileAction() {
  String action = server.arg("action");
  String path1 = server.arg("path1");
  String path2 = server.arg("path2");

  if (action == "delete") {
    if (sd.exists(path1)) { sd.remove(path1); server.send(200, "text/plain", "OK"); }
    else server.send(404, "text/plain", "Not found");
  } 
  else if (action == "mkdir") {
    if (sd.mkdir(path1)) server.send(200, "text/plain", "OK");
    else server.send(500, "text/plain", "Failed");
  }
  else if (action == "rename" || action == "move") {
    if (sd.exists(path1) && sd.rename(path1, path2)) server.send(200, "text/plain", "OK");
    else server.send(500, "text/plain", "Failed");
  } else {
    server.send(400, "text/plain", "Unknown action");
  }
}

void handleDownload() {
  String path = server.arg("path");
  if (!sd.exists(path)) { server.send(404, "text/plain", "Not found"); return; }
  
  FsFile download = sd.open(path, O_READ);
  String contentType = "application/octet-stream";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
  else if (path.endsWith(".png")) contentType = "image/png";
  
  server.stream(download, contentType, HTTP_GET, download.size());
  download.close();
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  static FsFile uploadFile;
  String path = server.arg("path");
  if (path == "") path = "/";
  if (!path.endsWith("/")) path += "/";
  path += upload.filename;

  if (upload.status == UPLOAD_FILE_START) {
    uploadFile = sd.open(path, O_WRITE | O_CREAT);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  }
}

void handleRoot() {
  if (hasCustomIndex) {
    FsFile idx = sd.open("/index.html", O_READ);
    server.stream(idx, "text/html", HTTP_GET, idx.size());
    idx.close();
  } else {
    server.send_P(200, "text/html", fallback_html); 
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);

  // Инициализация SdFat
  if (!sd.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed!");
    while (true) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(100); }
  }

  hasCustomIndex = sd.exists("/index.html");

  String ssid = "sd-card-3dp";
  String pass = "12345678";
  parseSetupIni(ssid, pass);

  if (ssid != "sd-card-3dp") {
    WiFi.begin(ssid.c_str(), pass.c_str());
    int timeout = 20;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(500); Serial.print("."); timeout--;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP("sd-card-3dp", "12345678");
    Serial.println("\nStarted AP mode");
  } else {
    Serial.println("\nConnected to WiFi!");
  }
  
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/thumb", HTTP_GET, handleThumbnail);
  server.on("/api/list", HTTP_GET, handleList);
  server.on("/api/action", HTTP_POST, handleFileAction);
  server.on("/api/download", HTTP_GET, handleDownload);
  server.on("/api/upload", HTTP_POST, [](){ server.send(200, "text/plain", "OK"); }, handleUpload);
  
  server.begin();
}

void loop() {
  server.handleClient();
}
