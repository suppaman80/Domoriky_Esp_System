#include "WebHandler.h"
#include "Config.h"
#include "PeerHandler.h"
#include "MqttHandler.h"
#include "EspNowHandler.h"
#include "NodeTypeManager.h"
#include "HaDiscovery.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "version.h"
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

// Global OTA Status Tracker
OtaStatus globalOtaStatus = {"", "IDLE", 0, "", 0};

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
    configServer.on("/api/dashboard_discover", HTTP_OPTIONS, sendCors);
    configServer.on("/api/node/remove", HTTP_OPTIONS, sendCors);
    configServer.on("/api/node/restart", HTTP_OPTIONS, sendCors);
    configServer.on("/api/node/reset", HTTP_OPTIONS, sendCors);
    configServer.on("/api/network_discovery", HTTP_OPTIONS, sendCors);
    configServer.on("/api/node/ha_discovery", HTTP_OPTIONS, sendCors);
    configServer.on("/api/ping_network", HTTP_OPTIONS, sendCors);
    
    configServer.on("/", HTTP_GET, handleRoot);
    configServer.on("/api/dashboard_info", HTTP_GET, handleApiDashboardInfo);
    // Route /api/set_led removed
    configServer.on("/api/dashboard_discover", HTTP_POST, []() {
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        sendDashboardDiscovery();
        configServer.send(200, "text/plain", "Discovery Sent");
    });
    configServer.on("/save", HTTP_POST, handleSave);
    configServer.on("/settings", HTTP_GET, handleSettings);
    configServer.on("/nodes", HTTP_GET, handleNodes);
    configServer.on("/ota_manager", HTTP_GET, handleOtaManager);
    configServer.on("/gateway_ota", HTTP_GET, handleGatewayOTA);
    
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
    configServer.on("/api/nodes_list", HTTP_GET, handleApiNodesList);
    configServer.on("/api/nodetypes", HTTP_GET, []() {
        configServer.send(200, "application/json", NodeTypeManager::getJsonConfig());
    });
    configServer.on("/api/node/remove", HTTP_POST, handleDeleteNode);
    configServer.on("/api/node/restart", HTTP_POST, handleApiNodeRestart);
    configServer.on("/api/node/reset", HTTP_POST, handleApiNodeReset);
    configServer.on("/api/ping_node", HTTP_GET, handleApiPingNode);
    configServer.on("/api/node_status", HTTP_GET, handleApiNodeStatus);
    configServer.on("/api/network_discovery", HTTP_POST, handleNetworkDiscovery);
    configServer.on("/api/node/ha_discovery", HTTP_POST, handleApiForceHaDiscovery);
    configServer.on("/api/ping_network", HTTP_POST, handlePingNetwork);
    configServer.on("/api/ota_status", HTTP_GET, handleApiOtaStatus);
    
    // OTA Handlers

    configServer.on("/trigger_ota", HTTP_POST, handleTriggerOta);
    // Fix CORS Preflight for update_gateway
    configServer.on("/update_gateway", HTTP_OPTIONS, [](){
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        configServer.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
        configServer.sendHeader("Access-Control-Allow-Headers", "Content-Type, Content-Length, X-Requested-With");
        configServer.sendHeader("Access-Control-Max-Age", "86400");
        configServer.send(200);
    });
    configServer.on("/update_gateway", HTTP_POST, [](){
        configServer.sendHeader("Access-Control-Allow-Origin", "*");
        configServer.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE"); // Aggiunto per sicurezza
        if (Update.hasError()) {
            configServer.send(500, "text/plain", "Update Failed");
        } else {
            configServer.send(200, "text/plain", "Update Success! Rebooting...");
            delay(1000);
            ESP.restart();
        }
    }, handleGatewayUpdate);
    
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
        return;
    }

    // CAPTIVE PORTAL REDIRECT
    // Se siamo in modalità AP e la richiesta non è per l'IP del gateway,
    // reindirizza alla home page.
    if (WiFi.getMode() & WIFI_AP) {
        String hostHeader = configServer.hostHeader();
        String softApIP = WiFi.softAPIP().toString();
        
        // Se l'host richiesto NON è l'IP del SoftAP, reindirizza
        if (hostHeader != softApIP && hostHeader != (String(gateway_id) + ".local")) {
            DevLog.printf("CP Redirect: %s -> %s\n", hostHeader.c_str(), softApIP.c_str());
            configServer.sendHeader("Location", String("http://") + softApIP + "/", true);
            configServer.send(302, "text/plain", "");
            configServer.client().stop();
            return;
        }
    }

    configServer.send(404, "text/plain", "Not Found");
}

