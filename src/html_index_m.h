#ifndef HTML_INDEX_M_H
#define HTML_INDEX_M_H

const char INDEX_M_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>SD WiFi File Manager (Mobile)</title>
<style>
    body { font-family: Arial, sans-serif; margin:0; padding:5px; }
    #container { display:flex; flex-direction:column; height:100vh; }
    #preview { height:30%; border-bottom:1px solid #ccc; overflow:hidden; text-align:center; }
    #preview img { max-height:100%; max-width:100%; }
    #filemanager { flex:1; overflow-y:auto; }
    .controls { display:flex; flex-wrap:wrap; gap:5px; margin-bottom:5px; }
    .file-entry { padding:8px; border-bottom:1px solid #eee; display:flex; justify-content:space-between; }
    #dropzone { border:2px dashed #aaa; padding:10px; text-align:center; margin:5px 0; }
    #progress { width:100%; height:20px; background:#ddd; display:none; }
    #progressbar { height:100%; width:0%; background:#4caf50; }
</style>
</head>
<body>
<div id="container">
    <div id="preview">
        <img id="thumbImg" src="" alt="Preview">
    </div>
    <div id="filemanager">
        <div class="controls">
            <button onclick="refresh()">Обновить</button>
            <button onclick="createFolder()">Папка</button>
            <label>Загрузить: <input type="file" id="fileInput" multiple onchange="uploadFiles(this.files)"></label>
        </div>
        <div id="dropzone" ondragover="event.preventDefault();" ondrop="handleDrop(event)">
            Перетащите файлы сюда
        </div>
        <div id="progress"><div id="progressbar"></div></div>
        <div id="filelist"></div>
    </div>
</div>
<script>
    let currentPath = '/';
    // Аналогичные функции, как в десктопной версии
    function loadFiles(path) {
        if (!path) path = currentPath;
        fetch('/api/files?path=' + encodeURIComponent(path))
        .then(r => r.json())
        .then(data => {
            currentPath = data.path;
            let html = '';
            data.items.forEach(item => {
                let icon = item.type === 'dir' ? '📁' : '📄';
                html += `<div class="file-entry" onclick="clickFile('${item.name}')">
                    <span>${icon} ${item.name}</span>
                    <span>${item.size}</span>
                </div>`;
            });
            document.getElementById('filelist').innerHTML = html;
        });
    }
    function clickFile(name) {
        let path = currentPath + (currentPath.endsWith('/') ? '' : '/') + name;
        if (name.includes('.')) {
            document.getElementById('thumbImg').src = '/api/thumbnail?path=' + encodeURIComponent(path);
        } else {
            loadFiles(path);
        }
    }
    function refresh() { loadFiles(currentPath); }
    function createFolder() {
        let name = prompt('Имя папки:');
        if (!name) return;
        let path = currentPath + (currentPath.endsWith('/') ? '' : '/') + name;
        fetch('/api/mkdir', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path:path}) })
        .then(() => refresh());
    }
    function uploadFiles(files) {
        const formData = new FormData();
        for (let f of files) formData.append('file', f);
        let xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/upload');
        xhr.upload.onprogress = function(e) {
            if (e.lengthComputable) {
                let pct = (e.loaded / e.total) * 100;
                document.getElementById('progress').style.display = 'block';
                document.getElementById('progressbar').style.width = pct + '%';
            }
        };
        xhr.onload = function() {
            document.getElementById('progress').style.display = 'none';
            refresh();
        };
        xhr.send(formData);
    }
    function handleDrop(e) {
        e.preventDefault();
        let files = e.dataTransfer.files;
        uploadFiles(files);
    }
    loadFiles('/');
</script>
</body>
</html>
)rawliteral";

#endif
