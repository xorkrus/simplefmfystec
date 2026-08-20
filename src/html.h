#ifndef HTML_H
#define HTML_H

const char fallbackHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SD Card File Manager</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #333; margin-bottom: 20px; }
        .upload-area { border: 2px dashed #ccc; border-radius: 10px; padding: 30px; text-align: center; background: #fff; margin-bottom: 20px; transition: border-color 0.3s; }
        .upload-area.dragover { border-color: #4CAF50; background: #e8f5e9; }
        .upload-area p { color: #666; margin-bottom: 10px; }
        .btn { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-size: 14px; margin: 5px; }
        .btn:hover { background: #45a049; }
        .btn-danger { background: #f44336; }
        .btn-danger:hover { background: #da190b; }
        .btn-info { background: #2196F3; }
        .btn-info:hover { background: #1976D2; }
        .progress-container { display: none; margin-top: 10px; }
        .progress-bar { width: 100%; height: 20px; background: #e0e0e0; border-radius: 10px; overflow: hidden; }
        .progress-fill { height: 100%; background: #4CAF50; width: 0%; transition: width 0.3s; }
        .progress-text { text-align: center; margin-top: 5px; color: #666; }
        .file-list { background: #fff; border-radius: 10px; overflow: hidden; }
        .file-item { display: flex; align-items: center; padding: 15px; border-bottom: 1px solid #eee; }
        .file-item:last-child { border-bottom: none; }
        .file-item:hover { background: #f9f9f9; }
        .file-icon { width: 50px; height: 50px; object-fit: cover; border-radius: 5px; margin-right: 15px; }
        .file-icon.gcode-thumb { width: 300px; height: 300px; object-fit: contain; }
        .file-info { flex: 1; }
        .file-name { font-weight: bold; color: #333; }
        .file-size { color: #999; font-size: 12px; }
        .file-actions { display: flex; gap: 5px; }
        .toolbar { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; align-items: center; }
        .path-display { background: #fff; padding: 10px 15px; border-radius: 5px; flex: 1; }
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); justify-content: center; align-items: center; z-index: 1000; }
        .modal-content { background: #fff; padding: 25px; border-radius: 10px; width: 90%; max-width: 400px; }
        .modal h2 { margin-bottom: 15px; color: #333; }
        .modal input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; margin-bottom: 15px; }
        .modal-buttons { display: flex; gap: 10px; justify-content: flex-end; }
        .breadcrumb { margin-bottom: 15px; }
        .breadcrumb a { color: #2196F3; text-decoration: none; }
        .breadcrumb a:hover { text-decoration: underline; }
        .dir-item .file-icon { background: #FFC107; display: flex; align-items: center; justify-content: center; border-radius: 5px; }
        .dir-item .file-icon::before { content: "📁"; font-size: 30px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📁 SD Card File Manager</h1>
        <div class="breadcrumb" id="breadcrumb"></div>
        <div class="toolbar">
            <button class="btn btn-info" onclick="showCreateDirModal()">📁 New Folder</button>
            <div class="path-display" id="currentPath">Current: /</div>
        </div>
        <div class="upload-area" id="uploadArea">
            <p>Drag & drop files here or click to select</p>
            <input type="file" id="fileInput" multiple style="display:none">
            <button class="btn" onclick="document.getElementById('fileInput').click()">Select Files</button>
            <div class="progress-container" id="progressContainer">
                <div class="progress-bar"><div class="progress-fill" id="progressFill"></div></div>
                <div class="progress-text" id="progressText">0%</div>
            </div>
        </div>
        <div class="file-list" id="fileList"></div>
    </div>
    <div class="modal" id="renameModal">
        <div class="modal-content">
            <h2>Rename File</h2>
            <input type="text" id="renameInput" placeholder="New name">
            <input type="hidden" id="renameOldName">
            <div class="modal-buttons">
                <button class="btn" onclick="closeModal('renameModal')">Cancel</button>
                <button class="btn btn-info" onclick="doRename()">Rename</button>
            </div>
        </div>
    </div>
    <div class="modal" id="moveModal">
        <div class="modal-content">
            <h2>Move File</h2>
            <input type="text" id="moveInput" placeholder="Destination path (e.g., /folder/)">
            <input type="hidden" id="moveSource">
            <div class="modal-buttons">
                <button class="btn" onclick="closeModal('moveModal')">Cancel</button>
                <button class="btn btn-info" onclick="doMove()">Move</button>
            </div>
        </div>
    </div>
    <div class="modal" id="createDirModal">
        <div class="modal-content">
            <h2>New Folder</h2>
            <input type="text" id="newDirName" placeholder="Folder name">
            <div class="modal-buttons">
                <button class="btn" onclick="closeModal('createDirModal')">Cancel</button>
                <button class="btn btn-info" onclick="doCreateDir()">Create</button>
            </div>
        </div>
    </div>
    <script>
        let currentPath = '/';
        const uploadArea = document.getElementById('uploadArea');
        const fileInput = document.getElementById('fileInput');
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, preventDefaults, false);
        });
        function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }
        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => uploadArea.classList.add('dragover'), false);
        });
        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, () => uploadArea.classList.remove('dragover'), false);
        });
        uploadArea.addEventListener('drop', handleDrop, false);
        fileInput.addEventListener('change', handleFileSelect, false);
        function handleDrop(e) { uploadFiles(e.dataTransfer.files); }
        function handleFileSelect(e) { uploadFiles(e.target.files); }
        async function uploadFiles(files) {
            const progressContainer = document.getElementById('progressContainer');
            const progressFill = document.getElementById('progressFill');
            const progressText = document.getElementById('progressText');
            progressContainer.style.display = 'block';
            for (let i = 0; i < files.length; i++) {
                const file = files[i];
                const formData = new FormData();
                formData.append('file', file);
                try {
                    await fetch('/upload?path=' + encodeURIComponent(currentPath), { method: 'POST', body: formData });
                    progressFill.style.width = ((i + 1) / files.length * 100) + '%';
                    progressText.textContent = Math.round((i + 1) / files.length * 100) + '%';
                } catch (err) { alert('Upload failed: ' + err.message); }
            }
            setTimeout(() => { progressContainer.style.display = 'none'; }, 1000);
            loadDirectory();
        }
        async function loadDirectory() {
            try {
                const response = await fetch('/list?path=' + encodeURIComponent(currentPath));
                const data = await response.json();
                renderFileList(data);
                updateBreadcrumb();
                document.getElementById('currentPath').textContent = 'Current: ' + currentPath;
            } catch (err) { console.error('Failed to load directory:', err); }
        }
        function renderFileList(data) {
            const fileList = document.getElementById('fileList');
            fileList.innerHTML = '';
            if (currentPath !== '/') {
                const parentItem = document.createElement('div');
                parentItem.className = 'file-item dir-item';
                parentItem.innerHTML = '<div class="file-icon"></div><div class="file-info"><div class="file-name">..</div></div>';
                parentItem.onclick = () => { currentPath = currentPath.split('/').slice(0, -2).join('/') + '/'; if(currentPath === '') currentPath = '/'; loadDirectory(); };
                fileList.appendChild(parentItem);
            }
            (data.dirs || []).forEach(dir => {
                const item = document.createElement('div');
                item.className = 'file-item dir-item';
                item.innerHTML = '<div class="file-icon"></div><div class="file-info"><div class="file-name">' + dir + '</div></div><div class="file-actions"><button class="btn btn-danger" onclick="deleteItem(\'' + currentPath + dir + '/\')">×</button></div>';
                item.onclick = (e) => { if (!e.target.classList.contains('btn')) { currentPath = currentPath + dir + '/'; loadDirectory(); } };
                fileList.appendChild(item);
            });
            (data.files || []).forEach(file => {
                const item = document.createElement('div');
                item.className = 'file-item';
                const isGcode = file.name.toLowerCase().endsWith('.gcode') || file.name.toLowerCase().endsWith('.gco') || file.name.toLowerCase().endsWith('.g');
                const thumbClass = isGcode ? 'file-icon gcode-thumb' : 'file-icon';
                const thumbUrl = '/thumb?file=' + encodeURIComponent(currentPath + file.name);
                item.innerHTML = '<img class="' + thumbClass + '" src="' + thumbUrl + '" onerror="this.src=\'/thumb?file=default\'" alt="thumb"><div class="file-info"><div class="file-name">' + file.name + '</div><div class="file-size">' + formatSize(file.size) + '</div></div><div class="file-actions"><button class="btn" onclick="downloadFile(\'' + currentPath + file.name + '\')">⬇</button><button class="btn btn-info" onclick="showRenameModal(\'' + currentPath + file.name + '\')">✏</button><button class="btn btn-info" onclick="showMoveModal(\'' + currentPath + file.name + '\')">➡</button><button class="btn btn-danger" onclick="deleteItem(\'' + currentPath + file.name + '\')">×</button></div>';
                fileList.appendChild(item);
            });
        }
        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        }
        function updateBreadcrumb() {
            const breadcrumb = document.getElementById('breadcrumb');
            const parts = currentPath.split('/').filter(p => p);
            let html = '<a href="#" onclick="currentPath=\'/\'; loadDirectory(); return false;">Root</a>';
            let path = '/';
            parts.forEach(part => { path += part + '/'; html += ' / <a href="#" onclick="currentPath=\'' + path + '\'; loadDirectory(); return false;">' + part + '</a>'; });
            breadcrumb.innerHTML = html;
        }
        function deleteItem(path) { if (confirm('Delete ' + path + '?')) { fetch('/delete?path=' + encodeURIComponent(path), { method: 'POST' }).then(() => loadDirectory()); } }
        function downloadFile(path) { window.location.href = '/download?file=' + encodeURIComponent(path); }
        function showRenameModal(path) { document.getElementById('renameOldName').value = path; document.getElementById('renameInput').value = path.split('/').pop(); document.getElementById('renameModal').style.display = 'flex'; }
        function doRename() { const oldPath = document.getElementById('renameOldName').value; const newName = document.getElementById('renameInput').value; if (newName) { const newPath = oldPath.substring(0, oldPath.lastIndexOf('/') + 1) + newName; fetch('/rename?old=' + encodeURIComponent(oldPath) + '&new=' + encodeURIComponent(newPath), { method: 'POST' }).then(() => { closeModal('renameModal'); loadDirectory(); }); } }
        function showMoveModal(path) { document.getElementById('moveSource').value = path; document.getElementById('moveInput').value = currentPath; document.getElementById('moveModal').style.display = 'flex'; }
        function doMove() { const source = document.getElementById('moveSource').value; const dest = document.getElementById('moveInput').value; if (dest) { const fileName = source.split('/').pop(); const newPath = dest.endsWith('/') ? dest + fileName : dest + '/' + fileName; fetch('/move?src=' + encodeURIComponent(source) + '&dst=' + encodeURIComponent(newPath), { method: 'POST' }).then(() => { closeModal('moveModal'); loadDirectory(); }); } }
        function showCreateDirModal() { document.getElementById('newDirName').value = ''; document.getElementById('createDirModal').style.display = 'flex'; }
        function doCreateDir() { const name = document.getElementById('newDirName').value; if (name) { fetch('/mkdir?path=' + encodeURIComponent(currentPath + name), { method: 'POST' }).then(() => { closeModal('createDirModal'); loadDirectory(); }); } }
        function closeModal(id) { document.getElementById(id).style.display = 'none'; }
        loadDirectory();
    </script>
</body>
</html>
)rawliteral";

#endif