// -------------------------------------------------------------------------
// Page Handlers
// -------------------------------------------------------------------------

void handleRoot() {
    // SE SIAMO IN MODALITÀ AP (CONFIGURAZIONE), MOSTRA DIRETTAMENTE LA PAGINA DI SETUP
    if (WiFi.getMode() & WIFI_AP) {
        handleSettings();
        return;
    }

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
    
    // LED Control Section Removed (Moved to Setup)

    
    if (configServer.hasArg("saved")) {
        configServer.sendContent(F("<div class='success-message'>✅ Configurazione salvata con successo!</div>"));
    }
    
    configServer.sendContent(F("<div class='section'><h2>🛠️ Strumenti</h2>"));
    configServer.sendContent(F("<button class='settings-btn' style='background:#009688;margin-bottom:10px' onclick=\"window.location.href='/nodes'\">📦 Gestione Nodi</button>"));
    configServer.sendContent(F("<div style='display:grid;grid-template-columns:1fr 1fr;gap:10px'>"));
    configServer.sendContent(F("<button class='ota-btn' onclick=\"window.location.href='/ota_manager'\">🚀 Node OTA</button>"));
    configServer.sendContent(F("<input type='file' id='gw_update' accept='.bin' style='display:none' onchange='uploadGatewayFw(this)'>"));
    configServer.sendContent(F("<button class='gw-ota-btn' onclick=\"document.getElementById('gw_update').click()\">☁️ Gateway OTA</button>"));
    configServer.sendContent(F("</div>"));
    configServer.sendContent(F("<form action='/reboot' method='post' style='margin-top:10px'><button type='submit' class='reboot-btn'>🔄 Riavvia Gateway</button></form>"));
    configServer.sendContent(F("<form action='/reset_ap' method='post' style='margin-top:10px' onsubmit=\"return confirm('Sei sicuro? Questo cancellerà le credenziali WiFi e riavvierà in modalità AP.')\"><button type='submit' class='reset-ap-btn'>⚠️ Reset WiFi & AP Mode</button></form>"));
    configServer.sendContent(F("<form action='/factory_reset' method='post' style='margin-top:10px' onsubmit=\"return confirm('ATTENZIONE: Questo cancellerà TUTTI i dati (WiFi, Peer, Configurazione) e ripristinerà il gateway alle impostazioni di fabbrica. Sei sicuro?')\"><button type='submit' class='reset-ap-btn' style='background:#b71c1c'>☢️ Factory Reset</button></form>"));
    
    // Dashboard Auto-Discovery Script
    configServer.sendContent(F("<script>"));
    configServer.sendContent(F("function uploadGatewayFw(input){if(!input.files.length)return;if(!confirm('Sei sicuro di voler aggiornare il firmware del Gateway? Il dispositivo si riavvierà.')){input.value='';return;}var d=new FormData();d.append('update',input.files[0]);fetch('/update_gateway',{method:'POST',body:d}).then(r=>{if(r.ok){alert('Aggiornamento riuscito! Il dispositivo si riavvierà...');setTimeout(()=>location.reload(),10000);}else{alert('Errore aggiornamento');input.value='';}}).catch(e=>{alert('Errore: '+e);input.value='';});}"));
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

void handleNodes() {
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html", "");
    configServer.sendContent(F("<!DOCTYPE html><html><head><title>Gestione Nodi</title>"));
    configServer.sendContent(F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));
    configServer.sendContent(F("<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}"));
    configServer.sendContent(F(".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:800px;margin:0 auto}"));
    configServer.sendContent(F("h1{color:#333;text-align:center}table{width:100%;border-collapse:collapse;margin-top:20px}"));
    configServer.sendContent(F("th,td{padding:12px;text-align:left;border-bottom:1px solid #ddd}th{background-color:#4CAF50;color:white}"));
    configServer.sendContent(F("tr:hover{background-color:#f5f5f5}.btn{padding:6px 12px;border:none;border-radius:4px;cursor:pointer;margin-right:5px;color:white}"));
    configServer.sendContent(F(".btn-restart{background:#ff9800}.btn-reset{background:#f44336}.btn-remove{background:#607d8b}"));
    configServer.sendContent(F(".btn-disc{background:#2196F3;width:100%;margin-bottom:10px;padding:12px}.btn-ping{background:#9c27b0;width:100%;margin-bottom:10px;padding:12px}"));
    configServer.sendContent(F(".back-btn{background:#6c757d;width:100%;padding:12px;margin-top:20px}"));
    configServer.sendContent(F(".status-online{color:green;font-weight:bold}.status-offline{color:red;font-weight:bold}</style>"));
    
    configServer.sendContent(F("<script>function loadNodes(){fetch('/api/nodes_list').then(r=>r.json()).then(d=>{let h='';d.nodes.forEach(n=>{"));
    configServer.sendContent(F("h+='<tr><td>'+n.id+'</td><td>'+n.type+'</td><td>'+n.mac+'</td><td>'+n.firmware+'</td>';"));
    configServer.sendContent(F("h+='<td class=\"'+(n.online?'status-online':'status-offline')+'\">'+(n.online?'ONLINE':'OFFLINE')+'</td>';"));
    configServer.sendContent(F("h+='<td><button class=\"btn btn-restart\" onclick=\"api(\\'restart\\',\\''+n.id+'\\')\">🔄</button>';"));
    configServer.sendContent(F("h+='<button class=\"btn btn-reset\" onclick=\"if(confirm(\\'Reset WiFi?\\'))api(\\'reset\\',\\''+n.id+'\\')\">⚠️</button>';"));
    configServer.sendContent(F("h+='<button class=\"btn btn-remove\" onclick=\"if(confirm(\\'Remove?\\'))remove(\\''+n.mac+'\\')\">🗑️</button></td></tr>';});"));
    configServer.sendContent(F("document.getElementById('list').innerHTML=h;});}"));
    configServer.sendContent(F("function api(act,id){fetch('/api/node/'+act+'?nodeId='+id,{method:'POST'}).then(r=>r.json()).then(d=>alert(d.message||d.error));}"));
    configServer.sendContent(F("function remove(mac){fetch('/api/node/remove?mac='+mac,{method:'POST'}).then(r=>r.json()).then(d=>{alert(d.message||d.error);loadNodes();});}"));
    configServer.sendContent(F("function disc(){fetch('/api/network_discovery',{method:'POST'}).then(r=>r.json()).then(d=>alert(d.message));}"));
    configServer.sendContent(F("function pingNet(){fetch('/api/ping_network',{method:'POST'}).then(r=>r.json()).then(d=>alert(d.message));}"));
    configServer.sendContent(F("setInterval(loadNodes,5000);window.onload=loadNodes;</script>"));
    
    configServer.sendContent(F("</head><body><div class='container'><h1>📦 Gestione Nodi</h1>"));
    configServer.sendContent(F("<button class='btn btn-disc' onclick='disc()'>🔎 Avvia Discovery</button>"));
    configServer.sendContent(F("<button class='btn btn-ping' onclick='pingNet()'>📡 Ping Network</button>"));
    configServer.sendContent(F("<table><thead><tr><th>ID</th><th>Tipo</th><th>MAC</th><th>FW</th><th>Stato</th><th>Azioni</th></tr></thead><tbody id='list'></tbody></table>"));
    configServer.sendContent(F("<button class='btn back-btn' onclick=\"location.href='/'\">⬅ Torna alla Home</button></div></body></html>"));
    configServer.sendContent(""); // Terminate chunked response
}

