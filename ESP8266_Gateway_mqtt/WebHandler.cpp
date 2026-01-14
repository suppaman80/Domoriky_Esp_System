#include "WebHandler.h"
#include "Config.h"
#include "PeerHandler.h"
#include "MqttHandler.h"
#include "EspNowHandler.h"
#include "NodeTypeManager.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "version.h"

// Dashboard Discovery
String discoveredDashboardIP = "";
unsigned long lastDashboardSeen = 0;

// External functions from .ino
extern void printGatewayStatus();

// Helper for debug printing
void printDebugStats(const char* context) {
    uint32_t free = ESP.getFreeHeap();
    uint32_t max = ESP.getMaxFreeBlockSize();
    uint8_t frag = ESP.getHeapFragmentation();
    DevLog.printf("[DEBUG] %s - Heap: %u (MaxBlock: %u, Frag: %u%%)\n", context, free, max, frag);
}

// -------------------------------------------------------------------------
// Setup & Lifecycle
// -------------------------------------------------------------------------

bool shouldStartConfigMode() {
    // Controlla se esiste una configurazione valida
    if (!LittleFS.exists("/config.json")) {
        return true;
    }
    
    // Carica configurazione per verificare se è valida
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile) {
        return true;
    }
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();
    
    if (error) {
        return true;
    }
    
    // Verifica se tutti i campi necessari sono presenti
    const char* ssid = doc["wifi_ssid"];
    const char* password = doc["wifi_password"];
    const char* mqtt_server_conf = doc["mqtt_server"];
    
    return (ssid == nullptr || strlen(ssid) == 0 || 
            password == nullptr || strlen(password) == 0 ||
            mqtt_server_conf == nullptr || strlen(mqtt_server_conf) == 0);
}

void handleApiDashboardInfo() {
    DynamicJsonDocument doc(256);
    if (discoveredDashboardIP.length() > 0) {
        doc["found"] = true;
        doc["ip"] = discoveredDashboardIP;
        doc["age"] = (millis() - lastDashboardSeen) / 1000;
    } else {
        doc["found"] = false;
    }
    String response;
    serializeJson(doc, response);
    configServer.send(200, "application/json", response);
}

