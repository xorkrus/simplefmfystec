// html.h
#ifndef HTML_H
#define HTML_H

// Встроенная fallback страница для desktop
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD Card Manager</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 10px; display: flex; flex-direction: column; height: 100vh; box-sizing: border-box; }
        .container { display: flex; flex: 1; gap: 10px; }
        .file-manager { flex: 2; border: 1px solid #ccc; border-radius: 5px; padding: 10px; overflow-y: auto; }
        .preview { flex: 1; border: 1px solid #ccc; border-radius: 5px; padding: 10px; display: flex; align-items: center; justify-content: center; }
        .preview img { max-width: 100%; max-height: 100%; }
        .toolbar { display: flex; gap: 5px; margin-bottom: 10px; flex-wrap: wrap; }
        .toolbar button, .toolbar input[type=file] { padding: 5px 10px; }
        ul { list-style: none; padding: 0; }
        li { display: flex; align-items: center; padding: 5px; border-bottom: 1px solid #eee; cursor: pointer; }
        li:hover { background: #f5f5f5; }
        .icon { margin-right: 5px; }
        .file-name { flex: 1; }
        .file-size { margin-right: 10px; color: #666; }
        .actions { display: flex; gap: 5px; }
        .actions button { padding: 2px 5px; font-size: 0.8em; }
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); align-items: center; justify-content: center; }
        .modal-content { background: white; padding: 20px; border-radius: 5px; width: 300px; }
        .modal input { width: 100%; padding: 5px; margin: 5px 0; box-sizing: border-box; }
        .modal button { margin-top: 10px; }
        #progress { display: none; margin-top: 10px; }
        #progress-bar { height: 20px; background: #4CAF50; width: 0; }
    </style>
</head>
<body>
    <div class="container">
        <div class="file-manager">
            <div class="toolbar">
                <button onclick="goUp()">⬆️ Вверх</button>
                <button onclick="refresh()">🔄 Обновить</button>
                <button onclick="showMkdir()">📁 Создать папку</button>
                <input type="file" id="fileInput" multiple onchange="uploadFiles()">
                <label for="fileInput">📤 Загрузить файлы</label>
                <button onclick="showRename()">✏️ Переименовать</button>
                <button onclick="deleteSelected()">🗑 Удалить</button>
            </div>
            <div id="currentPath">/</div>
            <ul id="fileList"></ul>
            <div id="progress">
                <div id="progress-bar"></div>
            </div>
        </div>
        <div class="preview" id="preview">
            <span>Выберите .gcode файл для предпросмотра</span>
        </div>
    </div>

    <!-- Модальное окно для ввода имени -->
    <div class="modal" id="modal">
        <div class="modal-content">
            <h3 id="modalTitle"></h3>
            <input type="text" id="modalInput">
            <button onclick="modalOk()">OK</button>
            <button onclick="modalCancel()">Отмена</button>
        </div>
    </div>

    <script>
        let currentPath = '/';
        let selectedFile = null;
        let modalCallback = null;

        function refresh() {
            fetch('/api/list?path=' + encodeURIComponent(currentPath))
                .then(r => r.json())
                .then(data => {
                    currentPath = data.path;
                    document.getElementById('currentPath').textContent = currentPath;
                    renderFiles(data.files);
                });
        }

        function renderFiles(files) {
            const list = document.getElementById('fileList');
            list.innerHTML = '';
            files.forEach(file => {
                const li = document.createElement('li');
                li.onclick = () => selectFile(file);
                const icon = file.isDir ? '📁' : '📄';
                li.innerHTML = `<span class="icon">${icon}</span><span class="file-name">${file.name}</span><span class="file-size">${file.isDir ? '' : formatSize(file.size)}</span>`;
                if (!file.isDir && file.name.endsWith('.gcode')) {
                    li.style.cursor = 'pointer';
                    li.addEventListener('click', () => showPreview(file.path));
                }
                list.appendChild(li);
            });
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }

        function selectFile(file) {
            selectedFile = file;
        }

        function goUp() {
            if (currentPath === '/') return;
            const parts = currentPath.split('/').filter(p => p);
            parts.pop();
            currentPath = '/' + parts.join('/');
            if (currentPath === '') currentPath = '/';
            refresh();
        }

        function showPreview(path) {
            const previewDiv = document.getElementById('preview');
            fetch('/api/thumbnail?path=' + encodeURIComponent(path))
                .then(r => {
                    if (r.ok) {
                        return r.blob().then(blob => {
                            const url = URL.createObjectURL(blob);
                            previewDiv.innerHTML = `<img src="${url}">`;
                        });
                    } else {
                        previewDiv.innerHTML = '<span>Нет миниатюры</span>';
                    }
                });
        }

        function uploadFiles() {
            const files = document.getElementById('fileInput').files;
            if (!files.length) return;
            const progressDiv = document.getElementById('progress');
            const progressBar = document.getElementById('progress-bar');
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';

            let completed = 0;
            const total = files.length;
            const uploadNext = (index) => {
                if (index >= files.length) {
                    progressDiv.style.display = 'none';
                    refresh();
                    return;
                }
                const file = files[index];
                const formData = new FormData();
                formData.append('file', file);
                const xhr = new XMLHttpRequest();
                xhr.open('POST', '/api/upload?path=' + encodeURIComponent(currentPath), true);
                xhr.upload.onprogress = (e) => {
                    if (e.lengthComputable) {
                        const percent = (completed + (e.loaded / e.total)) / total * 100;
                        progressBar.style.width = percent + '%';
                    }
                };
                xhr.onload = () => {
                    if (xhr.status === 200) {
                        completed++;
                        uploadNext(index + 1);
                    } else {
                        alert('Ошибка загрузки ' + file.name);
                        progressDiv.style.display = 'none';
                    }
                };
                xhr.onerror = () => {
                    alert('Ошибка сети');
                    progressDiv.style.display = 'none';
                };
                xhr.send(formData);
            };
            uploadNext(0);
        }

        function showMkdir() {
            modalCallback = (name) => {
                fetch('/api/mkdir', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({path: currentPath + '/' + name})
                }).then(() => refresh());
            };
            document.getElementById('modalTitle').textContent = 'Название папки';
            document.getElementById('modalInput').value = '';
            document.getElementById('modal').style.display = 'flex';
        }

        function showRename() {
            if (!selectedFile) { alert('Выберите файл или папку'); return; }
            modalCallback = (newName) => {
                fetch('/api/rename', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({path: selectedFile.path, newName: newName})
                }).then(() => refresh());
            };
            document.getElementById('modalTitle').textContent = 'Новое имя';
            document.getElementById('modalInput').value = selectedFile.name;
            document.getElementById('modal').style.display = 'flex';
        }

        function deleteSelected() {
            if (!selectedFile) { alert('Выберите файл или папку'); return; }
            if (!confirm('Удалить ' + selectedFile.name + '?')) return;
            fetch('/api/delete', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({path: selectedFile.path})
            }).then(() => refresh());
        }

        function modalOk() {
            const value = document.getElementById('modalInput').value.trim();
            if (value && modalCallback) modalCallback(value);
            document.getElementById('modal').style.display = 'none';
        }

        function modalCancel() {
            document.getElementById('modal').style.display = 'none';
        }

        refresh();
    </script>