void handleOtaManager() {
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html", "");
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;margin:20px;background:#f4f4f4}.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px #0002;max-width:600px;margin:auto}h1,h3{color:#333;margin-bottom:10px}input,select,button{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}button{background:#007bff;color:#fff;border:none;cursor:pointer}button:hover{background:#0056b3}.back{background:#6c757d}.success{color:#28a745}.error{color:#dc3545}"
    "#px{display:none;margin-top:20px;background:#e9ecef;border-radius:4px}#pb{height:20px;background:#28a745;width:0%;border-radius:4px;color:#fff;text-align:center;font-size:12px;line-height:20px;transition:width .2s}"
    "#ota-status-area{display:none;margin-top:20px;padding:15px;background:#f8f9fa;border-radius:4px;border:1px solid #ddd}#ota-log{font-family:monospace;font-size:12px;height:100px;overflow-y:auto;background:#333;color:#0f0;padding:5px;margin-top:10px;border-radius:3px}</style>"
    "<script>"
    "function flashNode(e){e.preventDefault();var s=document.getElementById('nodeId');var id=s.value;var u=document.getElementById('url').value;if(!id)return alert('Seleziona nodo');if(!u)return alert('URL Firmware mancante');"
    "var btn=document.getElementById('flashBtn');btn.disabled=true;btn.innerText='Avvio...';"
    "var x=document.getElementById('px-node');var b=document.getElementById('pb-node');x.style.display='block';b.style.width='0%';b.innerText='0%';"
    "var area=document.getElementById('ota-status-area');var badge=document.getElementById('ota-badge');var log=document.getElementById('ota-log');area.style.display='block';badge.innerText='TRIGGERED';"
    
    "fetch('/trigger_ota?nodeId='+encodeURIComponent(id)+'&url='+encodeURIComponent(u),{method:'POST'}).then(r=>r.json()).then(d=>{"
    "  if(d.status!='ok'){alert('Errore avvio');btn.disabled=false;return;}"
    "  var poll=setInterval(function(){"
    "    fetch('/api/ota_status').then(r=>r.json()).then(s=>{"
    "      badge.innerText=s.status; log.innerText=s.lastMessage + '\\n' + log.innerText;"
    "      if(s.progress!==undefined){ b.style.width=s.progress+'%'; b.innerText=s.progress+'%'; }"
    "      if(s.status==='SUCCESS'||s.status==='OTA_DONE'){ clearInterval(poll); b.style.width='100%'; b.innerText='100%'; b.style.background='#28a745'; alert('Aggiornamento Completato!'); location.reload(); }"
    "      else if(s.status.includes('FAIL')||s.status.includes('ERR')){ clearInterval(poll); b.style.background='#dc3545'; btn.disabled=false; btn.innerText='🚀 Flash Node'; alert('Aggiornamento Fallito!'); }"
    "    });"
    "  },1000);"
    "}).catch(e=>{alert('Errore comunicazione');btn.disabled=false;});"
    "}"
    "</script>"
    "</head><body><div class='card'><h1>🚀 Node OTA Manager</h1>");
    
    html += F("<div style='background:#f8f9fa;padding:10px;border:1px solid #ddd;font-size:.9em;border-radius:4px'><b>Heap:</b> ");
    html += String(ESP.getFreeHeap()) + F("b | <b>Up:</b> ") + String(millis()/60000) + "m</div>";
    
    configServer.sendContent(html);
    
    // Updated Flash Form: AJAX and Progress Bar
    html = F("<h3>Flash Node (Manual)</h3><p>Per aggiornare i nodi, usa preferibilmente la Dashboard.</p><form onsubmit='flashNode(event)'><label>Seleziona Nodo:</label><select id='nodeId' name='nodeId'>");
    for (int i = 0; i < peerCount; i++) {
        html += "<option value='" + String(peerList[i].nodeId) + "'>" + String(peerList[i].nodeId) + " (v" + String(peerList[i].firmwareVersion) + ")</option>";
    }
    html += F("</select><label>URL Firmware:</label><input type='text' id='url' name='url' placeholder='http://192.168.x.x/firmware.bin' required>");
    html += F("<button id='flashBtn'>🚀 Flash Node</button></form>");
    
    // Progress Area for Node OTA
    html += F("<div id='px-node' style='display:none;margin-top:20px;background:#e9ecef;border-radius:4px'><div id='pb-node' style='height:20px;background:#1a73e8;width:0%;border-radius:4px;color:#fff;text-align:center;font-size:12px;line-height:20px;transition:width .2s'>0%</div></div>");
    
    // Status Area
    html += F("<div id='ota-status-area'><div style='font-weight:bold;margin-bottom:5px'>Stato: <span id='ota-badge'>IDLE</span></div><div id='ota-log'>Waiting...</div></div>");
    
    html += F("<button class='back' onclick=\"location.href='/'\">⬅ Torna alla Home</button></div></body></html>");
    configServer.sendContent(html);
    configServer.sendContent(""); // Terminate chunked response
}

