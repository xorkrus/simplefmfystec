#ifndef HTML_INDEX_M_H
#define HTML_INDEX_M_H

const char INDEX_M_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>SD WiFi File Manager (Mobile)</title>
    <style>
        /* ----- Reset & base ----- */
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
               background: #f5f7fa; color: #1e293b; height: 100vh; overflow: hidden; }
        .app { display: flex; flex-direction: column; height: 100vh; padding: 8px; gap: 8px; }
        /* ----- Top: Preview ----- */
        .top-preview { background: #fff; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.05);
                       padding: 12px; flex: 0 0 auto; max-height: 40vh; overflow: hidden;
                       display: flex; flex-direction: column; align-items: center; }
        .top-preview .preview-img { max-width: 100%; max-height: 200px; object-fit: contain;
                                    border-radius: 8px; background: #f1f5f9; }
        .top-preview .no-preview { color: #94a3b8; font-size: 14px; padding: 20px 0; }
        .top-preview .file-meta { width: 100%; text-align: center; margin-top: 6px; }
        .top-preview .file-meta .fname { font-weight: 500; font-size: 16px; word-break: break-all; }
        .top-preview .file-meta .fsize { color: #64748b; font-size: 13px; }
        .top-preview .actions { display: flex; gap: 8px; margin-top: 6px; flex-wrap: wrap;
                                justify-content: center; }
        .top-preview .actions button { background: #eef2f6; border: none; padding: 6px 12px;
                                       border-radius: 20px; font-size: 13px; cursor: pointer; }
        .top-preview .actions button:hover { background: #dce2ea; }
        /* ----- Bottom: File Manager ----- */
        .bottom-manager { flex: 1; background: #fff; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.05);
                          padding: 12px; display: flex; flex-direction: column; min-height: 0; }
        .toolbar { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 8px; }
        .toolbar button, .toolbar label { background: #eef2f6; border: none; padding: 6px 12px;
                 border-radius: 20px; font-size: 13px; cursor: pointer; }
        .toolbar .primary { background: #2563eb; color: #fff; }
        .toolbar input[type="file"] { display: none; }
        .breadcrumb { font-size: 13px; color: #64748b; padding: 4px 0; display: flex; flex-wrap: wrap;
                      gap: 2px; border-bottom: 1px solid #e2e8f0; margin-bottom: 8px; }
        .breadcrumb span { cursor: pointer; color: #2563eb; }
        .breadcrumb .sep { color: #94a3b8; }
        .file-list { flex: 1; overflow-y: auto; }
        .file-item { display: flex; align-items: center; padding: 8px 6px; border-bottom: 1px solid #f1f5f9;
                     gap: 6px; }
        .file-item .icon { font-size: 18px; width: 24px; text-align: center; }
        .file-item .name { flex: 1; font-size: 14px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .file-item .size { font-size: 12px; color: #64748b; margin-right: 4px; }
        .file-item .actions { display: flex; gap: 4px; }
        .file-item .actions button { background: none; border: none; font-size: 16px; padding: 2px 4px; }
        .drop-zone { border: 2px dashed #cbd5e1; border-radius: 10px; padding: 12px; text-align: center;
                     margin: 6px 0; background: #f8fafc; font-size: 13px; color: #64748b; }
        .progress-bar { width: 100%; height: 4px; background: #e2e8f0; border-radius: 2px; margin: 4px 0; display: none; }
        .progress-bar .fill { height: 100%; width: 0%; background: #2563eb; border-radius: 2px; transition: width 0.3s; }

        .toast-container { position: fixed; bottom: 10px; left: 50%; transform: translateX(-50%);
                           z-index: 999; display: flex; flex-direction: column; gap: 6px; align-items: center; }
        .toast { background: #1e293b; color: #f8fafc; padding: 10px 20px; border-radius: 30px;
                 font-size: 13px; box-shadow: 0 4px 16px rgba(0,0,0,0.15); animation: slideUp 0.3s ease; }
        .toast.error { background: #b91c1c; }
        .toast.success { background: #065f46; }
        @keyframes slideUp { from { opacity:0; transform: translateY(20px); } to { opacity:1; transform: translateY(0); } }
    </style>
</head>
<body>
    <div class="app">
        <!-- Top preview -->
        <div class="top-preview" id="topPreview">
            <img id="previewImg" class="preview-img" src="" alt="Нет превью" style="display:none;">
            <div class="no-preview" id="noPreview">👆 Выберите файл</div>
            <div class="file-meta" id="fileMeta" style="display:none;">
                <div class="fname" id="fileName"></div>
                <div class="fsize" id="fileSize"></div>
                <div class="actions">
                    <button onclick="downloadCurrent()">⬇ Скачать</button>
                    <button onclick="deleteCurrent()">🗑 Удалить</button>
                    <button onclick="renameCurrent()">✏ Переимен.</button>
                </div>
            </div>
        </div>

        <!-- Bottom manager -->
        <div class="bottom-manager">
            <div class="toolbar">
                <button onclick="refresh()">⟳</button>
                <button onclick="createFolder()">📁</button>
                <label class="primary">⬆ <input type="file" id="fileInput" multiple onchange="uploadFiles(this.files)"></label>
                <button onclick="goHome()">🏠</button>
            </div>
            <div class="breadcrumb" id="breadcrumb"></div>
            <div class="drop-zone" id="dropZone">📤 Перетащите файлы</div>
            <div class="progress-bar" id="progress"><div class="fill" id="progressFill"></div></div>
            <div class="file-list" id="fileList"></div>
        </div>
    </div>

    <div class="toast-container" id="toastContainer"></div>

    <script>
        // ----- State (почти идентично десктопной версии) -----
        let currentPath = '/';
        let selectedFile = null;
        let currentItems = [];

        function toast(msg, type = 'info') {
            const c = document.getElementById('toastContainer');
            const el = document.createElement('div');
            el.className = 'toast ' + type;
            el.textContent = msg;
            c.appendChild(el);
            setTimeout(() => el.remove(), 3000);
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
                selectedFile = null;
                clearPreview();
                return data;
            } catch (e) {
                toast('Ошибка загрузки: ' + e.message, 'error');
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
                list.innerHTML = '<div style="padding:16px; text-align:center; color:#94a3b8; font-size:14px;">Папка пуста</div>';
                return;
            }
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
                const fullPath = pathJoin(currentPath, item.name);
                html += `<div class="file-item" data-path="${fullPath}" data-type="${item.type}">
                    <span class="icon">${icon}</span>
                    <span class="name">${item.name}</span>
                    <span class="size">${sizeStr}</span>
                    <span class="actions">
                        ${isDir ? '' : `<button onclick="selectFile('${fullPath}')">👁</button>`}
                        <button onclick="deleteFile('${fullPath}')">🗑</button>
                    </span>
                </div>`;
            });
            list.innerHTML = html;
        }

        // Клик по элементу (делегирование)
        document.getElementById('fileList').addEventListener('click', function(e) {
            const item = e.target.closest('.file-item');
            if (!item) return;
            if (e.target.closest('button')) return;
            const path = item.dataset.path;
            const type = item.dataset.type;
            if (type === 'dir') {
                loadDirectory(path);
            } else {
                selectFile(path);
            }
        });

        // ----- Preview -----
        function selectFile(path) {
            selectedFile = path;
            const name = getBasename(path);
            const img = document.getElementById('previewImg');
            const noPreview = document.getElementById('noPreview');
            const meta = document.getElementById('fileMeta');
            img.style.display = 'block';
            noPreview.style.display = 'none';
            meta.style.display = 'block';
            img.src = '/api/thumbnail?path=' + encodeURIComponent(path) + '&t=' + Date.now();
            img.onerror = function() {
                img.style.display = 'none';
                noPreview.style.display = 'block';
                noPreview.textContent = '❌ Миниатюра не найдена';
                meta.style.display = 'block';
            };
            document.getElementById('fileName').textContent = name;
            const item = currentItems.find(i => getBasename(path) === i.name);
            document.getElementById('fileSize').textContent = item ? formatSize(item.size) : '';
        }

        function clearPreview() {
            document.getElementById('previewImg').style.display = 'none';
            document.getElementById('noPreview').style.display = 'block';
            document.getElementById('noPreview').textContent = '👆 Выберите файл';
            document.getElementById('fileMeta').style.display = 'none';
        }

        // ----- Upload -----
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
                    fill.style.width = (e.loaded / e.total * 100) + '%';
                }
            };
            xhr.onload = function() {
                progress.style.display = 'none';
                if (xhr.status === 200) {
                    toast('Файлы загружены', 'success');
                    refresh();
                } else {
                    toast('Ошибка загрузки', 'error');
                }
            };
            xhr.onerror = function() {
                progress.style.display = 'none';
                toast('Ошибка сети', 'error');
            };
            xhr.send(formData);
        }

        // Drag & Drop
        const dropZone = document.getElementById('dropZone');
        dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.style.borderColor = '#2563eb'; });
        dropZone.addEventListener('dragleave', () => dropZone.style.borderColor = '#cbd5e1');
        dropZone.addEventListener('drop', e => {
            e.preventDefault();
            dropZone.style.borderColor = '#cbd5e1';
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
            const newName = prompt('Новое имя:', oldName);
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

        function downloadCurrent() { if (selectedFile) downloadFile(selectedFile); }

        // ----- Create folder -----
        async function createFolder() {
            const name = prompt('Имя новой папки:');
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

        // ----- Current actions -----
        function deleteCurrent() { if (selectedFile) deleteFile(selectedFile); }
        function renameCurrent() { if (selectedFile) renameFile(selectedFile); }

        // ----- Init -----
        loadDirectory('/');
    </script>
</body>
</html>
)rawliteral";

#endif
