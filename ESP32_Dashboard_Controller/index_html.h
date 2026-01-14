const char debug_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<title>Web Serial Monitor</title>
<style>
body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; margin: 0; display: flex; flex-direction: column; height: 100vh; }
header { background: #333; padding: 10px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #444; }
h1 { margin: 0; font-size: 18px; color: #fff; }
#terminal { flex: 1; overflow-y: auto; padding: 10px; white-space: pre-wrap; word-wrap: break-word; font-size: 14px; line-height: 1.4; }
.log-line { border-bottom: 1px solid #2d2d2d; padding: 2px 0; }
.btn { background: #0d6efd; color: white; border: none; padding: 5px 15px; border-radius: 4px; cursor: pointer; text-decoration: none; font-family: sans-serif; font-size: 14px; }
.btn-back { background: #6c757d; margin-right: 10px; }
.controls { display: flex; align-items: center; }
#status { font-size: 12px; color: #888; margin-right: 10px; }
.autoscroll-container { margin-left: 15px; font-family: sans-serif; font-size: 12px; display: flex; align-items: center; }
input[type="checkbox"] { margin-right: 5px; }
.cmd-bar { display: flex; gap: 5px; padding: 10px; background: #2d2d2d; border-top: 1px solid #444; }
.cmd-input { flex: 1; padding: 8px; border-radius: 4px; border: 1px solid #555; background: #1e1e1e; color: white; font-family: monospace; }
.cmd-input:focus { outline: none; border-color: #0d6efd; }
</style>
</head>
<body>
<header>
<div style="display:flex; align-items:center;">
<h1>Web Serial Monitor</h1>
</div>
<div class="controls">
<span id="status">Connessione...</span>
<div class="autoscroll-container">
<input type="checkbox" id="autoscroll" checked> <label for="autoscroll">Auto-scroll</label>
</div>
<button onclick="clearLog()" class="btn" style="background: #dc3545; margin-left: 10px;">Clear</button>
</div>
</header>
<div id="terminal"></div>
<div class="cmd-bar">
<button onclick="sendCmd('status')" class="btn" style="background: #198754;">STATUS</button>
<button onclick="sendCmd('help')" class="btn" style="background: #6c757d;">HELP</button>
<button onclick="sendCmd('reset')" class="btn" style="background: #ffc107; color: black;">RESET</button>
<button onclick="sendCmd('restart')" class="btn" style="background: #dc3545;">RESTART</button>
<input type="text" id="cmdInput" class="cmd-input" placeholder="Type command..." onkeydown="if(event.key==='Enter') sendCustomCmd()">
<button onclick="sendCustomCmd()" class="btn">SEND</button>
</div>
<script>
const terminal = document.getElementById('terminal');
const status = document.getElementById('status');
const autoscroll = document.getElementById('autoscroll');
let lastLog = [];
function updateLog() {
fetch('/api/logs')
.then(res => res.json())
.then(logs => {
status.innerText = 'Connected';
status.style.color = '#4caf50';
terminal.innerHTML = logs.map(line => `<div class="log-line">${escapeHtml(line)}</div>`).join('');
if (autoscroll.checked) {
terminal.scrollTop = terminal.scrollHeight;
}
})
.catch(err => {
status.innerText = 'Disconnected';
status.style.color = '#dc3545';
});
}
function escapeHtml(text) {
if (!text) return "";
return text
.replace(/&/g, "&amp;")
.replace(/</g, "&lt;")
.replace(/>/g, "&gt;")
.replace(/"/g, "&quot;")
.replace(/'/g, "&#039;");
}
function clearLog() {
fetch('/api/logs/clear', { method: 'POST' })
.then(() => {
terminal.innerHTML = '';
});
}
function sendCmd(cmd) {
fetch('/api/debug/command', {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: 'command=' + encodeURIComponent(cmd)
})
.then(res => {
if(res.ok) console.log('Command sent:', cmd);
else console.error('Command failed');
});
}
function sendCustomCmd() {
const input = document.getElementById('cmdInput');
const cmd = input.value.trim();
if (cmd) {
sendCmd(cmd);
input.value = '';
}
}
setInterval(updateLog, 1000);
updateLog();
</script>
</body>
</html>
)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Domotica Dashboard</title>
<style>
:root { --primary: #0d6efd; --success: #198754; --danger: #dc3545; --warning: #ffc107; --dark: #212529; --light: #f8f9fa; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: #f0f2f5; margin: 0; padding: 20px; color: #333; }
.container { max-width: 1200px; margin: 0 auto; }
header { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px; display: flex; justify-content: space-between; align-items: center; }
h1 { margin: 0; font-size: 24px; color: var(--dark); }
.status-badge { padding: 5px 10px; border-radius: 4px; font-size: 14px; font-weight: bold; }
.status-online { background-color: #d1e7dd; color: #0f5132; }
.status-offline { background-color: #f8d7da; color: #842029; }
.gateway-card { background: white; border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); border-left: 5px solid var(--primary); }
.gateway-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; border-bottom: 1px solid #eee; padding-bottom: 10px; }
.gateway-title { font-size: 18px; font-weight: bold; margin: 0; }
.gateway-info { font-size: 12px; color: #666; }
.nodes-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; }
.node-card { background: #f8f9fa; border: 1px solid #eee; border-radius: 8px; padding: 15px; transition: transform 0.2s; }
.node-card:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.1); }
.node-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
.node-id { font-weight: bold; font-size: 16px; }
.node-type { font-size: 12px; background: #e9ecef; padding: 2px 6px; border-radius: 4px; color: #495057; }
.controls { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 15px; }
.btn { border: none; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 14px; font-weight: 500; transition: all 0.2s; white-space: nowrap; }
.btn-switch { background-color: #e9ecef; color: #495057; flex: 1; }
.btn-switch.active { background-color: var(--success); color: white; }
.btn-switch:hover:not(.active) { background-color: #dee2e6; }
.loading { text-align: center; padding: 40px; color: #666; }
.no-data { text-align: center; padding: 20px; color: #999; font-style: italic; }
.toolbar { background: white; padding: 15px 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px; display: flex; flex-wrap: wrap; gap: 10px; align-items: center; justify-content: space-between; }
.toolbar-group { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
@keyframes spin { 100% { transform: rotate(360deg); } }
</style>
</head>
<body>
<div class="container">
<header>
<div>
<h1><span id="dashboard-title">Dashboard</span> <span id="mqtt-prefix-title" style="font-size: 16px; color: #666; font-weight: normal;"></span></h1>
<div style="font-size: 12px; color: #666; margin-top: 5px; display: flex; align-items: center; gap: 10px;">
<span id="connection-status" class="status-badge" style="background-color: #ffc107; color: #000;">⌛ Connessione...</span>
<span id="last-update"></span> |
<span id="dashboard-version" style="font-weight: bold; color: #6610f2;">v?.?.?</span>
</div>
</div>
<div>
<button onclick="location.href='/settings'" class="btn" style="background: #6c757d; color: white;">⚙️ Setup</button>
</div>
</header>
<div class="toolbar">
<div class="toolbar-group">
<button onclick="triggerDiscovery()" class="btn" style="background: var(--success); color: white;">🔍 Discovery</button>
<button onclick="refreshDiscovery()" class="btn" style="background: var(--primary); color: white;">🔄 Aggiorna Rete</button>
</div>
<div class="toolbar-group">
<span id="dashboard-update-container" style="display:none; margin-right: 5px;"></span>
<button onclick="checkUpdates()" class="btn" style="background: #0dcaf0; color: black;" title="Controlla aggiornamenti online">☁️ Check Upd</button>
<button class="btn" style="background: #6610f2; color: white;" onclick="document.getElementById('system-firmware').click()" title="Aggiornamento Firmware Dashboard Manuale">🚀 FW Upload</button>
<button onclick="window.open('/debug', 'DebugWindow', 'width=800,height=600');" class="btn" style="background: #212529; color: white;">🖥️ Console</button>
<input type="file" id="system-firmware" accept=".bin" style="display: none" onchange="uploadSystemFirmware()">
<input type="file" id="node-ota-file" accept=".bin" style="display: none" onchange="handleNodeOtaUpload()">
</div>
</div>
<!-- System Status Panel -->
<div class="gateway-card" style="border-left: 5px solid var(--success);">
<div class="gateway-header" style="border-bottom: none; margin-bottom: 0; padding-bottom: 0;">
<div style="width: 100%;">
<div style="display: flex; align-items: center; margin-bottom: 15px; border-bottom: 1px solid #eee; padding-bottom: 10px;">
<span style="font-size: 24px; margin-right: 10px;">📊</span>
<h2 style="margin: 0; font-size: 20px; color: var(--dark);">Stato Sistema (Dashboard)</h2>
</div>
<!-- System Status Grid (Matched to Gateway Image Layout) -->
<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px;">
<!-- Column 1 (Left) -->
<div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">💾</span>
<strong>RAM Libera:</strong> <span id="sys-ram">---</span> bytes
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">📁</span>
<strong>Spazio Dati:</strong> <span id="sys-fs">---</span> bytes
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">🕒</span>
<strong>Orario:</strong> <span id="sys-time">---</span>
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">🔧</span>
<strong>Firmware:</strong> <span id="sys-fw">---</span>
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">📡</span>
<strong>MAC Address:</strong> <span id="sys-mac">---</span>
</div>
</div>
<!-- Column 2 (Right) -->
<div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">🧩</span>
<strong>Frammentazione:</strong> <span id="sys-frag">---</span>%
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">⚡</span>
<strong>CPU Freq:</strong> <span id="sys-cpu">---</span> MHz
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">⏱️</span>
<strong>Uptime:</strong> <span id="sys-uptime">---</span> min
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">📅</span>
<strong>Build:</strong> <span id="sys-build">---</span>
</div>
<div style="margin-bottom: 8px;">
<span style="font-size: 18px;">🌐</span>
<strong>IP Address:</strong> <span id="sys-ip">---</span> (<span id="sys-rssi">---</span> dBm)
</div>
</div>
</div>
</div>
</div>
</div>
<div id="app">
<div class="loading">Caricamento dispositivi...</div>
</div>
</div>
<!-- Update Modal -->
<div id="update-modal" style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.5); z-index:5000; align-items:center; justify-content:center;">
<div style="background:white; padding:30px; border-radius:10px; max-width:500px; width:90%; position:relative;">
<h2 style="margin-top:0; color:#0d6efd;">Aggiornamento Disponibile</h2>
<div id="update-content"></div>
<div style="margin-top:20px; display:flex; justify-content:flex-end; gap:10px;">
<button onclick="document.getElementById('update-modal').style.display='none'" class="btn" style="background:#6c757d;">Chiudi</button>
<button id="btn-perform-update" onclick="performGitHubUpdate()" class="btn" style="background:#198754;">Aggiorna Ora</button>
</div>
</div>
</div>
<script>
let nodeTypesConfig = {};
const defaultRelayConfig = {
description: "Default 4-Relay",
entities: [
{ suffix: "relay_1", type: "switch", name: "Relay 1", idx: 0 },
{ suffix: "relay_2", type: "switch", name: "Relay 2", idx: 1 },
{ suffix: "relay_3", type: "switch", name: "Relay 3", idx: 2 },
{ suffix: "relay_4", type: "switch", name: "Relay 4", idx: 3 }
]
};
const systemTypes = ["COMMAND", "RESPONSE", "ACK", "FEEDBACK", "DISCOVERY", "UNKNOWN", "null", "", "REGISTRATION", "HEARTBEAT"];
let gateways = {};
let peers = {};
let dashboardData = {};
let lastUpdate = 0;
function fetchNodeTypes() {
fetch('/api/nodetypes')
.then(response => {
if (!response.ok) throw new Error("HTTP error " + response.status);
return response.json();
})
.then(config => {
console.log("Loaded Node Types Config:", config);
nodeTypesConfig = config;
updateUI();
})
.catch(err => {
console.error('Error fetching node types config:', err);
nodeTypesConfig = {
"4_RELAY_CONTROLLER": defaultRelayConfig
};
});
}
// --- Refactored OTA Functions ---

// 1. Gateway Auto Update
function startAutoUpdateGateway(url, ip, btn) {
    if(!confirm(`Avviare aggiornamento automatico Gateway (${ip}) da GitHub?\nVersione: ${url.split('/').pop()}`)) return;

    const originalText = btn.innerHTML;
    btn.disabled = true;
    btn.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span> DL...';

    showToast("Download firmware da GitHub...", "info");

    fetch(url)
    .then(res => {
        if (!res.ok) throw new Error(`Download fallito (${res.status})`);
        return res.blob();
    })
    .then(blob => {
        const filename = url.split('/').pop() || "firmware.bin";
        const file = new File([blob], filename, { type: "application/octet-stream" });
        
        btn.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span> Install...';
        uploadGatewayFirmware(ip, file, 1, btn, originalText);
    })
    .catch(err => {
        console.error(err);
        showToast("Errore Download: " + err.message, "error");
        btn.innerHTML = "⚠️ Err";
        setTimeout(() => {
            btn.innerHTML = originalText;
            btn.disabled = false;
        }, 3000);
    });
}

// 2. Node Auto Update
function startAutoUpdateNode(nodeId, gatewayId, url, btn) {
    if (!gatewayId || !gateways[gatewayId]) {
        alert("Errore: Gateway non trovato.");
        return;
    }
    
    if(!confirm(`Avviare aggiornamento automatico Nodo ${nodeId} da GitHub?\nVersione: ${url.split('/').pop()}`)) return;

    const originalText = btn.innerHTML;
    btn.disabled = true;
    btn.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span> DL...';
    
    showToast("Download firmware da GitHub...", "info");

    fetch(url)
    .then(res => {
        if (!res.ok) throw new Error(`Download fallito (${res.status})`);
        return res.blob();
    })
    .then(blob => {
        const filename = url.split('/').pop() || "firmware.bin";
        const file = new File([blob], filename, { type: "application/octet-stream" });
        
        // Setup monitoring context
        otaTargetNode = nodeId;
        otaTargetGatewayIp = gateways[gatewayId].ip;
        otaBtnElement = btn;
        
        // Call the upload logic directly
        performNodeOtaUpload(file, btn, originalText);
    })
    .catch(err => {
        console.error(err);
        showToast("Errore Download: " + err.message, "error");
        btn.innerHTML = "⚠️ Err";
        setTimeout(() => {
            btn.innerHTML = originalText;
            btn.disabled = false;
        }, 3000);
    });
}

function uploadGatewayFirmware(ip, fileOrInput, attempt = 1, btn = null, originalBtnText = null) {
    let file;
    if (fileOrInput instanceof HTMLElement) { // Input element
        if (fileOrInput.files.length === 0) return;
        file = fileOrInput.files[0];
        
        // --- SAFETY CHECK ---
        const fileName = file.name.toLowerCase();
        if (!fileName.includes("gateway") && !fileName.includes("esp8266")) {
             alert("⛔ BLOCCO SICUREZZA ⛔\n\nIl file selezionato NON sembra un firmware per il Gateway.\nControlla che il nome del file contenga 'Gateway' o 'ESP8266'.\n\nOperazione Annullata.");
             fileOrInput.value = '';
             return;
        }
        // --------------------

        if (attempt === 1 && !confirm(`Aggiornare Gateway (${ip}) con:\n${file.name}?`)) {
            fileOrInput.value = '';
            return;
        }
    } else { // File object (Auto update)
        file = fileOrInput;
    }

    const maxAttempts = 3;
    const statusId = 'ota-status-' + ip.replace(/\./g, '-');
    let statusDiv = document.getElementById(statusId);
    if (!statusDiv) {
        statusDiv = document.createElement('div');
        statusDiv.id = statusId;
        statusDiv.style.cssText = "position:fixed; top:50%; left:50%; transform:translate(-50%, -50%); background:rgba(0,0,0,0.8); color:white; padding:20px; border-radius:10px; z-index:1000; text-align:center; min-width:300px; box-shadow: 0 4px 6px rgba(0,0,0,0.1);";
        document.body.appendChild(statusDiv);
    }
    statusDiv.innerHTML = `Avvio aggiornamento Gateway (${ip})...<br>Tentativo ${attempt}/${maxAttempts}<br>File: ${file.name}`;

    const formData = new FormData();
    formData.append("update", file);

    const xhr = new XMLHttpRequest();
    xhr.open("POST", `http://${ip}/update_gateway`, true);
    xhr.timeout = 60000; // 60s timeout for upload

    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            statusDiv.innerHTML = `Gateway Upload (${ip}): ${percent}%<br>Tentativo ${attempt}/${maxAttempts}`;
        }
    };

    xhr.onload = function() {
        if (xhr.status === 200) {
            statusDiv.innerHTML = `✅ Aggiornamento Riuscito!<br>Riavvio Gateway in corso...`;
            statusDiv.style.background = "rgba(40, 167, 69, 0.9)";
            setTimeout(() => {
                if(statusDiv) statusDiv.remove();
                if(btn && originalBtnText) {
                     btn.innerHTML = originalBtnText;
                     btn.disabled = false;
                }
            }, 5000);
        } else {
            handleGwError(`Errore HTTP: ${xhr.status}`);
        }
    };

    xhr.onerror = function() {
        handleGwError("Errore di Rete");
    };

    xhr.ontimeout = function() {
        handleGwError("Timeout Connessione");
    };

    function handleGwError(msg) {
        console.error(`Gateway OTA Error (Attempt ${attempt}):`, msg);
        if (attempt < maxAttempts) {
            statusDiv.innerHTML = `⚠️ Errore: ${msg}<br>Riprovo tra 2s (Tentativo ${attempt+1})...`;
            setTimeout(() => {
                uploadGatewayFirmware(ip, file, attempt + 1, btn, originalBtnText);
            }, 2000);
        } else {
            statusDiv.innerHTML = `❌ Aggiornamento Fallito dopo ${maxAttempts} tentativi.<br>Errore: ${msg}`;
            statusDiv.style.background = "rgba(220, 53, 69, 0.9)";
            setTimeout(() => { if(statusDiv) statusDiv.remove(); }, 5000);
            if(btn && originalBtnText) {
                btn.innerHTML = "❌";
                setTimeout(() => {
                    btn.innerHTML = originalBtnText;
                    btn.disabled = false;
                }, 3000);
            }
        }
    }

    xhr.send(formData);
}
function performDashboardAutoUpdate(url, btn) {
    if(!confirm(`Avviare aggiornamento automatico Dashboard da GitHub?\nVersione: ${url.split('/').pop()}`)) return;

    const originalText = btn.innerHTML;
    btn.disabled = true;
    btn.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span> Avvio...';
    
    showToast("Richiesta aggiornamento inviata...", "info");

    fetch('/api/update_from_url', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'url=' + encodeURIComponent(url)
    })
    .then(res => {
        if (!res.ok) throw new Error("Errore avvio update: " + res.status);
        return res.text();
    })
    .then(text => {
        showToast("Aggiornamento Avviato! Il dispositivo si riavvierà.", "success");
        btn.innerHTML = "✅ Rebooting...";
        setTimeout(() => location.reload(), 15000);
    })
    .catch(err => {
        console.error(err);
        showToast("Errore: " + err.message, "error");
        btn.innerHTML = "⚠️ Err";
        setTimeout(() => {
            btn.innerHTML = originalText;
            btn.disabled = false;
        }, 3000);
    });
}

function updateUI() {
    // 1. Check Dashboard Update & Inject Button if needed
    const updateContainer = document.getElementById('dashboard-update-container');
    if (updateContainer) {
        // Use shared helper
        if (hasDashboardUpdate()) {
            const dashUpd = dashboardData.updates.dashboard;
            const onlineVer = dashUpd.version;
            const url = dashUpd.url;
            
            // Remove existing button if any
            const existingBtn = document.getElementById('dash-auto-update-btn');
            if (existingBtn) existingBtn.remove();

            const btn = document.createElement('button');
            btn.id = 'dash-auto-update-btn';
            btn.className = 'btn';
            btn.style.cssText = "background: #ffc107; color: black; border: 1px solid #d39e00; cursor: pointer; animation: pulse 2s infinite;";
            btn.innerHTML = `Update v${onlineVer}`;
            btn.title = `Aggiorna Dashboard a v${onlineVer}`;
            btn.onclick = function() { performDashboardAutoUpdate(url, this); };
            
            // Append to toolbar container
            updateContainer.appendChild(btn);
            updateContainer.style.display = 'inline-block';
        } else {
            updateContainer.style.display = 'none';
        }
    }

const app = document.getElementById('app');
if (Object.keys(gateways).length === 0) {
app.innerHTML = '<div class="no-data">Nessun Gateway rilevato. Assicurati che il sistema MQTT sia attivo.</div>';
return;
}
let html = '';
for (const [gwId, gw] of Object.entries(gateways)) {

const isOnline = true;
    let gwUpdateBtn = '';
    
    // Use shared helper
    if (hasGatewayUpdate(gw)) {
        const gwOnlineVer = dashboardData.updates.gateway.version;
        const gwUrl = dashboardData.updates.gateway.url;
        gwUpdateBtn = `
        <button class="status-badge" style="background: #ffc107; color: black; border: 1px solid #d39e00; cursor: pointer; margin-right:5px; animation: pulse 2s infinite;" onclick="startAutoUpdateGateway('${gwUrl}', '${gw.ip}', this)" title="Aggiorna Automaticamente a v${gwOnlineVer}">
        Update v${gwOnlineVer}
        </button>`;
    }
html += `
<div class="gateway-card">
<div class="gateway-header">
<div>
<div class="gateway-title">Gateway: ${gwId}</div>
<div class="gateway-info">
IP: ${gw.ip || 'N/A'} |
MQTT: ${gw.mqttStatus || 'N/A'} |
Last Seen: ${new Date(Date.now() - (dashboardData.uptime - gw.lastSeen)).toLocaleTimeString()}
</div>
<div class="gateway-info" style="color: #0d6efd; font-weight: bold; margin-top: 2px;">
v${gw.version || '?'} (Build: ${gw.buildDate || 'N/A'})
</div>
</div>
<div style="display: flex; align-items: center; gap: 10px;">
${gwUpdateBtn}
<a href="http://${gw.ip}/" target="_blank" class="status-badge" style="background: #0dcaf0; color: #000; text-decoration: none; cursor: pointer;">🌐 Gestione</a>
<input type="file" id="gateway-ota-file-${gwId}" accept=".bin" style="display: none" onchange="uploadGatewayFirmware('${gw.ip}', this)">
<button class="status-badge" style="background: #6610f2; color: white; border: none; cursor: pointer;" onclick="document.getElementById('gateway-ota-file-${gwId}').click()" title="Aggiorna Firmware Gateway">🚀</button>
<span class="status-badge ${isOnline ? 'status-online' : 'status-offline'}">
${isOnline ? 'ONLINE' : 'OFFLINE'}
</span>
</div>
</div>
<div class="nodes-grid">`;
const gwPeers = Object.values(peers).filter(p => p.gatewayId === gwId);
if (gwPeers.length === 0) {
html += '<div style="grid-column: 1/-1; text-align: center; color: #999;">Nessun nodo associato</div>';
} else {
gwPeers.forEach(peer => {
html += renderNodeCard(peer);
});
}
html += `</div></div>`;
}
app.innerHTML = html;
const now = new Date();
document.getElementById('last-update').innerText = `Aggiornato: ${now.toLocaleTimeString()}`;
}
function sendAdminCommand(nodeId, command, gatewayId, mac, btnElement) {
if (mac && mac instanceof HTMLElement) {
btnElement = mac;
mac = null;
}
let confirmMsg = "";
if (command === 'RESTART') confirmMsg = "Riavviare il nodo " + nodeId + "?";
if (command === 'NODE_FACTORY_RESET') confirmMsg = "ATTENZIONE: Reset di fabbrica per " + nodeId + "? Il nodo perderà la configurazione WiFi/Pairing.";
if (command === 'REMOVE_PEER') confirmMsg = "Eliminare il nodo " + nodeId + " dalla lista del Gateway?";
if (!confirm(confirmMsg)) return;
let originalContent = "";
if (btnElement) {
originalContent = btnElement.innerHTML;
btnElement.disabled = true;
btnElement.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span>';
}
const restoreBtn = (success) => {
if (btnElement) {
btnElement.innerHTML = success ? "✅" : "❌";
setTimeout(() => {
if (command !== 'REMOVE_PEER' || !success) {
btnElement.disabled = false;
btnElement.innerHTML = originalContent;
}
}, 2000);
}
};
let promise;
if (command === 'RESTART') {
promise = sendCommand(nodeId, 'CONTROL', 'RESTART');
} else {
let payload = {
nodeId: nodeId,
command: command,
gatewayId: gatewayId
};
if (command === 'REMOVE_PEER') {
payload.mac = mac;
}

// DIRECT GATEWAY COMMAND (Bypass Dashboard/MQTT)
if (command === 'NODE_FACTORY_RESET' && gateways[gatewayId] && gateways[gatewayId].ip) {
    const fallbackToDashboard = (reason) => {
        console.warn("Direct Gateway command failed (" + reason + "), falling back to Dashboard API...");
        return fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        })
        .then(response => response.json());
    };

    promise = fetch('http://' + gateways[gatewayId].ip + '/api/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => {
        if (!response.ok) throw new Error("Gateway responded with " + response.status);
        return response.json();
    })
    .then(data => {
        if (data.error) throw new Error(data.error);
        return data;
    })
    .catch(err => {
        return fallbackToDashboard(err.message);
    });
} else {
    promise = fetch('/api/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(response => response.json());
}
}
promise
.then(data => {
if (data.error) throw new Error(data.error);
console.log('Admin Command Sent:', data);
restoreBtn(true);
if (command === 'REMOVE_PEER') {
if (btnElement) {
const card = btnElement.closest('.node-card');
if (card) {
setTimeout(() => {
card.style.transition = 'opacity 0.5s, transform 0.5s';
card.style.opacity = '0';
card.style.transform = 'scale(0.9)';
setTimeout(() => {
card.remove();
}, 500);
}, 1000);
}
}
}
})
.catch(err => {
console.error('Error:', err);
alert('Errore invio comando: ' + err);
restoreBtn(false);
});
}
function renderNodeCard(peer) {
const isOnline = (peer.status && peer.status.toLowerCase() === 'offline') ? false : true;
let attributes = peer.attributes;
if (!attributes || typeof attributes !== 'string' || attributes === "null") attributes = "0000";
let controlsHtml = '';
let config = null;
if (nodeTypesConfig[peer.nodeType]) {
config = nodeTypesConfig[peer.nodeType];
}
else if (peer.nodeType && peer.nodeType.indexOf('RELAY') !== -1) {
config = defaultRelayConfig;
}
else if (!peer.nodeType || systemTypes.includes(peer.nodeType)) {
config = defaultRelayConfig;
}
if (config && config.entities) {
controlsHtml += '<div class="controls">';
let hasSwitch = false;
config.entities.forEach(entity => {
if (entity.type === 'switch') {
hasSwitch = true;
let idx = entity.idx;
if (idx === undefined) idx = parseInt(entity.suffix.replace('relay_', '')) - 1;
if (idx < 0) idx = 0;
const isActive = (attributes.length > idx && attributes[idx] === '1');
const btnClass = isActive ? 'btn btn-switch active' : 'btn btn-switch';
const label = entity.name || entity.suffix;
controlsHtml += `<button class="${btnClass}" onclick="sendCommand('${peer.nodeId}', '${entity.suffix}', '2')">${label}</button>`;
}
else if (entity.type === 'cover') {
controlsHtml += `
<div style="width: 100%; margin-top: 5px; margin-bottom: 5px;">
<div style="font-size: 12px; font-weight: bold; margin-bottom: 2px;">${entity.name}</div>
<div class="btn-group" style="display: flex; gap: 5px;">
<button class="btn btn-switch" style="flex:1" onclick="sendCommand('${peer.nodeId}', '${entity.suffix}', 'UP')">▲</button>
<button class="btn btn-switch" style="flex:1" onclick="sendCommand('${peer.nodeId}', '${entity.suffix}', 'STOP')">■</button>
<button class="btn btn-switch" style="flex:1" onclick="sendCommand('${peer.nodeId}', '${entity.suffix}', 'DOWN')">▼</button>
</div>
</div>
`;
}
});
controlsHtml += '</div>';
if (hasSwitch && config.entities.filter(e => e.type === 'switch').length > 1) {
controlsHtml += `
<div class="controls" style="margin-top: 5px;">
<button class="btn btn-switch" onclick="sendCommand('${peer.nodeId}', 'CONTROL', 'ALL_ON')">TUTTI ON</button>
<button class="btn btn-switch" onclick="sendCommand('${peer.nodeId}', 'CONTROL', 'ALL_OFF')">TUTTI OFF</button>
</div>
`;
}
} else {
controlsHtml = `<div style="margin-top:10px; font-size:12px; color:#999;">Controlli non disponibili per questo tipo: ${peer.nodeType}</div>`;
}

let autoOtaBtn = "";

if (hasNodeUpdate(peer)) {
        const nodeType = peer.nodeType || "DEFAULT";
        let updateInfo = dashboardData.updates.nodes[nodeType];
        if (!updateInfo && nodeType === "UNKNOWN") updateInfo = dashboardData.updates.nodes["DEFAULT"];
        
        console.log(`[DEBUG] UPDATE AVAILABLE! ${updateInfo.version} > ${peer.firmwareVersion}`);
        
        autoOtaBtn = `
        <button class="status-badge" style="background: #ffc107; color: black; border: 1px solid #d39e00; cursor: pointer; margin-right:5px; animation: pulse 2s infinite; font-size: 12px; padding: 4px 8px;" 
        onclick="startAutoUpdateNode('${peer.nodeId}', '${peer.gatewayId}', '${updateInfo.url}', this)" 
        title="Aggiorna Automaticamente a v${updateInfo.version}">Update v${updateInfo.version}</button>`;
    }

controlsHtml += `
<div style="margin-top: 10px; border-top: 1px solid #ddd; padding-top: 8px; display: flex; justify-content: flex-end; gap: 5px; align-items: center;">
${autoOtaBtn}
<button class="btn" style="background: #6610f2; color: white; padding: 4px 8px; font-size: 12px; flex: 0;" onclick="startNodeOta('${peer.nodeId}', '${peer.gatewayId}', this)" title="Upload Manuale Firmware (.bin)">🚀</button>
<button class="btn" style="background: #ffc107; color: black; padding: 4px 8px; font-size: 12px; flex: 0;" onclick="sendAdminCommand('${peer.nodeId}', 'RESTART', '${peer.gatewayId}', null, this)" title="Riavvia Nodo">🔄</button>
<button class="btn" style="background: #fd7e14; color: white; padding: 4px 8px; font-size: 12px; flex: 0;" onclick="sendAdminCommand('${peer.nodeId}', 'NODE_FACTORY_RESET', '${peer.gatewayId}', null, this)" title="Factory Reset Nodo">⚠️</button>
<button class="btn" style="background: #dc3545; color: white; padding: 4px 8px; font-size: 12px; flex: 0;" onclick="sendAdminCommand('${peer.nodeId}', 'REMOVE_PEER', '${peer.gatewayId}', '${peer.mac}', this)" title="Elimina Nodo">🗑️</button>
</div>
`;

return `
<div class="node-card">
<div class="node-header">
<span class="node-id">${peer.nodeId}</span>
<span class="node-type">${peer.nodeType}</span>
</div>
<div style="display:flex; justify-content:space-between; font-size:12px; margin-bottom:10px;">
<span>MAC: ${peer.mac} <span style="background: #e9ecef; padding: 2px 6px; border-radius: 4px; color: #495057; font-weight: bold; margin-left: 5px;">v${peer.firmwareVersion || '?'}</span></span>
<span style="color: ${isOnline ? 'var(--success)' : 'var(--danger)'}; font-weight:bold;">
${isOnline ? '● Online' : '○ Offline'}
</span>
</div>
${controlsHtml}
</div>
`;
}
function compareVersions(v1, v2) {
    if (!v1 || !v2) return 0;
    // Remove non-numeric chars except dots (e.g. "v1.0" -> "1.0")
    const clean = (v) => v.toString().replace(/[^0-9.]/g, '');
    const p1 = clean(v1).split('.').map(Number);
    const p2 = clean(v2).split('.').map(Number);
    for (let i = 0; i < Math.max(p1.length, p2.length); i++) {
        const n1 = p1[i] || 0;
        const n2 = p2[i] || 0;
        if (n1 > n2) return 1;
        if (n1 < n2) return -1;
    }
    return 0;
}

// --- Shared Update Logic ---
function hasDashboardUpdate() {
    // Trust backend 'available' flag (set by checkGithubUpdates)
    if (dashboardData.updates && dashboardData.updates.dashboard) {
        return dashboardData.updates.dashboard.available === true;
    }
    return false;
}

function hasGatewayUpdate(gw) {
    if (!dashboardData.updates || !dashboardData.updates.gateway) return false;
    const onlineVer = dashboardData.updates.gateway.version;
    // For gateways, we still compare versions frontend-side as backend sets global availability
    return (gw.version && onlineVer && compareVersions(onlineVer, gw.version) > 0);
}

function hasNodeUpdate(peer) {
    if (!dashboardData.updates || !dashboardData.updates.nodes) return false;
    const type = peer.nodeType || "DEFAULT";
    let info = dashboardData.updates.nodes[type];
    if (!info && type === "UNKNOWN") info = dashboardData.updates.nodes["DEFAULT"];
    
    if (info && info.version) {
        return compareVersions(info.version, peer.firmwareVersion) > 0;
    }
    return false;
}
function renderData(data) {
    gateways = data.gateways || {};
    peers = data.peers || {};
    dashboardData = data.dashboard || {};
    // Ensure updates are attached to dashboardData for checkUpdates() to work
    if (data.updates) {
        dashboardData.updates = data.updates;
    }

    if (typeof checkOtaMonitors === 'function') {
        checkOtaMonitors();
    }
if (data.dashboard) {
document.getElementById('dashboard-version').innerText =
`v${data.dashboard.version} (Build: ${data.dashboard.buildDate})`;
if (data.dashboard.id) {
const titleElem = document.getElementById('dashboard-title');
if (titleElem) titleElem.innerText = data.dashboard.id;
}
if (data.dashboard.mqttPrefix) {
const prefixElem = document.getElementById('mqtt-prefix-title');
if (prefixElem) prefixElem.innerText = "[" + data.dashboard.mqttPrefix + "]";
document.title = (data.dashboard.id || "Dashboard") + " [" + data.dashboard.mqttPrefix + "]";
}
if (document.getElementById('sys-ram')) {
document.getElementById('sys-ram').innerText = data.dashboard.freeHeap;
document.getElementById('sys-fs').innerText = `${data.dashboard.fsUsed}/${data.dashboard.fsTotal}`;
const uptimeMin = Math.floor(data.dashboard.uptime / 60000);
document.getElementById('sys-uptime').innerText = uptimeMin;
if (document.getElementById('sys-time')) document.getElementById('sys-time').innerText = data.dashboard.time || 'N/A';
document.getElementById('sys-build').innerText = data.dashboard.buildDate.split(' ').slice(0, 3).join(' ');
let frag = 0;
if (data.dashboard.freeHeap > 0) {
frag = Math.round((1 - (data.dashboard.maxAllocHeap / data.dashboard.freeHeap)) * 100);
}
if (frag < 0) frag = 0;
document.getElementById('sys-frag').innerText = frag;
document.getElementById('sys-cpu').innerText = data.dashboard.cpuFreq;
document.getElementById('sys-fw').innerText = data.dashboard.version;
document.getElementById('sys-ip').innerText = data.dashboard.ip || 'N/A';
document.getElementById('sys-rssi').innerText = data.dashboard.wifiSignal || '0';
document.getElementById('sys-mac').innerText = data.dashboard.mac || 'N/A';
}
}
updateUI();
}
function fetchData() {
    fetch('/api/data?t=' + Date.now())
        .then(response => response.json())
        .then(data => {
            renderData(data);
        })
        .catch(err => console.error('Error fetching data:', err));
}
function sendCommand(nodeId, topic, command) {
return fetch('/api/command', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ nodeId, topic, command })
})
.then(res => res.json())
.then(res => {
console.log('Command sent:', res);
return res;
})
.catch(err => {
console.error('Error sending command:', err);
throw err;
});
}
function triggerDiscovery() {
const btn = document.querySelector('button[onclick="triggerDiscovery()"]');
if(btn) {
btn.innerText = "Scanning...";
btn.disabled = true;
}
fetch('/api/command', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ command: 'NETWORK_DISCOVERY' })
})
.then(res => res.json())
.then(data => {
})
.catch(err => {
console.error(err);
if(btn) alert("Errore durante la discovery");
})
.finally(() => {
if(btn) {
btn.innerText = "🔍 Discovery";
btn.disabled = false;
}
});
}
function refreshDiscovery() {
const btn = document.querySelector('button[onclick="refreshDiscovery()"]');
if(btn) {
btn.innerText = "Reset & Scansione...";
btn.disabled = true;
}
fetch('/api/command', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ command: 'RESET_NETWORK' })
})
.then(res => res.json())
.then(data => {
console.log("Network reset:", data);
return fetch('/api/command', {
method: 'POST',
headers: { 'Content-Type': 'application/json' },
body: JSON.stringify({ command: 'LIST_PEERS_GLOBAL' })
});
})
.then(res => res.json())
.then(data => {
console.log("Discovery sent:", data);
})
.catch(err => {
console.error(err);
if(btn) alert("Errore durante l'aggiornamento rete");
})
.finally(() => {
if(btn) {
btn.innerText = "Aggiorna Rete";
btn.disabled = false;
}
});
}
function uploadSystemFirmware() {
    const fileInput = document.getElementById('system-firmware');
    const file = fileInput.files[0];
    if (!file) return;

    // --- SAFETY CHECK ---
    const fileName = file.name.toLowerCase();
    if (!fileName.includes("dashboard") && !fileName.includes("esp32")) {
         alert("⛔ BLOCCO SICUREZZA ⛔\n\nIl file selezionato NON sembra un firmware per la Dashboard (ESP32).\nControlla che il nome del file contenga 'Dashboard' o 'ESP32'.\n\nOperazione Annullata.");
         fileInput.value = '';
         return;
    }
    // --------------------

    if (!confirm("ATTENZIONE: Stai per aggiornare il firmware della DASHBOARD. Il dispositivo si riavvierà. Continuare?")) return;
const formData = new FormData();
formData.append("file", file);
const btn = fileInput.nextElementSibling;
const originalText = btn.innerText;
btn.innerText = "Aggiornamento in corso...";
btn.disabled = true;
fetch('/system/update', {
method: 'POST',
body: formData
})
.then(async response => {
const text = await response.text();
if (!response.ok) throw new Error(text || "Update fallito (" + response.status + ")");
return text;
})
.then(text => {
alert("Aggiornamento completato: " + text + "\nIl dispositivo si sta riavviando...");
setTimeout(() => location.reload(), 5000);
})
.catch(err => {
console.error(err);
alert("Errore: " + err.message);
btn.innerText = originalText;
btn.disabled = false;
});
}
let otaTargetNode = null;
let otaTargetGatewayIp = null;
let otaBtnElement = null;
let activeOtaMonitors = {};
function startNodeOta(nodeId, gatewayId, btn) {
if (!gatewayId || !gateways[gatewayId]) {
alert("Errore: Gateway non trovato per questo nodo.");
return;
}
otaTargetNode = nodeId;
otaTargetGatewayIp = gateways[gatewayId].ip;
otaBtnElement = btn;
document.getElementById('node-ota-file').click();
}
function handleNodeOtaUpload() {
    const fileInput = document.getElementById('node-ota-file');
    const file = fileInput.files[0];
    if (!file) return;

    // --- SAFETY CHECK ---
    const fileName = file.name.toLowerCase();
    const peer = Object.values(peers).find(p => p.nodeId === otaTargetNode);
    const nodeType = peer ? (peer.nodeType || "").toLowerCase() : "";
    
    // Check for obvious mismatches
    const isGatewayFile = fileName.includes("gateway") || fileName.includes("esp8266");
    const isDashboardFile = fileName.includes("dashboard") || fileName.includes("esp32");
    
    if (isGatewayFile) {
        alert("⛔ BLOCCO SICUREZZA ⛔\n\nStai tentando di caricare un firmware GATEWAY su un NODO!\n\nOperazione Annullata.");
        fileInput.value = "";
        return;
    }
    
    if (isDashboardFile) {
        alert("⛔ BLOCCO SICUREZZA ⛔\n\nStai tentando di caricare un firmware DASHBOARD su un NODO!\n\nOperazione Annullata.");
        fileInput.value = "";
        return;
    }
    
    // --- Node Type Verification ---
    if (nodeType && nodeType.length > 2) {
        const nType = nodeType.toLowerCase();
        const fName = fileName.toLowerCase();
        
        // Create variations to check: 
        // 1. "4_relay_controller" (original)
        // 2. "4-relay-controller" (dashes)
        // 3. "4relaycontroller" (compact)
        const typeVariations = [
            nType,
            nType.replace(/_/g, '-'),
            nType.replace(/[_-]/g, '')
        ];
        
        const fileCompact = fName.replace(/[_-]/g, '');
        
        let typeMatch = false;
        for (let v of typeVariations) {
            if (fName.includes(v) || fileCompact.includes(v)) {
                typeMatch = true;
                break;
            }
        }

        if (!typeMatch) {
             const msg = "⚠️ ATTENZIONE POSSIBILE ERRORE ⚠️\n\n" +
                         "Il Nodo destinazione è di tipo: '" + nodeType + "'\n" +
                         "Ma il file selezionato NON sembra contenere questo nome.\n\n" +
                         "File: " + file.name + "\n\n" +
                         "Sei sicuro di voler procedere?";
             if (!confirm(msg)) {
                fileInput.value = "";
                return;
             }
        }
    }
    // ------------------------------

    if (!confirm("Avviare OTA per il nodo " + otaTargetNode + "?\nIl firmware verrà caricato sulla Dashboard e il nodo lo scaricherà direttamente.")) {
        fileInput.value = "";
        return;
    }

    const btn = otaBtnElement;
    const originalBtnContent = btn.innerHTML;
    
    performNodeOtaUpload(file, btn, originalBtnContent, () => {
        fileInput.value = "";
    });
}

function performNodeOtaUpload(file, btn, originalBtnContent, cleanupCallback = null) {
    const formData = new FormData();
    formData.append("update", file);

    const peer = Object.values(peers).find(p => p.nodeId === otaTargetNode);
    const currentVer = peer ? peer.firmwareVersion : '?';

    if(btn) {
        btn.disabled = true;
        btn.innerHTML = '<span style="display:inline-block; animation:spin 1s linear infinite">↻</span>';
    }

    activeOtaMonitors[otaTargetNode] = {
        startVersion: currentVer,
        startTime: Date.now(),
        btn: btn,
        originalContent: originalBtnContent,
        status: 'uploading'
    };
    updateOtaOverlay();

    fetch('/api/upload_node_fw', {
        method: 'POST',
        body: formData
    })
    .then(async res => {
        if (!res.ok) throw new Error("Upload fallito: " + res.status);
        
        const dashboardIp = (dashboardData && dashboardData.dashboard && dashboardData.dashboard.ip) ? dashboardData.dashboard.ip : window.location.hostname;
        const firmwareUrl = "http://" + dashboardIp + "/temp_firmware.bin";
        
        const triggerData = new URLSearchParams();
        triggerData.append("nodeId", otaTargetNode);
        triggerData.append("url", firmwareUrl);
        
        if (activeOtaMonitors[otaTargetNode]) {
            activeOtaMonitors[otaTargetNode].status = 'triggering';
            updateOtaOverlay();
        }

        return fetch('http://' + otaTargetGatewayIp + '/trigger_ota', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: triggerData
        });
    })
    .then(async res => {
        if (!res.ok) throw new Error("Trigger OTA fallito: " + res.status);
        if (activeOtaMonitors[otaTargetNode]) {
            activeOtaMonitors[otaTargetNode].status = 'waiting_node';
            updateOtaOverlay();
        }
    })
    .catch(err => {
        console.error(err);
        alert("Errore OTA: " + err.message);
        if (activeOtaMonitors[otaTargetNode]) {
            const m = activeOtaMonitors[otaTargetNode];
            if (m.btn) {
                m.btn.disabled = false;
                m.btn.innerHTML = "❌";
                setTimeout(() => m.btn.innerHTML = m.originalContent, 3000);
            }
            delete activeOtaMonitors[otaTargetNode];
            updateOtaOverlay();
        }
    })
    .finally(() => {
        if (cleanupCallback) cleanupCallback();
    });
}