void handleGatewayOTA() {
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "text/html", "");
    configServer.sendContent(F("<!DOCTYPE html><html><head><title>Gateway OTA</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"));
    configServer.sendContent(F("<style>body{font-family:sans-serif;margin:20px;background:#f4f4f4}.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px #0002;max-width:600px;margin:auto}h1,h3{color:#333;margin-bottom:10px}input,select,button{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:4px;box-sizing:border-box}button{background:#673ab7;color:#fff;border:none;cursor:pointer}button:hover{background:#512da8}.back{background:#6c757d}.success{color:#28a745}.error{color:#dc3545}#px{display:none;margin-top:20px;background:#e9ecef;border-radius:4px}#pb{height:20px;background:#673ab7;width:0%;border-radius:4px;color:#fff;text-align:center;font-size:12px;line-height:20px;transition:width .2s}</style>"));
    configServer.sendContent(F("<script>function up(e){e.preventDefault();var i=document.getElementById('f'),b=document.getElementById('pb'),x=document.getElementById('px');if(!i.files.length)return alert('File?');var d=new FormData();d.append('update',i.files[0]);var r=new XMLHttpRequest();r.open('POST','/update_gateway',true);x.style.display='block';r.upload.onprogress=function(e){if(e.lengthComputable){var p=(e.loaded/e.total)*100;b.style.width=p+'%';b.innerText=Math.round(p)+'%'}};r.onload=function(){if(r.status==200){b.style.background='#28a745';b.innerText='Success! Rebooting...';setTimeout(function(){location.href='/'},15000)}else{x.style.display='none';alert('Err '+r.status)}};r.onerror=function(){x.style.display='none';alert('Net Err')};r.send(d)}</script>"));
    configServer.sendContent(F("</head><body><div class='card'><h1>☁️ Gateway OTA</h1>"));
    configServer.sendContent(F("<div style='background:#f8f9fa;padding:10px;border:1px solid #ddd;font-size:.9em;border-radius:4px;margin-bottom:20px'>"));
    configServer.sendContent("<b>Current Version:</b> " + String(FIRMWARE_VERSION) + "<br>");
    configServer.sendContent("<b>Build:</b> " + String(BUILD_DATE) + " " + String(BUILD_TIME) + "</div>");
    configServer.sendContent(F("<h3>Upload Firmware (.bin)</h3><form onsubmit='up(event)'><input type='file' id='f' name='update' accept='.bin' required><button>📤 Update Gateway</button></form><div id='px'><div id='pb'>0%</div></div>"));
    configServer.sendContent(F("<br><button class='back' onclick=\"location.href='/'\">⬅ Home</button></div></body></html>"));
    configServer.sendContent(""); // Terminate chunked response
}

