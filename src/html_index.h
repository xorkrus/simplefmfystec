#ifndef HTML_INDEX_H
#define HTML_INDEX_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SD WiFi File Manager</title>
<style>
    body { font-family: Arial, sans-serif; margin:0; padding:10px; display:flex; flex-wrap:wrap; }
    #filemanager { flex:2; min-width:300px; margin-right:20px; }
    #preview { flex:1; min-width:200px; border:1px solid #ccc; text-align:center; padding:10px; }
    .file-entry { cursor:pointer; padding:5px; border-bottom:1px solid #eee; display:flex; justify-content:space-between; }
    .file-entry:hover { background:#f0f0f0; }
    .dir { font-weight:bold; }
    .controls { margin-bottom:10px; }
    .controls button { margin-right:5px; }
    #dropzone { border:2px dashed #aaa; padding:20px; text-align:center; margin:10px 0; }
    #progress { width:100%; height:20px; background:#ddd; display:none; }
    #progressbar { height:100%; width:0%; background:#4caf50; }
</style>
</head>
<body>
<div id="filemanager">
    <h2>File Manager</h2>
    <div class="controls">
        <button onclick="refresh()">Refresh</button>
        <button onclick="createFolder()">New Folder</button>
        <label>Upload: <input type="file" id="fileInput" multiple onchange="uploadFiles(this.files)"></label>
    </div>
    <div id="dropzone" ondragover="event.preventDefault();" ondrop="handleDrop(event)">
        Drag & drop files here
    </div>
    <div id="progress"><div id="progressbar"></div></div>
    <div id="filelist"></div>
</div>
<div id="preview">
    <h3>Preview</h3>
    <div id="thumbnail"><img id="thumbImg" src="" alt="No preview" style="max-width:100%;"></div>
    <div id="fileInfo"></div>
</div>
<script>
    let currentPath = '/';
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
            // Файл - показать превью
            document.getElementById('thumbImg').src = '/api/thumbnail?path=' + encodeURIComponent(path);
        } else {
            // Папка - перейти
            loadFiles(path);
        }
    }
    function refresh() { loadFiles(currentPath); }
    function createFolder() {
        let name = prompt('Enter folder name:');
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
    // Инициализация
    loadFiles('/');
</script>
</body>
</html>
)rawliteral";

#endif