function updateOtaOverlay() {
let container = document.getElementById('ota-monitor-overlay');
if (!container) {
container = document.createElement('div');
container.id = 'ota-monitor-overlay';
container.style.cssText = "position:fixed; top:50%; left:50%; transform:translate(-50%, -50%); z-index:2000; display:flex; flex-direction:column; gap:10px;";
document.body.appendChild(container);
}
const nodes = Object.keys(activeOtaMonitors);
if (nodes.length === 0) {
container.innerHTML = '';
return;
}
let html = '';
nodes.forEach(nodeId => {
const m = activeOtaMonitors[nodeId];
let statusText = "In attesa...";
let color = "#17a2b8";
if (m.status === 'uploading') statusText = "Upload al Server...";
if (m.status === 'triggering') statusText = "Invio comando al Gateway...";
if (m.status === 'waiting_node') {
statusText = "Attesa aggiornamento nodo...";
color = "#ffc107";
}
html += `
<div style="background:rgba(0,0,0,0.8); color:white; padding:15px; border-radius:5px; min-width: 250px; border-left: 5px solid ${color};">
<div style="font-weight:bold; margin-bottom:5px;">${nodeId} OTA</div>
<div style="font-size:12px;">Stato: ${statusText}</div>
<div style="font-size:12px; color:#ccc;">Ver: ${m.startVersion} -> ?</div>
<div style="font-size:10px; color:#999; margin-top:5px;">Tempo: ${Math.floor((Date.now() - m.startTime)/1000)}s</div>
</div>`;
});
container.innerHTML = html;
}
function checkOtaMonitors() {
const now = Date.now();
let changed = false;
const nodes = Object.keys(activeOtaMonitors);
if (nodes.length === 0) return;
nodes.forEach(nodeId => {
const m = activeOtaMonitors[nodeId];
const peer = Object.values(peers).find(p => p.nodeId === nodeId);
if (peer && m.status === 'waiting_node') {
const lastSeenAgo = (dashboardData.uptime || 0) - (peer.lastSeen || 0);
const timeSinceStart = Date.now() - m.startTime;
if (peer.firmwareVersion !== m.startVersion) {
finishOta(nodeId, m, peer.firmwareVersion, "success");
changed = true;
return;
} else if (timeSinceStart > 20000 && lastSeenAgo < 10000 && peer.status === 'online') {
finishOta(nodeId, m, peer.firmwareVersion, "success");
changed = true;
return;
}
}
if (now - m.startTime > 300000) {
finishOta(nodeId, m, null, "timeout");
changed = true;
}
});
if (changed || nodes.length > 0) updateOtaOverlay();
}
function finishOta(nodeId, m, newVersion, result) {
if (m.btn) {
m.btn.disabled = false;
if (result === "success") {
m.btn.innerHTML = "✅";
showToast(`Aggiornamento ${nodeId} Completato! v${m.startVersion} -> v${newVersion}`, 'success');
} else {
m.btn.innerHTML = "⚠️";
showToast(`Timeout Aggiornamento ${nodeId}`, 'error');
}
setTimeout(() => m.btn.innerHTML = m.originalContent, 5000);
}
delete activeOtaMonitors[nodeId];
}
function showToast(msg, type='info') {
const id = 'toast-' + Date.now();
const div = document.createElement('div');
const bg = type === 'success' ? '#198754' : (type === 'error' ? '#dc3545' : '#0dcaf0');
div.style.cssText = `position:fixed; top:20px; left:50%; transform:translateX(-50%); background:${bg}; color:white; padding:10px 20px; border-radius:5px; z-index:3000; box-shadow:0 2px 10px rgba(0,0,0,0.2); transition: opacity 0.5s;`;
div.innerText = msg;
document.body.appendChild(div);
setTimeout(() => {
div.style.opacity = '0';
setTimeout(() => div.remove(), 500);
}, 5000);
}
function formatUptime(ms) {
if (!ms) return '0m';
const mins = Math.floor(ms / 60000);
const hours = Math.floor(mins / 60);
if (hours > 0) return `${hours}h ${mins % 60}m`;
return `${mins}m`;
}
function showUpdateModal() {
if (!window.latestUpdate) return;
const content = document.getElementById('update-content');
content.innerHTML = `
<p><strong>Versione Attuale:</strong> ${dashboardData.version || '?'}</p>
<p><strong>Nuova Versione:</strong> ${window.latestUpdate.version}</p>
<p><strong>Note di rilascio:</strong><br>${window.latestUpdate.notes}</p>
<div style="background:#fff3cd; color:#856404; padding:10px; border-radius:5px; margin-top:10px; font-size:12px;">
Nota: L'aggiornamento scaricherà il firmware da GitHub e riavvierà il dispositivo. Assicurati che la connessione internet sia stabile.
</div>
`;
document.getElementById('update-modal').style.display = 'flex';
}
function performGitHubUpdate() {
if (!window.latestUpdate || !window.latestUpdate.url) return;
const btn = document.getElementById('btn-perform-update');
btn.innerText = "Avvio download...";
btn.disabled = true;
fetch('/api/update_from_url', {
method: 'POST',
headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
body: 'url=' + encodeURIComponent(window.latestUpdate.url)
})
.then(res => res.text())
.then(text => {
alert("Aggiornamento Avviato: " + text);
document.getElementById('update-modal').style.display = 'none';
setTimeout(() => location.reload(), 10000);
})
.catch(err => {
alert("Errore: " + err);
btn.innerText = "Aggiorna Ora";
btn.disabled = false;
});
}
function checkUpdates() {
const btn = document.querySelector('button[onclick="checkUpdates()"]');
const originalText = btn.innerText;
btn.innerText = "Checking...";
btn.disabled = true;
const startCheckTime = (dashboardData.updates && dashboardData.updates.lastCheck) ? dashboardData.updates.lastCheck : 0;
showToast("Controllo aggiornamenti avviato...", "info");
fetch('/api/check_updates', { method: 'POST' })
.then(res => res.json())
.then(data => {
console.log("Update check triggered:", data);
let attempts = 0;
const pollInterval = setInterval(() => {
attempts++;
fetchData();
                const newCheckTime = (dashboardData.updates && dashboardData.updates.lastCheck) ? dashboardData.updates.lastCheck : 0;
                const lastResult = (dashboardData.updates && dashboardData.updates.lastResult) ? dashboardData.updates.lastResult : "";
                
                console.log(`[DEBUG] Poll ${attempts}: Start=${startCheckTime}, New=${newCheckTime}, Result=${lastResult}`);

                if (newCheckTime !== startCheckTime && newCheckTime !== 0) {
clearInterval(pollInterval);
if (lastResult !== "Success" && lastResult !== "") {
showToast("Errore Controllo: " + lastResult, "error");
} else {
let found = false;
let details = [];

// Check Dashboard
if (hasDashboardUpdate()) {
found = true;
details.push("Dashboard");
}

// Check Gateways
for (const gwId in gateways) {
if (hasGatewayUpdate(gateways[gwId])) {
found = true;
if (!details.includes("Gateway")) details.push("Gateway");
}
}

// Check Nodes
for (const nodeId in peers) {
if (hasNodeUpdate(peers[nodeId])) {
found = true;
if (!details.includes("Nodi")) details.push("Nodi");
}
}
if (found) {
showToast("Aggiornamenti trovati per: " + details.join(", ") + "! Controlla le icone gialle.", "success");
} else {
showToast("Nessun nuovo aggiornamento trovato. Il sistema è aggiornato.", "success");
}
}
btn.innerText = originalText;
btn.disabled = false;
}
if (attempts > 120) {
clearInterval(pollInterval);
if (btn.disabled) {
btn.innerText = originalText;
btn.disabled = false;
showToast("Timeout: Nessuna risposta dal server o GitHub.", "warning");
}
}
}, 500);
})
.catch(err => {
console.error("Error checking updates:", err);
showToast("Errore richiesta aggiornamenti", "error");
btn.innerText = originalText;
btn.disabled = false;
});
}
let ws = null;
function initWebSocket() {
console.log('Trying to open WebSocket...');
ws = new WebSocket('ws://' + window.location.hostname + ':81/');
ws.onopen = function() {
console.log('WebSocket Connected');
const statusEl = document.getElementById('connection-status');
statusEl.innerText = '🟢 LIVE';
statusEl.style.backgroundColor = '#d1e7dd';
statusEl.style.color = '#0f5132';
};
ws.onmessage = function(event) {
try {
const data = JSON.parse(event.data);
renderData(data);
} catch (e) {
console.error('WS JSON Parse Error', e);
}
};
ws.onclose = function() {
console.log('WebSocket Disconnected');
const statusEl = document.getElementById('connection-status');
statusEl.innerText = '🔴 DISCONNECTED';
statusEl.style.backgroundColor = '#f8d7da';
statusEl.style.color = '#842029';
setTimeout(initWebSocket, 2000);
};
ws.onerror = function(error) {
console.error('WebSocket Error', error);
};
}
window.onload = function() {
fetchNodeTypes();
initWebSocket();
fetchData();
};
</script>
</body>
</html>
)rawliteral";
