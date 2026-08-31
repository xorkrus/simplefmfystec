#ifndef HTML_INDEX_H
#define HTML_INDEX_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD WiFi File Manager</title>
    <style>
        /* ----- Global Reset & Base ----- */
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
               background: #f5f7fa; color: #1e293b; height: 100vh; display: flex; }
        /* ----- Layout ----- */
        .app { display: flex; width: 100%; height: 100vh; gap: 16px; padding: 16px; }
        .left-panel { flex: 2; display: flex; flex-direction: column; min-width: 0; background: #fff;
                      border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); padding: 16px; }
        .right-panel { flex: 1; min-width: 240px; background: #fff; border-radius: 12px;
                       box-shadow: 0 4px 12px rgba(0,0,0,0.05); padding: 16px; display: flex;
                       flex-direction: column; align-items: center; }
        /* ----- Toolbar ----- */
        .toolbar { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 12px; align-items: center; }
        .toolbar button, .toolbar label { background: #eef2f6; border: none; padding: 8px 14px;
                 border-radius: 8px; font-size: 14px; cursor: pointer; transition: 0.2s; }
        .toolbar button:hover, .toolbar label:hover { background: #dce2ea; }
        .toolbar .primary { background: #2563eb; color: #fff; }
        .toolbar .primary:hover { background: #1d4ed8; }
        .toolbar input[type="file"] { display: none; }
        .breadcrumb { font-size: 14px; padding: 8px 0; color: #64748b; display: flex; flex-wrap: wrap;
                      align-items: center; gap: 4px; border-bottom: 1px solid #e2e8f0; margin-bottom: 12px; }
        .breadcrumb span { cursor: pointer; color: #2563eb; }
        .breadcrumb span:hover { text-decoration: underline; }
        .breadcrumb .sep { color: #94a3b8; cursor: default; }
        /* ----- File list ----- */
        .file-list { flex: 1; overflow-y: auto; }
        .file-item { display: flex; align-items: center; padding: 8px 12px; border-radius: 8px;
                     transition: 0.15s; cursor: default; border-bottom: 1px solid #f1f5f9; }
        .file-item:hover { background: #f8fafc; }
        .file-item .icon { font-size: 20px; margin-right: 10px; width: 28px; text-align: center; }
        .file-item .name { flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .file-item .size { font-size: 13px; color: #64748b; margin-right: 16px; white-space: nowrap; }
        .file-item .actions { display: flex; gap: 4px; }
        .file-item .actions button { background: none; border: none; font-size: 16px; cursor: pointer;
                                     padding: 4px 6px; border-radius: 6px; transition: 0.2s; }
        .file-item .actions button:hover { background: #e2e8f0; }
        .file-item.dir .name { font-weight: 500; color: #0f172a; }
        /* ----- Drop zone & progress ----- */
        .drop-zone { border: 2px dashed #cbd5e1; border-radius: 12px; padding: 24px; text-align: center;
                     margin: 12px 0; background: #f8fafc; transition: 0.3s; }
        .drop-zone.dragover { border-color: #2563eb; background: #eff6ff; }
        .drop-zone p { color: #64748b; font-size: 14px; }
        .progress-bar { width: 100%; height: 6px; background: #e2e8f0; border-radius: 4px;
                        margin: 8px 0; display: none; }
        .progress-bar .fill { height: 100%; width: 0%; background: #2563eb; border-radius: 4px;
                              transition: width 0.3s; }
        /* ----- Right panel preview ----- */
        .right-panel .preview-img { max-width: 100%; max-height: 260px; border-radius: 8px;
                                    object-fit: contain; background: #f1f5f9; }
        .right-panel .file-meta { margin-top: 12px; width: 100%; text-align: center; }
        .right-panel .file-meta .fname { font-weight: 500; word-break: break-all; }
        .right-panel .file-meta .fsize { color: #64748b; font-size: 14px; }
        .right-panel .actions { margin-top: 12px; display: flex; gap: 8px; flex-wrap: wrap;
                                justify-content: center; }
        .right-panel .actions button { background: #eef2f6; border: none; padding: 8px 16px;
                                       border-radius: 8px; cursor: pointer; font-size: 14px; }
        .right-panel .actions button:hover { background: #dce2ea; }
        .right-panel .no-preview { color: #94a3b8; font-size: 14px; margin-top: 40px; }
        /* ----- Toast notifications ----- */
        .toast-container { position: fixed; bottom: 20px; right: 20px; z-index: 999; display: flex;
                           flex-direction: column; gap: 8px; }
        .toast { background: #1e293b; color: #f8fafc; padding: 12px 20px; border-radius: 10px;
                 font-size: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.15); animation: slideUp 0.3s ease; }
        .toast.error { background: #b91c1c; }
        .toast.success { background: #065f46; }
        @keyframes slideUp { from { opacity:0; transform: translateY(20px); } to { opacity:1; transform: translateY(0); } }

        /* Scrollbar */
        .file-list::-webkit-scrollbar { width: 6px; }
        .file-list::-webkit-scrollbar-track { background: #f1f5f9; }
        .file-list::-webkit-scrollbar-thumb { background: #cbd5e1; border-radius: 4px; }

        /* Responsive */
        @media (max-width: 768px) {
            .app { flex-direction: column; padding: 8px; }
            .left-panel { flex: 1; }
            .right-panel { flex: 0 0 auto; min-height: 200px; }
        }
    </style>
</head>
<body>
    <div class="app">
        <!-- Left Panel -->
        <div class="left-panel">
            <div class="toolbar">
                <button onclick="refresh()">⟳ Обновить</button>
                <button onclick="createFolder()">📁 Новая папка</button>
                <label class="primary">⬆ Загрузить <input type="file" id="fileInput" multiple onchange="uploadFiles(this.files)"></label>
                <button onclick="goHome()">🏠 Корень</button>
            </div>
            <div class="breadcrumb" id="breadcrumb"></div>
            <div class="drop-zone" id="dropZone">
                <p>📤 Перетащите файлы сюда или нажмите «Загрузить»</p>
            </div>
            <div class="progress-bar" id="progress"><div class="fill" id="progressFill"></div></div>
            <div class="file-list" id="fileList"></div>
        </div>

        <!-- Right Panel -->
        <div class="right-panel" id="rightPanel">
            <h3 style="font-weight:400; margin-bottom:12px; color:#334155;">Предпросмотр</h3>
            <img id="previewImg" class="preview-img" src="" alt="Нет превью" style="display:none;">
            <div class="no-preview" id="noPreview">Выберите файл для просмотра</div>
            <div class="file-meta" id="fileMeta" style="display:none;">
                <div class="fname" id="fileName"></div>
                <div class="fsize" id="fileSize"></div>
                <div class="actions">
                    <button onclick="downloadCurrent()">⬇ Скачать</button>
                    <button onclick="deleteCurrent()">🗑 Удалить</button>
                    <button onclick="renameCurrent()">✏ Переименовать</button>
                </div>
            </div>
        </div>
    </div>

    <div class="toast-container" id="toastContainer"></div>

    <script>
        // ----- State -----
        let currentPath = '/';
        let selectedFile = null;       // полный путь выбранного файла
        let currentItems = [];

        // ----- Helpers -----
        function toast(msg, type = 'info') {
            const c = document.getElementById('toastContainer');
            const el = document.createElement('div');
            el.className = 'toast ' + type;
            el.textContent = msg;
            c.appendChild(el);
            setTimeout(() => el.remove(), 3500);
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1048576) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1048576).toFixed(1) + ' MB';
        }

        function pathJoin(a, b) {
            if (!a) return b;
            if (a.endsWith('/')) return a + b;
            return a + '/' + b;
        }

        function getDirname(path) {
            const idx = path.lastIndexOf('/');
            if (idx <= 0) return '/';
            return path.substring(0, idx);
        }

        function getBasename(path) {
            const idx = path.lastIndexOf('/');
            return path.substring(idx + 1);
        }

        // ----- API calls -----
        async function fetchJSON(url, options = {}) {
            const res = await fetch(url, options);
            if (!res.ok) throw new Error('HTTP ' + res.status);
            return res.json();
        }

        async function loadDirectory(path) {
            try {
                const data = await fetchJSON('/api/files?path=' + encodeURIComponent(path));
                currentPath = data.path;
                currentItems = data.items;
                renderBreadcrumb();
                renderFileList(data.items);
                // Сброс выбора
                selectedFile = null;
                clearPreview();
                return data;
            } catch (e) {
                toast('Ошибка загрузки папки: ' + e.message, 'error');
            }
        }

        // ----- Render -----
        function renderBreadcrumb() {
            const parts = currentPath.split('/').filter(p => p.length > 0);
            const bc = document.getElementById('breadcrumb');
            let html = '<span onclick="loadDirectory(\'/\')">📁 корень</span>';
            let accum = '';
            for (let i = 0; i < parts.length; i++) {
                accum += '/' + parts[i];
                html += ' <span class="sep">›</span> ';
                html += `<span onclick="loadDirectory('${accum}')">${parts[i]}</span>`;
            }
            bc.innerHTML = html;
        }

        function renderFileList(items) {
            const list = document.getElementById('fileList');
            if (!items || items.length === 0) {
                list.innerHTML = '<div style="padding:20px; text-align:center; color:#94a3b8;">Папка пуста</div>';
                return;
            }
            // Сортировка: папки сверху, потом файлы по имени
            const sorted = items.slice().sort((a,b) => {
                if (a.type === 'dir' && b.type !== 'dir') return -1;
                if (a.type !== 'dir' && b.type === 'dir') return 1;
                return a.name.localeCompare(b.name);
            });
            let html = '';
            sorted.forEach(item => {
                const isDir = item.type === 'dir';
                const icon = isDir ? '📁' : '📄';
                const sizeStr = isDir ? '' : formatSize(item.size);
                const name = item.name;
                const fullPath = pathJoin(currentPath, name);
                html += `<div class="file-item ${isDir ? 'dir' : ''}" data-path="${fullPath}" data-type="${item.type}">
                    <span class="icon">${icon}</span>
                    <span class="name" title="${name}">${name}</span>
                    <span class="size">${sizeStr}</span>
                    <span class="actions">
                        ${isDir ? '' : `<button onclick="downloadFile('${fullPath}')" title="Скачать">⬇</button>`}
                        <button onclick="deleteFile('${fullPath}')" title="Удалить">🗑</button>
                        <button onclick="renameFile('${fullPath}')" title="Переименовать">✏</button>
                        ${isDir ? '' : `<button onclick="selectFile('${fullPath}')" title="Превью">👁</button>`}
                    </span>
                </div>`;
            });
            list.innerHTML = html;
        }

        // ----- File actions -----
        function clickFileItem(path, type) {
            if (type === 'dir') {
                loadDirectory(path);
            } else {
                selectFile(path);
            }
        }

        // При клике на элемент (через делегирование)
        document.getElementById('fileList').addEventListener('click', function(e) {
            const item = e.target.closest('.file-item');
            if (!item) return;
            // Если клик по кнопке - не обрабатываем
            if (e.target.closest('button')) return;
            const path = item.dataset.path;
            const type = item.dataset.type;
            clickFileItem(path, type);
        });

        // Выбор файла для превью
        function selectFile(path) {
            selectedFile = path;
            const name = getBasename(path);
            // Запрашиваем миниатюру
            const img = document.getElementById('previewImg');
            const noPreview = document.getElementById('noPreview');
            const meta = document.getElementById('fileMeta');
            img.style.display = 'block';
            noPreview.style.display = 'none';
            meta.style.display = 'block';
            // Добавляем случайный параметр, чтобы избежать кеша
            img.src = '/api/thumbnail?path=' + encodeURIComponent(path) + '&t=' + Date.now();
            img.onerror = function() {
                // Если не загрузилась, показываем заглушку
                img.style.display = 'none';
                noPreview.style.display = 'block';
                noPreview.textContent = '❌ Миниатюра не найдена';
                meta.style.display = 'block';
            };
            document.getElementById('fileName').textContent = name;
            // Размер
            const item = currentItems.find(i => getBasename(path) === i.name);
            document.getElementById('fileSize').textContent = item ? formatSize(item.size) : '';
        }

        function clearPreview() {
            document.getElementById('previewImg').style.display = 'none';
            document.getElementById('noPreview').style.display = 'block';
            document.getElementById('noPreview').textContent = 'Выберите файл для просмотра';
            document.getElementById('fileMeta').style.display = 'none';
        }

        // ----- Upload with progress -----
        function uploadFiles(files) {
            if (!files || files.length === 0) return;
            const formData = new FormData();
            for (let f of files) formData.append('file', f);
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/api/upload');
            const progress = document.getElementById('progress');
            const fill = document.getElementById('progressFill');
            progress.style.display = 'block';
            fill.style.width = '0%';
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    const pct = (e.loaded / e.total) * 100;
                    fill.style.width = pct + '%';
                }
            };
            xhr.onload = function() {
                progress.style.display = 'none';
                if (xhr.status === 200) {
                    toast('Файлы загружены', 'success');
                    refresh();
                } else {
                    toast('Ошибка загрузки: ' + xhr.status, 'error');
                }
            };
            xhr.onerror = function() {
                progress.style.display = 'none';
                toast('Ошибка сети при загрузке', 'error');
            };
            xhr.send(formData);
        }

        // Drag & Drop
        const dropZone = document.getElementById('dropZone');
        dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('dragover'); });
        dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
        dropZone.addEventListener('drop', e => {
            e.preventDefault();
            dropZone.classList.remove('dragover');
            const files = e.dataTransfer.files;
            if (files.length) uploadFiles(files);
        });

        // ----- Delete -----
        async function deleteFile(path) {
            if (!confirm(`Удалить "${getBasename(path)}"?`)) return;
            try {
                const res = await fetch('/api/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path })
                });
                if (!res.ok) throw new Error('Ошибка удаления');
                toast('Удалено', 'success');
                if (selectedFile === path) clearPreview();
                refresh();
            } catch (e) {
                toast('Ошибка: ' + e.message, 'error');
            }
        }

        // ----- Rename -----
        async function renameFile(path) {
            const oldName = getBasename(path);
            const newName = prompt('Новое имя файла/папки:', oldName);
            if (!newName || newName === oldName) return;
            try {
                const res = await fetch('/api/rename', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path, newname: newName })
                });
                if (!res.ok) throw new Error('Ошибка переименования');
                toast('Переименовано', 'success');
                if (selectedFile === path) clearPreview();
                refresh();
            } catch (e) {
                toast('Ошибка: ' + e.message, 'error');
            }
        }

        // ----- Download -----
        function downloadFile(path) {
            window.open('/api/download?path=' + encodeURIComponent(path), '_blank');
        }

        function downloadCurrent() {
            if (selectedFile) downloadFile(selectedFile);
        }

        // ----- Create folder -----
        async function createFolder() {
            const name = prompt('Введите имя новой папки:');
            if (!name) return;
            const path = pathJoin(currentPath, name);
            try {
                const res = await fetch('/api/mkdir', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path })
                });
                if (!res.ok) throw new Error('Ошибка создания');
                toast('Папка создана', 'success');
                refresh();
            } catch (e) {
                toast('Ошибка: ' + e.message, 'error');
            }
        }

        // ----- Navigation -----
        function refresh() { loadDirectory(currentPath); }
        function goHome() { loadDirectory('/'); }

        // ----- Delete current -----
        function deleteCurrent() {
            if (selectedFile) deleteFile(selectedFile);
        }

        function renameCurrent() {
            if (selectedFile) renameFile(selectedFile);
        }

        // ----- Initial load -----
        loadDirectory('/');
    </script>
</body>
</html>
)rawliteral";

#endif