</body>
</html>
)rawliteral";

// Встроенная fallback страница для mobile (адаптирована под вертикальное расположение)
const char INDEX_M_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD Card Manager Mobile</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 10px; display: flex; flex-direction: column; height: 100vh; box-sizing: border-box; }
        .preview { height: 40vh; border: 1px solid #ccc; border-radius: 5px; padding: 10px; display: flex; align-items: center; justify-content: center; overflow: hidden; }
        .preview img { max-width: 100%; max-height: 100%; }
        .file-manager { flex: 1; border: 1px solid #ccc; border-radius: 5px; padding: 10px; overflow-y: auto; }
        .toolbar { display: flex; gap: 5px; margin-bottom: 10px; flex-wrap: wrap; }
        .toolbar button, .toolbar input[type=file] { padding: 5px 10px; }
        ul { list-style: none; padding: 0; }
        li { display: flex; align-items: center; padding: 5px; border-bottom: 1px solid #eee; cursor: pointer; }
        li:hover { background: #f5f5f5; }
        .icon { margin-right: 5px; }
        .file-name { flex: 1; }
        .file-size { margin-right: 10px; color: #666; }
        .actions { display: flex; gap: 5px; }
        .actions button { padding: 2px 5px; font-size: 0.8em; }
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); align-items: center; justify-content: center; }
        .modal-content { background: white; padding: 20px; border-radius: 5px; width: 90%; max-width: 300px; }
        .modal input { width: 100%; padding: 5px; margin: 5px 0; box-sizing: border-box; }
        .modal button { margin-top: 10px; }
        #progress { display: none; margin-top: 10px; }
        #progress-bar { height: 20px; background: #4CAF50; width: 0; }
    </style>
</head>
<body>
    <div class="preview" id="preview">
        <span>Выберите .gcode файл</span>
    </div>
    <div class="file-manager">
        <div class="toolbar">
            <button onclick="goUp()">⬆️</button>
            <button onclick="refresh()">🔄</button>
            <button onclick="showMkdir()">📁</button>
            <input type="file" id="fileInput" multiple onchange="uploadFiles()">
            <label for="fileInput">📤</label>
            <button onclick="showRename()">✏️</button>
            <button onclick="deleteSelected()">🗑</button>
        </div>
        <div id="currentPath">/</div>
        <ul id="fileList"></ul>
        <div id="progress">
            <div id="progress-bar"></div>
        </div>
    </div>

    <div class="modal" id="modal">
        <div class="modal-content">
            <h3 id="modalTitle"></h3>
            <input type="text" id="modalInput">
            <button onclick="modalOk()">OK</button>
            <button onclick="modalCancel()">Отмена</button>
        </div>
    </div>

    <script>
        // Аналогичный JS как в desktop версии, но адаптированный
        let currentPath = '/';
        let selectedFile = null;
        let modalCallback = null;

        function refresh() {
            fetch('/api/list?path=' + encodeURIComponent(currentPath))
                .then(r => r.json())
                .then(data => {
                    currentPath = data.path;
                    document.getElementById('currentPath').textContent = currentPath;
                    renderFiles(data.files);
                });
        }

        function renderFiles(files) {
            const list = document.getElementById('fileList');
            list.innerHTML = '';
            files.forEach(file => {
                const li = document.createElement('li');
                li.onclick = () => selectFile(file);
                const icon = file.isDir ? '📁' : '📄';
                li.innerHTML = `<span class="icon">${icon}</span><span class="file-name">${file.name}</span><span class="file-size">${file.isDir ? '' : formatSize(file.size)}</span>`;
                if (!file.isDir && file.name.endsWith('.gcode')) {
                    li.addEventListener('click', () => showPreview(file.path));
                }
                list.appendChild(li);
            });
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }

        function selectFile(file) {
            selectedFile = file;
        }

        function goUp() {
            if (currentPath === '/') return;
            const parts = currentPath.split('/').filter(p => p);
            parts.pop();
            currentPath = '/' + parts.join('/');
            if (currentPath === '') currentPath = '/';
            refresh();
        }

        function showPreview(path) {
            const previewDiv = document.getElementById('preview');
            fetch('/api/thumbnail?path=' + encodeURIComponent(path))
                .then(r => {
                    if (r.ok) {
                        return r.blob().then(blob => {
                            const url = URL.createObjectURL(blob);
                            previewDiv.innerHTML = `<img src="${url}">`;
                        });
                    } else {
                        previewDiv.innerHTML = '<span>Нет миниатюры</span>';
                    }
                });
        }

        function uploadFiles() {
            const files = document.getElementById('fileInput').files;
            if (!files.length) return;
            const progressDiv = document.getElementById('progress');
            const progressBar = document.getElementById('progress-bar');
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';

            let completed = 0;
            const total = files.length;
            const uploadNext = (index) => {
                if (index >= files.length) {
                    progressDiv.style.display = 'none';
                    refresh();
                    return;
                }
                const file = files[index];
                const formData = new FormData();
                formData.append('file', file);
                const xhr = new XMLHttpRequest();
                xhr.open('POST', '/api/upload?path=' + encodeURIComponent(currentPath), true);
                xhr.upload.onprogress = (e) => {
                    if (e.lengthComputable) {
                        const percent = (completed + (e.loaded / e.total)) / total * 100;
                        progressBar.style.width = percent + '%';
                    }
                };
                xhr.onload = () => {
                    if (xhr.status === 200) {
                        completed++;
                        uploadNext(index + 1);
                    } else {
                        alert('Ошибка загрузки');
                        progressDiv.style.display = 'none';
                    }
                };
                xhr.onerror = () => {
                    alert('Ошибка сети');
                    progressDiv.style.display = 'none';
                };
                xhr.send(formData);
            };
            uploadNext(0);
        }

        function showMkdir() {
            modalCallback = (name) => {
                fetch('/api/mkdir', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({path: currentPath + '/' + name})
                }).then(() => refresh());
            };
            document.getElementById('modalTitle').textContent = 'Название папки';
            document.getElementById('modalInput').value = '';
            document.getElementById('modal').style.display = 'flex';
        }

        function showRename() {
            if (!selectedFile) { alert('Выберите файл или папку'); return; }
            modalCallback = (newName) => {
                fetch('/api/rename', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({path: selectedFile.path, newName: newName})
                }).then(() => refresh());
            };
            document.getElementById('modalTitle').textContent = 'Новое имя';
            document.getElementById('modalInput').value = selectedFile.name;
            document.getElementById('modal').style.display = 'flex';
        }

        function deleteSelected() {
            if (!selectedFile) { alert('Выберите файл или папку'); return; }
            if (!confirm('Удалить ' + selectedFile.name + '?')) return;
            fetch('/api/delete', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({path: selectedFile.path})
            }).then(() => refresh());
        }

        function modalOk() {
            const value = document.getElementById('modalInput').value.trim();
            if (value && modalCallback) modalCallback(value);
            document.getElementById('modal').style.display = 'none';
        }

        function modalCancel() {
            document.getElementById('modal').style.display = 'none';
        }

        refresh();
    </script>
</body>
</html>
)rawliteral";

#endif