void setupWebRoutes() {
    DevLog.println("🔧 setupWebRoutes: Registrazione rotte web...");
    // configServer.enableCORS(true); // DISABILITATO: Gestiamo CORS manualmente per evitare conflitti e errori "instant"
    
    // Fix CORS Preflight for Dashboard API calls
    auto sendCors = [](){ 
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        configServer.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
        configServer.sendHeader("Access-Control-Allow-Headers", "Content-Type, Content-Length, X-Requested-With");
        configServer.sendHeader("Access-Control-Max-Age", "86400"); // Cache preflight per 24h
        configServer.send(200); 
    };
    configServer.on("/trigger_ota", HTTP_OPTIONS, sendCors);
    configServer.on("/trigger_ota", HTTP_POST, []() {
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        
        if (!configServer.hasArg("nodeId") || !configServer.hasArg("url")) {
            configServer.send(400, "text/plain", "Missing parameters");
            return;
        }

        String targetNodeId = configServer.arg("nodeId");
        String url = configServer.arg("url");
        
        // Find MAC
        uint8_t* targetMac = nullptr;
        for (int i = 0; i < peerCount; i++) {
            if (String(peerList[i].nodeId) == targetNodeId) {
                targetMac = peerList[i].mac;
                break;
            }
        }

        if (!targetMac) {
            configServer.send(404, "text/plain", "Node not found");
            return;
        }

        // Construct Payload: SSID|PASS|URL
        // Usa le credenziali salvate o quelle correnti
        String ssidToSend = strlen(saved_wifi_ssid) > 0 ? String(saved_wifi_ssid) : String(wifi_ssid);
        String passToSend = strlen(saved_wifi_password) > 0 ? String(saved_wifi_password) : String(wifi_password);
        
        String payload = ssidToSend + "|" + passToSend + "|" + url;

        // Send Command
        espNow.send(targetMac, targetNodeId.c_str(), "CONTROL", "OTA_UPDATE", payload.c_str(), "COMMAND", gateway_id);
        
        DevLog.printf("🚀 Triggered Node OTA for %s (URL: %s)\n", targetNodeId.c_str(), url.c_str());
        configServer.send(200, "text/plain", "OTA Triggered");
    });
    configServer.on("/api/dashboard_discover", HTTP_OPTIONS, sendCors);
    
    configServer.on("/", HTTP_GET, handleRoot);
    configServer.on("/api/dashboard_info", HTTP_GET, handleApiDashboardInfo);
    // Route /api/set_led removed
    configServer.on("/api/dashboard_discover", HTTP_POST, []() {
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        sendDashboardDiscovery();
        configServer.send(200, "text/plain", "Discovery Sent");
    });
    configServer.on("/save", HTTP_POST, handleSave);

    // Gateway Firmware Update Handler (OTA)
    configServer.on("/update_gateway", HTTP_POST, []() {
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        configServer.sendHeader("Connection", "close");
        configServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(1000);
        ESP.restart();
    }, []() {
        HTTPUpload& upload = configServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
            DevLog.printf("📡 Aggiornamento FW Gateway avviato: %s\n", upload.filename.c_str());
            // Start with max available size
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) { 
                DevLog.println("❌ Errore Inizio OTA");
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                // Write error
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { // true to set the size to the current progress
                DevLog.printf("✅ OTA Completato con successo: %u bytes\n", upload.totalSize);
            } else {
                DevLog.println("❌ Errore Fine OTA");
                Update.printError(Serial);
            }
        }
    });

    configServer.on("/settings", HTTP_GET, handleSettings);
    
    configServer.on("/debug", HTTP_GET, []() {
        configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
        configServer.send(200, "text/html", "");
        
        // Header & Styles
        configServer.sendContent(F("<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'><title>Gateway Serial Monitor</title>"));
        configServer.sendContent(F("<style>body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;margin:0;display:flex;flex-direction:column;height:100vh}"));
        configServer.sendContent(F("header{background:#333;padding:10px 20px;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #444}"));
        configServer.sendContent(F("h1{margin:0;font-size:18px;color:#fff}#terminal{flex:1;overflow-y:auto;padding:10px;white-space:pre-wrap;word-wrap:break-word;font-size:14px;line-height:1.4}"));
        configServer.sendContent(F(".log-line{border-bottom:1px solid #2d2d2d;padding:2px 0}.btn{background:#0d6efd;color:white;border:none;padding:5px 15px;border-radius:4px;cursor:pointer;font-family:sans-serif;font-size:14px}"));
        configServer.sendContent(F(".controls{display:flex;align-items:center}#status{font-size:12px;color:#888;margin-right:10px}.autoscroll-container{margin-left:15px;font-family:sans-serif;font-size:12px;display:flex;align-items:center}"));
        configServer.sendContent(F(".cmd-bar{display:flex;gap:5px;padding:10px;background:#2d2d2d;border-top:1px solid #444}"));
        configServer.sendContent(F(".cmd-input{flex:1;padding:8px;border-radius:4px;border:1px solid #555;background:#1e1e1e;color:white;font-family:monospace}.cmd-input:focus{outline:none;border-color:#0d6efd}"));
        configServer.sendContent(F("</style></head><body><header><div><h1>Gateway Serial Monitor</h1></div><div class='controls'><span id='status'>Connessione...</span>"));
        configServer.sendContent(F("<div class='autoscroll-container'><input type='checkbox' id='autoscroll' checked><label for='autoscroll'>Auto-scroll</label></div>"));
        configServer.sendContent(F("<button onclick='clearLog()' class='btn' style='background:#dc3545;margin-left:10px'>Clear</button></div></header>"));
        configServer.sendContent(F("<div id='terminal'></div>"));
        
        // Command Bar
        configServer.sendContent(F("<div class='cmd-bar'>"));
        configServer.sendContent(F("<button onclick=\"sendCmd('status')\" class='btn' style='background:#198754'>STATUS</button>"));
        configServer.sendContent(F("<button onclick=\"sendCmd('help')\" class='btn' style='background:#6c757d'>HELP</button>"));
        configServer.sendContent(F("<button onclick=\"sendCmd('reset')\" class='btn' style='background:#ffc107;color:#000'>RESET WIFI</button>"));
        configServer.sendContent(F("<button onclick=\"sendCmd('restart')\" class='btn' style='background:#dc3545'>RESTART</button>"));
        configServer.sendContent(F("<input type='text' id='cmdInput' class='cmd-input' placeholder='Type command...' onkeydown=\"if(event.key==='Enter') sendCustomCmd()\"><button onclick='sendCustomCmd()' class='btn'>SEND</button></div>"));
        
        // Scripts
        configServer.sendContent(F("<script>const terminal=document.getElementById('terminal');const status=document.getElementById('status');const autoscroll=document.getElementById('autoscroll');"));
        configServer.sendContent(F("function updateLog(){fetch('/api/logs').then(r=>r.json()).then(logs=>{status.innerText='Connected';status.style.color='#4caf50';"));
        configServer.sendContent(F("terminal.innerHTML=logs.map(l=>`<div class='log-line'>${l.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')}</div>`).join('');"));
        configServer.sendContent(F("if(autoscroll.checked)terminal.scrollTop=terminal.scrollHeight;}).catch(e=>{status.innerText='Disconnected';status.style.color='#dc3545'})}"));
        configServer.sendContent(F("function clearLog(){fetch('/api/logs/clear',{method:'POST'}).then(()=>{terminal.innerHTML=''})}"));
        configServer.sendContent(F("function sendCmd(c){fetch('/api/debug/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'command='+encodeURIComponent(c)})"));
        configServer.sendContent(F(".then(r=>{if(r.ok)console.log('Cmd sent:',c);else console.error('Cmd failed')})}"));
        configServer.sendContent(F("function sendCustomCmd(){const i=document.getElementById('cmdInput');const c=i.value.trim();if(c){sendCmd(c);i.value=''}}"));
        configServer.sendContent(F("setInterval(updateLog,500);updateLog();</script></body></html>"));
        
        configServer.sendContent("");
    });
    
    // API Logs
    configServer.on("/api/logs", HTTP_GET, []() {
        configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
        configServer.send(200, "application/json", "");
        
        ChunkedPrint cp(&configServer);
        DevLog.streamJSON(cp);
        cp.flush();
        
        configServer.sendContent(""); // Terminate chunked transmission
    });
    configServer.on("/api/logs/clear", HTTP_POST, []() {
        DevLog.clear();
        configServer.send(200, "text/plain", "Cleared");
    });
    
    // API Debug Command
    configServer.on("/api/debug/command", HTTP_POST, []() {
        if (!configServer.hasArg("command")) { configServer.send(400, "text/plain", "Missing command"); return; }
        String cmd = configServer.arg("command");
        DevLog.printf("[WEB-CMD] %s\n", cmd.c_str());
        
        if (cmd == "status") {
             printGatewayStatus();
        } else if (cmd == "restart") {
             DevLog.println("Rebooting...");
             configServer.send(200, "text/plain", "Rebooting...");
             delay(1000);
             ESP.restart();
             return;
        } else if (cmd == "reset") {
             DevLog.println("Resetting WiFi...");
             resetWiFiConfig();
             return;
        } else if (cmd == "totalreset") {
             DevLog.println("Performing Total Reset...");
             totalReset();
             return;
        } else if (cmd == "help") {
             DevLog.println("Commands: status, restart, reset, totalreset, peers, queue, config, help");
        }
        
        configServer.send(200, "text/plain", "OK");
    });

    configServer.on("/reboot", HTTP_POST, handleReboot);
    configServer.on("/reset_ap", HTTP_POST, handleResetAp);
    configServer.on("/factory_reset", HTTP_POST, handleFactoryReset);
    
    // API
    configServer.on("/api/nodetypes", HTTP_GET, []() {
        configServer.send(200, "application/json", NodeTypeManager::getJsonConfig());
    });
    
    configServer.onNotFound(handleNotFound);
}