// -------------------------------------------------------------------------
// API Handlers
// -------------------------------------------------------------------------

void handleApiNodesList() {
    configServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    configServer.send(200, "application/json", "");
    configServer.sendContent(F("{\"nodes\":["));
    for (int i = 0; i < peerCount; i++) {
        if (i > 0) configServer.sendContent(F(","));
        String nodeJson = F("{");
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 peerList[i].mac[0], peerList[i].mac[1], peerList[i].mac[2],
                 peerList[i].mac[3], peerList[i].mac[4], peerList[i].mac[5]);
        nodeJson += F("\"mac\":\""); nodeJson += macStr;
        nodeJson += F("\",\"id\":\""); nodeJson += peerList[i].nodeId;
        nodeJson += F("\",\"type\":\""); nodeJson += peerList[i].nodeType;
        nodeJson += F("\",\"firmware\":\""); nodeJson += (strlen(peerList[i].firmwareVersion) > 0) ? peerList[i].firmwareVersion : "-";
        nodeJson += F("\",\"online\":"); nodeJson += peerList[i].isOnline ? F("true") : F("false");
        nodeJson += F("}");
        configServer.sendContent(nodeJson);
    }
    configServer.sendContent(F("]}"));
    configServer.sendContent(""); // Terminate chunked response
}

void handleApiNodeRestart() {
    if (!configServer.hasArg("nodeId")) { configServer.send(400, "application/json", "{\"error\":\"Missing nodeId\"}"); return; }
    String nodeId = configServer.arg("nodeId");
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == nodeId) {
            espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "RESTART", "", "COMMAND", gateway_id);
            configServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Restart command sent\"}");
            return;
        }
    }
    configServer.send(404, "application/json", "{\"error\":\"Node not found\"}");
}

