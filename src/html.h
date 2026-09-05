// html.h
#ifndef HTML_H
#define HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD Manager</title>
    <style>
        :root {
            --bg: #f4f4f4;
            --text: #333;
            --border: #ccc;
            --hover: #e0e0e0;
            --danger: #d9534f;
            --primary: #0275d8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: system-ui, -apple-system, sans-serif;
            background: var(--bg);
            color: var(--text);
            padding: 10px;
            max-width: 1000px;
            margin: 0 auto;
        }
        .toolbar {
            display: flex;
            gap: 8px;
            margin-bottom: 10px;
            flex-wrap: wrap;
            align-items: center;
        }
        .toolbar button, .toolbar label {
            background: var(--primary);
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 0.9rem;
        }
        .toolbar button:hover { opacity: 0.9; }
        .toolbar input[type="file"] { display: none; }
        #currentPath {
            background: white;
            padding: 6px 10px;
            border: 1px solid var(--border);
            border-radius: 4px;
            margin-bottom: 10px;
            font-family: monospace;
            word-break: break-all;
        }
        ul { list-style: none; }
        li {
            display: flex;
            align-items: center;
            padding: 6px 10px;
            background: white;
            border: 1px solid var(--border);
            border-radius: 4px;
            margin-bottom: 4px;
            gap: 10px;
        }
        li:hover { background: var(--hover); }
        .icon { font-size: 1.2rem; }
        .name { flex: 1; word-break: break-all; }
        .size { color: #666; font-size: 0.85rem; }
        .actions { display: flex; gap: 5px; }
        .actions button {
            border: none;
            background: transparent;
            cursor: pointer;
            padding: 2px 5px;
            font-size: 0.9rem;
        }
        .actions .delete { color: var(--danger); }
        .actions .test { color: var(--primary); }
        #progress {
            display: none;
            margin-top: 10px;
            background: #ddd;
            border-radius: 10px;
            height: 20px;
            overflow: hidden;
        }
        #progress-bar {
            height: 100%;
            width: 0;
            background: var(--primary);
            transition: width 0.2s;
        }
        #network-info {
            margin-top: 20px;
            padding: 10px;
            background: white;
            border: 1px solid var(--border);
            border-radius: 4px;
            font-family: monospace;
            font-size: 0.85rem;
            white-space: pre-line;
        }
        .modal {
            display: none;
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: rgba(0,0,0,0.4);
            justify-content: center;
            align-items: center;
        }
        .modal-content {
            background: white;
            padding: 20px;
            border-radius: 5px;
            width: 90%;
            max-width: 350px;
        }
        .modal input { width: 100%; padding: 8px; margin: 5px 0; }
        .modal button { margin-top: 10px; margin-right: 5px; }
    </style>
</head>
<body>
    <div class="toolbar">
        <button onclick="goUp()">⬆️ Вверх</button>
        <button onclick="refresh()">🔄 Обновить</button>
        <button onclick="showMkdir()">📁 Папка</button>
        <button onclick="showRename()">✏️ Переименовать</button>
        <label for="fileInput">📁 Выбрать файлы</label>
        <input type="file" id="fileInput" multiple>
        <button onclick="uploadSelected()">⬆️ Загрузить выбранное</button>
    </div>

    <div id="currentPath">/</div>
    <ul id="fileList"></ul>

    <div id="progress">
        <div id="progress-bar"></div>
    </div>

    <div id="network-info">Загрузка сетевых данных...</div>

    <!-- Модальное окно -->
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
        const fileInput = document.getElementById('fileInput');

        function refresh() {
            fetch('/api/list?path=' + encodeURIComponent(currentPath))
                .then(r => r.json())
                .then(data => {
                    currentPath = data.path;
                    document.getElementById('currentPath').textContent = currentPath;
                    renderFiles(data.files);
                })
                .catch(() => alert('Ошибка загрузки списка'));
        }

        function renderFiles(files) {
            const list = document.getElementById('fileList');
            list.innerHTML = '';
            files.forEach(file => {
                const li = document.createElement('li');
                const icon = file.isDir ? '📁' : '📄';
                li.innerHTML = `
                    <span class="icon">${icon}</span>
                    <span class="name">${file.name}</span>
                    <span class="size">${file.isDir ? '' : formatSize(file.size)}</span>
                    <span class="actions">
                        <button class="test" title="Тест чтения" onclick="testRead('${file.path}')">⏱</button>
                        <button class="delete" title="Удалить" onclick="deleteFile('${file.path}','${file.name}')">🗑</button>
                    </span>
                `;
                li.onclick = (e) => {
                    if (e.target.tagName !== 'BUTTON') {
                        selectedFile = file;
                        // выделение визуально
                        li.style.background = '#d0e4ff';
                    }
                };
                list.appendChild(li);
            });
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }

        function goUp() {
            if (currentPath === '/') return;
            const parts = currentPath.split('/').filter(p => p);
            parts.pop();
            currentPath = '/' + parts.join('/');
            if (currentPath === '') currentPath = '/';
            refresh();
        }

        function uploadSelected() {
            const files = fileInput.files;
            if (!files.length) {
                alert('Выберите файлы для загрузки');
                return;
            }
            const progressDiv = document.getElementById('progress');
            const progressBar = document.getElementById('progress-bar');
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';

            let completed = 0;
            const total = files.length;
            const uploadNext = (index) => {
                if (index >= files.length) {
                    progressDiv.style.display = 'none';
                    fileInput.value = '';
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

        function testRead(path) {
            fetch('/api/testread?path=' + encodeURIComponent(path))
                .then(r => r.json())
                .then(data => {
                    alert(`Файл: ${path}\nРазмер: ${data.size} байт\nВремя чтения: ${data.time_ms} мс`);
                })
                .catch(() => alert('Ошибка теста чтения'));
        }

        function deleteFile(path, name) {
            if (!confirm(`Удалить ${name}?`)) return;
            fetch('/api/delete', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({path: path})
            })
            .then(() => refresh())
            .catch(() => alert('Ошибка удаления'));
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

        function modalOk() {
            const value = document.getElementById('modalInput').value.trim();
            if (value && modalCallback) modalCallback(value);
            document.getElementById('modal').style.display = 'none';
        }

        function modalCancel() {
            document.getElementById('modal').style.display = 'none';
        }

        // Запуск
        refresh();
        refreshNetworkInfo();
        setInterval(refreshNetworkInfo, 10000); // обновление каждые 10 сек
    </script>
</body>
</html>
)rawliteral";

#endif