bool startWebServer() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP8266-Gateway-Config", "");
    
    // Start DNS Server for Captive Portal
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    // setupWebRoutes() called in setup()
    
    configServer.begin();
    DevLog.println("🌐 Web Server avviato in modalità AP");
    DevLog.print("IP AP: ");
    DevLog.println(WiFi.softAPIP());
    return true;
}

void startStationWebServer() {
    // setupWebRoutes() called in setup()
    configServer.begin();
    DevLog.println("🌐 Web Server avviato in modalità Station");
    DevLog.print("IP Station: ");
    DevLog.println(WiFi.localIP());
    DevLog.print("Open http://");
    DevLog.println(WiFi.localIP());
}

void stopWebServer() {
    configServer.stop();
    dnsServer.stop();
    DevLog.println("🌐 Web Server arrestato");
}

void handleNotFound() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    configServer.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
    configServer.sendHeader("Access-Control-Allow-Headers", "Content-Type, Content-Length, X-Requested-With");
    
    if (configServer.method() == HTTP_OPTIONS) {
        configServer.send(200);
    } else {
        configServer.send(404, "text/plain", "Not Found");
    }
}

// -------------------------------------------------------------------------
// Page Handlers
// -------------------------------------------------------------------------

void handleRoot() {
    printDebugStats("START handleRoot");
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html", "");
    
    configServer.sendContent(F("<!DOCTYPE html><html><head><title>Configurazione Gateway ESP8266</title>"));
    configServer.sendContent(F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    configServer.sendContent(F("<style>"));
    configServer.sendContent(F("body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}"));
    configServer.sendContent(F(".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:800px;margin:0 auto}"));
    configServer.sendContent(F("h1{color:#333;text-align:center;margin-bottom:20px}"));
    configServer.sendContent(F(".section{margin-bottom:20px;border:1px solid #eee;padding:15px;border-radius:8px;background:#fafafa}"));
    configServer.sendContent(F(".section h2{margin-top:0;color:#333;font-size:18px;border-bottom:2px solid #4CAF50;padding-bottom:5px}"));
    configServer.sendContent(F("button{background:#4CAF50;color:white;padding:12px 20px;border:none;border-radius:4px;font-size:16px;cursor:pointer;width:100%;margin-top:10px}"));
    configServer.sendContent(F("button:hover{background:#45a049}"));
    configServer.sendContent(F(".header-flex{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}"));
    configServer.sendContent(F(".console-btn{background:#333;color:white;padding:5px 15px;border:none;border-radius:5px;font-size:14px;cursor:pointer;width:auto;margin:0}"));
    configServer.sendContent(F(".console-btn:hover{background:#000}"));
    configServer.sendContent(F(".reboot-btn{background:#ff9800;margin-top:15px}"));
    configServer.sendContent(F(".ota-btn{background:#2196F3;margin-top:10px}"));
    configServer.sendContent(F(".gw-ota-btn{background:#673ab7;margin-top:10px}"));
    configServer.sendContent(F(".reset-ap-btn{background:#dc3545;margin-top:10px}"));
    configServer.sendContent(F(".settings-btn{background:#607d8b;margin-top:10px}"));
    configServer.sendContent(F(".success-message{background:#d4edda;color:#155724;padding:15px;border-radius:4px;margin-bottom:20px;border:1px solid #c3e6cb}"));
    configServer.sendContent(F("</style></head><body>"));
    configServer.sendContent(F("<div class='container'>"));
    
    // Header Flex
    configServer.sendContent(F("<div class='header-flex'><h1 style='margin:0'>"));
    if (strlen(gateway_id) > 0) {
        configServer.sendContent(String(gateway_id));
    } else {
        configServer.sendContent(F("Gateway ESP8266"));
    }
    configServer.sendContent(F("</h1>"));
    configServer.sendContent(F("<div><button id='dashboardBtn' class='console-btn' style='margin-right:10px;opacity:0.5;background:#0d6efd' disabled>📱 Dashboard</button>"));
    configServer.sendContent(F("<button id='discoverBtn' class='console-btn' style='margin-right:10px;background:#ffc107;color:black' onclick=\"fetch('/api/dashboard_discover',{method:'POST'}).then(()=>{console.log('Discovery sent!');checkDashboard()})\">🔍</button>"));
    configServer.sendContent(F("<button class='console-btn' style='margin-right:10px;background:#6c757d' onclick=\"window.location.href='/settings'\">⚙️ Setup</button>"));
    configServer.sendContent(F("<button class='console-btn' onclick=\"window.open('/debug','_blank','width=800,height=900,scrollbars=yes,resizable=yes')\">🖥️ Console</button></div></div>"));
    
    FSInfo fs_info;
    LittleFS.info(fs_info);
    
    configServer.sendContent(F("<div class='section'><h2>📊 Stato Sistema</h2>"));
    configServer.sendContent(F("<div style='display:grid;grid-template-columns:1fr 1fr;gap:10px'>"));
    configServer.sendContent("<div><b>💾 RAM Libera:</b> " + String(ESP.getFreeHeap()) + " bytes</div>");
    configServer.sendContent("<div><b>🧩 Frammentazione:</b> " + String(ESP.getHeapFragmentation()) + "%</div>");
    configServer.sendContent("<div><b>📂 Spazio Dati:</b> " + String(fs_info.usedBytes) + "/" + String(fs_info.totalBytes) + " bytes</div>");
    configServer.sendContent("<div><b>⚡ CPU Freq:</b> " + String(ESP.getCpuFreqMHz()) + " MHz</div>");
    
    time_t now = time(nullptr);
    String timeStr = "In attesa di NTP...";
    if (now > 100000) {
        struct tm * timeinfo = localtime(&now);
        char buf[20];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d %02d/%02d/%04d", 
                 timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                 timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
        timeStr = String(buf);
    }
    configServer.sendContent("<div><b>🕒 Orario:</b> " + timeStr + "</div>");
    
    configServer.sendContent("<div><b>⏱️ Uptime:</b> " + String(millis() / 60000) + " min</div>");
    configServer.sendContent("<div><b>🔧 Firmware:</b> " + String(FIRMWARE_VERSION) + "</div>");
    configServer.sendContent("<div><b>📅 Build:</b> " + String(BUILD_DATE) + "</div>");
    configServer.sendContent("<div><b>📡 MAC Address:</b> " + WiFi.macAddress() + "</div>");
    configServer.sendContent("<div><b>🌐 IP Address:</b> " + WiFi.localIP().toString() + "</div>");
    configServer.sendContent(F("</div></div>"));

    // Linked Nodes Section (Read-only)
    configServer.sendContent(F("<div class='section'><h2>🔗 Nodi Linkati</h2>"));
    if (peerCount == 0) {
        configServer.sendContent(F("<p>Nessun nodo collegato.</p>"));
    } else {
        configServer.sendContent(F("<div style='overflow-x:auto'><table style='width:100%;border-collapse:collapse;font-size:14px'><thead><tr style='background:#eee;text-align:left'><th style='padding:8px'>Nome</th><th style='padding:8px'>MAC</th><th style='padding:8px'>FW</th><th style='padding:8px'>Stato</th></tr></thead><tbody>"));
        for (int i = 0; i < peerCount; i++) {
             String macStr = macToString(peerList[i].mac);
             bool isOnline = (millis() - peerList[i].lastSeen) < 60000; // 60s timeout
             String statusColor = isOnline ? "#198754" : "#dc3545";
             String statusText = isOnline ? "ONLINE" : "OFFLINE";
             
             configServer.sendContent("<tr><td style='padding:8px;border-bottom:1px solid #ddd'>" + String(peerList[i].nodeId) + 
                                      "</td><td style='padding:8px;border-bottom:1px solid #ddd;font-family:monospace'>" + macStr + 
                                      "</td><td style='padding:8px;border-bottom:1px solid #ddd'>" + String(peerList[i].firmwareVersion) + 
                                      "</td><td style='padding:8px;border-bottom:1px solid #ddd;font-weight:bold;color:" + statusColor + "'>" + statusText + "</td></tr>");
        }
        configServer.sendContent(F("</tbody></table></div>"));
    }
    configServer.sendContent(F("</div>"));
    
    // LED Control Section Removed (Moved to Setup)

    
    if (configServer.hasArg("saved")) {
        configServer.sendContent(F("<div class='success-message'>✅ Configurazione salvata con successo!</div>"));
    }
    
    configServer.sendContent(F("<form action='/reboot' method='post' style='margin-top:10px'><button type='submit' class='reboot-btn'>🔄 Riavvia Gateway</button></form>"));
    configServer.sendContent(F("<form action='/reset_ap' method='post' style='margin-top:10px' onsubmit=\"return confirm('Sei sicuro? Questo cancellerà le credenziali WiFi e riavvierà in modalità AP.')\"><button type='submit' class='reset-ap-btn'>⚠️ Reset WiFi & AP Mode</button></form>"));
    configServer.sendContent(F("<form action='/factory_reset' method='post' style='margin-top:10px' onsubmit=\"return confirm('ATTENZIONE: Questo cancellerà TUTTI i dati (WiFi, Peer, Configurazione) e ripristinerà il gateway alle impostazioni di fabbrica. Sei sicuro?')\"><button type='submit' class='reset-ap-btn' style='background:#b71c1c'>☢️ Factory Reset</button></form>"));
    
    // Dashboard Auto-Discovery Script
    configServer.sendContent(F("<script>"));
    configServer.sendContent(F("function checkDashboard(){fetch('/api/dashboard_info').then(r=>r.json()).then(d=>{"));
    configServer.sendContent(F("const btn=document.getElementById('dashboardBtn');"));
    configServer.sendContent(F("if(d.found){"));
    configServer.sendContent(F("  btn.disabled=false;btn.onclick=()=>window.open('http://'+d.ip,'_blank');btn.style.opacity='1';btn.title='Trovata: '+d.ip;btn.innerText='📱 Dashboard ('+d.ip+')'"));
    configServer.sendContent(F("}else{btn.disabled=true;btn.style.opacity='0.5';btn.title='Non rilevata';btn.innerText='📱 Dashboard (Offline)'}"));
    configServer.sendContent(F("}).catch(e=>console.error(e))}"));
    configServer.sendContent(F("setInterval(checkDashboard,5000);checkDashboard();"));
    configServer.sendContent(F("</script>"));

    configServer.sendContent(F("</div></div></body></html>"));
    configServer.sendContent(""); // Terminate chunked response
    
    printDebugStats("END handleRoot");
}

void handleSettings() {
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html", "");
    
    configServer.sendContent(F("<!DOCTYPE html><html><head><title>Impostazioni di Rete</title>"));
    configServer.sendContent(F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    configServer.sendContent(F("<style>"));
    configServer.sendContent(F("body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}"));
    configServer.sendContent(F(".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:800px;margin:0 auto}"));
    configServer.sendContent(F("h1{color:#333;text-align:center;margin-bottom:20px}"));
    configServer.sendContent(F(".form-group{margin-bottom:15px}"));
    configServer.sendContent(F("label{display:block;margin:10px 0 5px 0;font-weight:bold;color:#555}"));
    configServer.sendContent(F("input,select{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;font-size:14px;box-sizing:border-box}"));
    configServer.sendContent(F("button{background:#4CAF50;color:white;padding:12px 20px;border:none;border-radius:4px;font-size:16px;cursor:pointer;width:100%;margin-top:10px}"));
    configServer.sendContent(F(".section{margin-bottom:20px;border:1px solid #eee;padding:15px;border-radius:8px;background:#fafafa}"));
    configServer.sendContent(F(".section h2{margin-top:0;color:#333;font-size:18px;border-bottom:2px solid #4CAF50;padding-bottom:5px}"));
    configServer.sendContent(F(".back-btn{background:#6c757d;margin-top:10px}"));
    configServer.sendContent(F(".radio-group { display: flex; flex-direction: column; gap: 10px; margin-bottom: 15px; }"));
    configServer.sendContent(F(".radio-group label { font-weight: normal; cursor: pointer; display: flex; align-items: center; gap: 10px; margin-bottom: 0; }"));
    configServer.sendContent(F("input[type='radio'], input[type='checkbox'] { width: auto; margin: 0; cursor: pointer; transform: scale(1.2); }"));
    configServer.sendContent(F("</style>"));
    configServer.sendContent(F("<script>function toggleIPFields(enable) {"));
    configServer.sendContent(F("  var container = document.getElementById('static_fields_container');"));
    configServer.sendContent(F("  if(container) container.style.display = enable ? 'block' : 'none';"));
    configServer.sendContent(F("}</script></head><body>"));
    configServer.sendContent(F("<div class='container'><h1>⚙️ Impostazioni di Rete</h1>"));
    
    configServer.sendContent(F("<form action='/save' method='post'>"));
    
    configServer.sendContent(F("<div class='section'><h2>📶 Configurazione WiFi</h2>"));
    configServer.sendContent(F("<div class='form-group'><label for='wifi_ssid'>Rete WiFi:</label><select id='wifi_ssid' name='wifi_ssid' required>"));
    streamNetworksList(configServer);
    configServer.sendContent(F("</select></div>"));
    
    String wifiPassValue = (strlen(saved_wifi_password) > 0) ? String(saved_wifi_password) : String(wifi_password);
    configServer.sendContent(F("<div class='form-group'><label for='wifi_password'>Password WiFi:</label><input type='text' id='wifi_password' name='wifi_password' value='"));
    configServer.sendContent(wifiPassValue);
    configServer.sendContent(F("' placeholder='Inserire la password del WIFI'></div></div>"));
    
    configServer.sendContent(F("<div class='section'><h2>🔌 Configurazione MQTT</h2>"));
    configServer.sendContent(F("<div class='form-group'><label for='mqtt_server'>Server MQTT:</label><input type='text' id='mqtt_server' name='mqtt_server' value='"));
    configServer.sendContent(String(mqtt_server));
    configServer.sendContent(F("' required></div>"));
    configServer.sendContent(F("<div class='form-group'><label for='mqtt_port'>Porta MQTT:</label><input type='number' id='mqtt_port' name='mqtt_port' value='"));
    configServer.sendContent(String(mqtt_port));
    configServer.sendContent(F("' required></div>"));
    configServer.sendContent(F("<div class='form-group'><label for='mqtt_user'>Username MQTT (opzionale):</label><input type='text' id='mqtt_user' name='mqtt_user' value='"));
    configServer.sendContent(String(mqtt_user));
    configServer.sendContent(F("' placeholder='Inserire se necessario'></div>"));
    configServer.sendContent(F("<div class='form-group'><label for='mqtt_password'>Password MQTT (opzionale):</label><input type='text' id='mqtt_password' name='mqtt_password' value='"));
    configServer.sendContent(String(mqtt_password));
    configServer.sendContent(F("' placeholder='Inserire se necessario'></div>"));
    configServer.sendContent(F("<div class='form-group'><label for='mqtt_topic_prefix'>Prefisso Topic MQTT:</label><input type='text' id='mqtt_topic_prefix' name='mqtt_topic_prefix' value='"));
    configServer.sendContent(String(mqtt_topic_prefix));
    configServer.sendContent(F("' required></div></div>"));
    
    configServer.sendContent(F("<div class='section'><h2>🕒 Configurazione Orario (NTP)</h2>"));
    configServer.sendContent(F("<div class='form-group'><label for='ntp_server'>Server NTP:</label><input type='text' id='ntp_server' name='ntp_server' value='"));
    configServer.sendContent(String(ntp_server));
    configServer.sendContent(F("' required></div>"));
    
    int tz_hours = gmt_offset_sec / 3600;
    bool dst_active = daylight_offset_sec > 0;
    
    configServer.sendContent(F("<div class='form-group'><label for='timezone_hours'>Fuso Orario (Ore da GMT):</label><input type='number' id='timezone_hours' name='timezone_hours' value='"));
    configServer.sendContent(String(tz_hours));
    configServer.sendContent(F("' required><small>Es. 1 per Italia</small></div>"));
    
    configServer.sendContent(F("<div class='form-group'><label>Ora Legale (Estate):</label>"));
    configServer.sendContent(F("<div class='radio-group'><label><input type='checkbox' id='dst_active' name='dst_active' value='1' "));
    if (dst_active) configServer.sendContent(F("checked"));
    configServer.sendContent(F("> Attiva (+1 ora)</label></div></div></div>"));

    configServer.sendContent(F("<div class='section'><h2>⚙️ Configurazione Gateway</h2>"));
    configServer.sendContent(F("<div class='form-group'><label for='gateway_id'>ID Gateway:</label><input type='text' id='gateway_id' name='gateway_id' value='"));
    configServer.sendContent(String(gateway_id));
    configServer.sendContent(F("' required></div>"));
    
    configServer.sendContent(F("<div class='form-group'><label>LED di Stato:</label>"));
    configServer.sendContent(F("<div class='radio-group'><label><input type='checkbox' id='led_enabled' name='led_enabled' value='1' "));
    if (led_enabled) configServer.sendContent(F("checked"));
    configServer.sendContent(F("> Attiva LED</label></div></div></div>"));
    
    configServer.sendContent(F("<div class='section'><h2>🌐 Configurazione IP</h2>"));
    configServer.sendContent(F("<div class='form-group'><label>Modalità IP:</label><div class='radio-group'>"));
    String dhcpChecked = (strcmp(network_mode, "dhcp") == 0) ? "checked" : "";
    String staticChecked = (strcmp(network_mode, "static") == 0) ? "checked" : "";
    configServer.sendContent("<label><input type='radio' id='dhcp_mode' name='network_mode' value='dhcp' " + dhcpChecked + " onclick='toggleIPFields(false)'> 🌐 DHCP Automatico</label>");
    configServer.sendContent("<label><input type='radio' id='static_mode' name='network_mode' value='static' " + staticChecked + " onclick='toggleIPFields(true)'> ⚙️ IP Statico</label>");
    configServer.sendContent(F("</div></div>"));
    
    String staticIpValue = (static_ip[0]) ? String(static_ip) : "192.168.1.19";
    String staticGatewayValue = (static_gateway[0]) ? String(static_gateway) : "192.168.1.1";
    String staticSubnetValue = (static_subnet[0]) ? String(static_subnet) : "255.255.255.0";
    String staticDnsValue = (static_dns[0]) ? String(static_dns) : "8.8.8.8";
    
    String displayStyle = (strcmp(network_mode, "static") == 0) ? "block" : "none";
    configServer.sendContent("<div id='static_fields_container' style='display:" + displayStyle + "'>");
    
    configServer.sendContent("<div class='form-group'><label for='static_ip'>IP Statico:</label><input type='text' id='static_ip' name='static_ip' value='" + staticIpValue + "'></div>");
    configServer.sendContent("<div class='form-group'><label for='static_gateway'>Gateway:</label><input type='text' id='static_gateway' name='static_gateway' value='" + staticGatewayValue + "'></div>");
    configServer.sendContent("<div class='form-group'><label for='static_subnet'>Subnet Mask:</label><input type='text' id='static_subnet' name='static_subnet' value='" + staticSubnetValue + "'></div>");
    configServer.sendContent("<div class='form-group'><label for='static_dns'>DNS:</label><input type='text' id='static_dns' name='static_dns' value='" + staticDnsValue + "'></div>");
    
    configServer.sendContent(F("</div></div>"));
    
    configServer.sendContent(F("<div class='btn-group'>"));
    configServer.sendContent(F("<button type='submit'>💾 Salva</button>"));
    configServer.sendContent(F("<button type='button' class='back-btn' onclick=\"window.location.href='/'\">⬅ Home</button>"));
    configServer.sendContent(F("</div></form></div></body></html>"));
    configServer.sendContent(""); // Terminate chunked response
}

void handleSave() {
    if (configServer.hasArg("mqtt_server")) {
        DynamicJsonDocument doc(2048);
        doc["wifi_ssid"] = configServer.arg("wifi_ssid");
        doc["wifi_password"] = configServer.arg("wifi_password");
        doc["mqtt_server"] = configServer.arg("mqtt_server");
        doc["mqtt_port"] = configServer.arg("mqtt_port").toInt();
        doc["mqtt_user"] = configServer.arg("mqtt_user");
        doc["mqtt_password"] = configServer.arg("mqtt_password");
        doc["mqtt_topic_prefix"] = configServer.arg("mqtt_topic_prefix");
        
        doc["ntp_server"] = configServer.arg("ntp_server");
        
        // Handle simplified time settings
        if (configServer.hasArg("timezone_hours")) {
            int tz_hours = configServer.arg("timezone_hours").toInt();
            doc["gmt_offset_sec"] = tz_hours * 3600;
        } else if (configServer.hasArg("gmt_offset_sec")) {
            doc["gmt_offset_sec"] = configServer.arg("gmt_offset_sec").toInt();
        }
        
        if (configServer.hasArg("dst_active")) {
             doc["daylight_offset_sec"] = 3600;
        } else {
             // If timezone_hours was sent, but dst_active wasn't, it means unchecked (0)
             // If legacy fields were used, we might need to handle differently, but here we assume new form
             if (configServer.hasArg("timezone_hours")) {
                 doc["daylight_offset_sec"] = 0;
             } else if (configServer.hasArg("daylight_offset_sec")) {
                 doc["daylight_offset_sec"] = configServer.arg("daylight_offset_sec").toInt();
             }
        }

        doc["gateway_id"] = configServer.arg("gateway_id");
        
        if (configServer.hasArg("led_enabled")) {
             doc["led_enabled"] = true;
             led_enabled = true;
        } else {
             doc["led_enabled"] = false;
             led_enabled = false;
        }

        doc["network_mode"] = configServer.arg("network_mode");
        doc["static_ip"] = configServer.arg("static_ip");
        doc["static_gateway"] = configServer.arg("static_gateway");
        doc["static_subnet"] = configServer.arg("static_subnet");
        doc["static_dns"] = configServer.arg("static_dns");
        
        File configFile = LittleFS.open("/config.json", "w");
        if (configFile) {
            serializeJson(doc, configFile);
            configFile.close();
            
            configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
            configServer.send(200, "text/html", "");
            configServer.sendContent(F("<!DOCTYPE html><html><head><title>Configurazione Salvata</title>"));
            configServer.sendContent(F("<meta name='viewport' content='width=device-width, initial-scale=1'><meta charset='UTF-8'>"));
            configServer.sendContent(F("<style>body{font-family:Arial;margin:20px;background:#f0f0f0}.container{max-width:400px;margin:auto;background:white;padding:20px;border-radius:10px;text-align:center}"));
            configServer.sendContent(F(".success{color:#28a745;font-size:24px}.info{margin:20px 0;padding:15px;background:#d4edda;border-radius:5px}</style>"));
            configServer.sendContent(F("</head><body><div class='container'><h2 class='success'>✅ Configurazione Salvata!</h2>"));
            configServer.sendContent(F("<div class='info'><p>I dati sono stati salvati correttamente.</p></div>"));
            configServer.sendContent(F("<p>Il gateway si riavvierà automaticamente tra <span id='c'>5</span> secondi...</p>"));
            configServer.sendContent(F("<script>let c=5;const t=setInterval(()=>{c--;document.getElementById('c').innerText=c;if(c<=0){clearInterval(t);fetch('/reboot',{method:'POST'});document.body.innerHTML='<div class=\"container\"><h2 class=\"success\">🔄 Riavvio in corso...</h2><p>Il dispositivo si sta riavviando. Verrai reindirizzato alla Home Page tra 15 secondi...</p></div>';setTimeout(()=>{window.location.href='/'},15000);}},1000);</script>"));
            configServer.sendContent(F("</div></body></html>"));
            configServer.sendContent(""); // Terminate chunked response
        } else {
            configServer.send(500, "text/plain", "Errore salvataggio configurazione");
        }
    } else {
        configServer.send(400, "text/plain", "Parametri mancanti");
    }
}

// -------------------------------------------------------------------------
// Action Handlers
// -------------------------------------------------------------------------

void handleResetAp() {
    configServer.send(200, "text/plain", "Resetting WiFi...");
    delay(100);
    resetWiFiConfig();
}

void handleFactoryReset() {
    configServer.send(200, "text/plain", "Factory Reset...");
    delay(100);
    totalReset();
}

void handleReboot() {
    configServer.send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void streamNetworksList(ESP8266WebServer& server) {
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        server.sendContent(F("<option value=\"\">Scansione in corso...</option>"));
        return;
    }
    int n = WiFi.scanComplete();
    if (n < 0) {
        WiFi.scanNetworks(true);
        server.sendContent(F("<option value=\"\">Avvio scansione...</option>"));
        return;
    }
    if (n == 0) {
        server.sendContent(F("<option value=\"\">Nessuna rete trovata</option>"));
    } else {
        int maxNetworks = min(n, 15);
        const char* targetSSID = (strlen(saved_wifi_ssid) > 0) ? saved_wifi_ssid : wifi_ssid;
        for (int i = 0; i < maxNetworks; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 0) {
                ssid.replace("\"", "&quot;");
                server.sendContent(F("<option value=\""));
                server.sendContent(ssid);
                server.sendContent(F("\""));
                if (strcmp(targetSSID, ssid.c_str()) == 0) server.sendContent(F(" selected"));
                server.sendContent(F(">"));
                server.sendContent(ssid);
                server.sendContent(F("</option>"));
            }
        }
    }
}

void resetWiFiConfig() {
    mqttClient.disconnect();
    memset(saved_wifi_ssid, 0, sizeof(saved_wifi_ssid));
    memset(saved_wifi_password, 0, sizeof(saved_wifi_password));
    wifi_credentials_loaded = false;
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, configFile);
            configFile.close();
            if (!error) {
                doc.remove("wifi_ssid");
                doc.remove("wifi_password");
                File newConfigFile = LittleFS.open("/config.json", "w");
                if (newConfigFile) {
                    serializeJson(doc, newConfigFile);
                    newConfigFile.close();
                }
            }
        }
    }
    delay(2000);
    ESP.restart();
}

void totalReset() {
    // Cancella tutti i peer ESP-NOW usando PeerHandler
    clearAllPeers();
    
    // Formatta completamente LittleFS
    LittleFS.format();
    
    // Riavvia l'ESP8266
    ESP.restart();
}