void handleApiNodeReset() {
    if (!configServer.hasArg("nodeId")) { configServer.send(400, "application/json", "{\"error\":\"Missing nodeId\"}"); return; }
    String nodeId = configServer.arg("nodeId");
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == nodeId) {
            espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "RESET_WIFI", "", "COMMAND", gateway_id);
            configServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Reset WiFi command sent\"}");
            return;
        }
    }
    configServer.send(404, "application/json", "{\"error\":\"Node not found\"}");
}

void handleApiPingNode() {
    if (!configServer.hasArg("nodeId")) { configServer.send(400, "application/json", "{\"error\":\"Missing nodeId\"}"); return; }
    String targetNodeId = configServer.arg("nodeId");
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == targetNodeId) {
            espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "PING", "REQUEST", "COMMAND", gateway_id);
            configServer.send(200, "application/json", "{\"status\":\"ping_sent\"}");
            return;
        }
    }
    configServer.send(404, "application/json", "{\"error\":\"Node not found\"}");
}

void handleApiNodeStatus() {
    if (!configServer.hasArg("nodeId")) { configServer.send(400, "application/json", "{\"error\":\"Missing nodeId\"}"); return; }
    String targetNodeId = configServer.arg("nodeId");
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == targetNodeId) {
            String json = "{";
            json += "\"id\":\"" + String(peerList[i].nodeId) + "\",";
            json += "\"online\":" + String(peerList[i].isOnline ? "true" : "false") + ",";
            json += "\"version\":\"" + String(peerList[i].firmwareVersion) + "\"";
            json += "}";
            configServer.send(200, "application/json", json);
            return;
        }
    }
    configServer.send(404, "application/json", "{\"error\":\"Node not found\"}");
}

void handleApiForceHaDiscovery() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!mqttClient.connected()) {
        configServer.send(500, "application/json", "{\"error\":\"MQTT Not Connected\"}");
        return;
    }

    String targetNodeId = "";
    if (configServer.hasArg("nodeId")) {
        targetNodeId = configServer.arg("nodeId");
    }

    int count = 0;
    for (int i = 0; i < peerCount; i++) {
        if (targetNodeId.length() == 0 || String(peerList[i].nodeId) == targetNodeId) {
            // Force full discovery
            HaDiscovery::publishDiscovery(mqttClient, peerList[i], mqtt_topic_prefix);
            HaDiscovery::publishDashboardConfig(mqttClient, peerList[i], mqtt_topic_prefix);
            
            // Force status update to ensure availability is online
            publishPeerStatus(i, "FORCE_DISCOVERY");
            
            count++;
        }
    }

    if (count > 0) {
        configServer.send(200, "application/json", "{\"status\":\"ok\", \"count\":" + String(count) + "}");
    } else {
        configServer.send(404, "application/json", "{\"error\":\"Node not found\"}");
    }
}

void handleNetworkDiscovery() {
    networkDiscoveryActive = true;
    networkDiscoveryStartTime = millis();
    
    // Invia broadcast discovery per nuovi nodi o nodi esistenti
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espNow.send(broadcastMac, "GATEWAY", "DISCOVERY", "DISCOVERY", "REQUEST", "DISCOVERY", gateway_id);
    
    // For known peers, refresh HA Discovery immediately
    if (mqttConnected) {
        int refreshed = 0;
        for (int i = 0; i < peerCount; i++) {
            mqttClient.loop(); // Keep alive
            HaDiscovery::publishDiscovery(mqttClient, peerList[i], mqtt_topic_prefix, true); // Force RESET
            HaDiscovery::publishDashboardConfig(mqttClient, peerList[i], mqtt_topic_prefix);
            refreshed++;
        }
        DevLog.printf("Discovery refresh sent for %d known peers\n", refreshed);
    }
    
    configServer.send(200, "application/json", "{\"message\":\"Discovery started\"}");
}

void handlePingNetwork() {
    pingNetworkActive = true;
    pingNetworkStartTime = millis();
    pingResponseCount = 0;
    memset(pingResponseReceived, 0, sizeof(pingResponseReceived));
    // Copy peers to ping list
    for (int i = 0; i < peerCount; i++) {
        memcpy(pingedNodesMac[i], peerList[i].mac, 6);
        espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "PING", "REQUEST", "COMMAND", gateway_id);
    }
    pingResponseCount = peerCount;
    configServer.send(200, "application/json", "{\"message\":\"Ping network started\"}");
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

void handleDeleteNode() {
    if (configServer.hasArg("mac")) {
        String macStr = configServer.arg("mac");
        removePeer(macStr.c_str());
        configServer.send(200, "application/json", "{\"message\":\"Node removed\"}");
    } else {
        configServer.send(400, "application/json", "{\"error\":\"Missing MAC\"}");
    }
}

void handleApiOtaStatus() {
    StaticJsonDocument<512> doc;
    doc["nodeId"] = globalOtaStatus.nodeId;
    doc["status"] = globalOtaStatus.status;
    doc["timestamp"] = globalOtaStatus.timestamp;
    doc["lastMessage"] = globalOtaStatus.lastMessage;
    doc["progress"] = globalOtaStatus.progress;
    
    String json;
    serializeJson(doc, json);
    configServer.send(200, "application/json", json);
}

void handleTriggerOta() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    configServer.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE");
    configServer.sendHeader("Access-Control-Allow-Headers", "Content-Type, Content-Length, X-Requested-With");
    configServer.sendHeader("Access-Control-Max-Age", "86400");

    if (!configServer.hasArg("nodeId")) {
        configServer.send(400, "text/plain", "Missing nodeId");
        return;
    }
    String nodeId = configServer.arg("nodeId");
    
    // Get Firmware URL from request (provided by Dashboard)
    String url = configServer.hasArg("url") ? configServer.arg("url") : "";
    if (url.length() == 0) {
        configServer.send(400, "text/plain", "Missing firmware URL");
        return;
    }

    // Reset global status
    globalOtaStatus.nodeId = nodeId;
    globalOtaStatus.status = "TRIGGERED";
    globalOtaStatus.timestamp = millis();
    globalOtaStatus.lastMessage = "Invio comando OTA...";
    globalOtaStatus.progress = 0;

    // Find peer
    for (int i = 0; i < peerCount; i++) {
        if (String(peerList[i].nodeId) == nodeId) {
            // Construct payload: SSID|PASS|URL
            String ssid = WiFi.SSID();
            String pass = WiFi.psk();
            
            // Fallback to saved config if WiFi.SSID/PSK is empty
            if (ssid.length() == 0) ssid = String(saved_wifi_ssid);
            if (pass.length() == 0) pass = String(saved_wifi_password);
            
            // Ultimo tentativo: usa la password globale hardcoded se disponibile
            if (pass.length() == 0 && wifi_password != nullptr) {
                 pass = String(wifi_password);
            }

            String payload = ssid + "|" + pass + "|" + url;
            
            // Mask password for debug log
            String maskedPass = (pass.length() > 0) ? (String(pass.charAt(0)) + "****" + String(pass.charAt(pass.length()-1))) : "EMPTY";

            DevLog.printf("[OTA] Triggering OTA for %s\n", nodeId.c_str());
            DevLog.printf("[OTA] Payload: SSID=%s, PASS=%s (Len:%d), URL=%s\n", ssid.c_str(), maskedPass.c_str(), pass.length(), url.c_str());
            DevLog.printf("[OTA] Gateway ID used for command: '%s' (Address: %p)\n", gateway_id, gateway_id);

            // Send OTA_UPDATE command with payload
            espNow.send(peerList[i].mac, peerList[i].nodeId, "CONTROL", "OTA_UPDATE", payload.c_str(), "COMMAND", gateway_id);
            
            configServer.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    configServer.send(404, "text/plain", "Node not found");
}

void handleGatewayUpdate() {
    HTTPUpload& upload = configServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        otaRunning = true; // Stop other tasks
        WiFiUDP::stopAll();
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
            otaRunning = false;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (otaRunning) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
                otaRunning = false;
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (otaRunning) {
            if (Update.end(true)) {
                DevLog.println("Update Success! Finishing...");
            } else {
                Update.printError(Serial);
            }
            otaRunning = false;
        }
    }
    yield(); // Avoid watchdog timeout
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
