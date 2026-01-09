#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebSocketsServer.h>
#include <map>
#include <vector>
#include <algorithm>
#include "index_html.h"
#include "version.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Refactored Includes
#include "Structs.h"
#include "Config.h"
#include "WebLog.h"
#include "DataManager.h"

// --- Variabili Globali Locali ---
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
DNSServer dnsServer;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
File uploadFile;

// Variabili per aggiornamenti GitHub
SystemUpdates systemUpdates;
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/suppaman80/Domoriky_Esp_System/master/versions.json";
const unsigned long UPDATE_CHECK_INTERVAL = 3600000; // Controlla ogni ora (1 ora = 3600000 ms)

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// --- Forward Declaration ---
String generateNetworksList();

// --- Config Server Handlers ---

void handleConfigRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    String html = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configurazione Dashboard</title>
    <style>
        body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #f0f2f5; margin: 0; padding: 20px; color: #333; }
        .container { background: white; max-width: 450px; margin: 0 auto; padding: 30px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.08); }
        h1 { text-align: center; color: #0d6efd; margin-top: 0; margin-bottom: 30px; font-size: 24px; }
        h2 { font-size: 16px; color: #666; border-bottom: 2px solid #eee; padding-bottom: 10px; margin-top: 25px; margin-bottom: 15px; text-transform: uppercase; letter-spacing: 0.5px; }
        label { display: block; margin-bottom: 5px; font-weight: 500; font-size: 14px; }
        input[type="text"], input[type="number"], input[type="password"], select { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 6px; box-sizing: border-box; font-size: 14px; transition: border-color 0.2s; }
        input:focus, select:focus { border-color: #0d6efd; outline: none; }
        .radio-group { display: flex; gap: 20px; margin-bottom: 15px; }
        .radio-group label { font-weight: normal; cursor: pointer; display: flex; align-items: center; gap: 5px; }
        button { width: 100%; padding: 12px; border: none; border-radius: 6px; font-size: 16px; font-weight: 600; cursor: pointer; transition: background 0.2s; margin-top: 10px; }
        .btn-save { background-color: #0d6efd; color: white; }
        .btn-save:hover { background-color: #0b5ed7; }
        .btn-reboot { background-color: #ffc107; color: #000; margin-top: 15px; }
        .btn-reboot:hover { background-color: #ffca2c; }
        .hidden { display: none; }
        .note { font-size: 12px; color: #888; margin-top: -10px; margin-bottom: 15px; }
    </style>
    <script>
        function toggleIP() {
            const isStatic = document.querySelector('input[name="network_mode"]:checked').value === 'static';
            const staticFields = document.getElementById('static-fields');
            staticFields.style.display = isStatic ? 'block' : 'none';
        }
        
        function loadNetworks() {
            var sel = document.getElementsByName('wifi_ssid')[0];
            fetch('/scan')
                .then(function(response) { return response.text(); })
                .then(function(html) {
                    if(html.trim() !== "") {
                        sel.innerHTML = html;
                    } else {
                        var current = sel.value;
                        sel.innerHTML = "<option value='" + current + "' selected>" + current + " (Salvata - Nessuna rete trovata)</option>";
                    }
                })
                .catch(function(err) {
                    console.error('Scan error', err);
                });
        }
        
        window.onload = function() {
            loadNetworks();
        };
    </script>
</head>
<body>
    <div class="container">
        <h1>Configurazione Dashboard</h1>
        
        <button onclick="location.href='/'" style="background-color: #6c757d; color: white; margin-bottom: 20px;">⬅ Torna alla Dashboard</button>

        <form action="/save" method="post">
            
            <h2>Connessione WiFi</h2>
            <label>Rete WiFi (SSID)</label>
            <select name="wifi_ssid">
                <option value=")rawliteral";
            
    server.sendContent(html);
    server.sendContent(String(wifi_ssid));
    server.sendContent(F("\" selected>"));
    server.sendContent(String(wifi_ssid));
    if (strlen(wifi_ssid) > 0) server.sendContent(F(" (Salvata - Scansione in corso...)"));
    else server.sendContent(F("Scansione in corso..."));
    server.sendContent(F("</option>"));
    
    html = R"rawliteral(
            </select>
            <label>Password WiFi</label>
            <input type="password" name="wifi_password" placeholder="Inserisci password..." value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(wifi_password));
    
    html = R"rawliteral(">

            <h2>MQTT Broker</h2>
            <label>Indirizzo Server</label>
            <input type="text" name="mqtt_server" placeholder="es. 192.168.1.100" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(mqtt_server));
    
    html = R"rawliteral(">
            <div style="display: flex; gap: 10px;">
                <div style="flex: 1;">
                    <label>Porta</label>
                    <input type="number" name="mqtt_port" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(mqtt_port));

    html = R"rawliteral(">
                </div>
                <div style="flex: 2;">
                    <label>User (Opzionale)</label>
                    <input type="text" name="mqtt_user" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(mqtt_user));

    html = R"rawliteral(">
                </div>
            </div>
            <label>Password (Opzionale)</label>
            <input type="password" name="mqtt_password" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(mqtt_password));
    
    html = R"rawliteral(">
            <label>Prefisso Topic</label>
            <input type="text" name="mqtt_topic_prefix" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(mqtt_topic_prefix));

    html = R"rawliteral(">

            <h2>🕒 Configurazione Orario (NTP)</h2>
            <div class="form-group">
                <label for="ntp_server">Server NTP</label>
                <input type="text" id="ntp_server" name="ntp_server" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(ntp_server));
    server.sendContent(F("\"></div>"));

    server.sendContent(F("<div class=\"form-group\"><label for=\"ntp_timezone\">Fuso Orario (Ore)</label><input type=\"number\" step=\"0.5\" id=\"ntp_timezone\" name=\"ntp_timezone\" value=\""));
    server.sendContent(String(ntp_timezone));
    server.sendContent(F("\"> <small>(es. 1 per GMT+1)</small></div>"));

    html = R"rawliteral(
            <div class="radio-group" style="margin-top: 10px;">
                <label><input type="checkbox" name="ntp_dst" value="1" )rawliteral";
    server.sendContent(html);
    if (ntp_dst) server.sendContent("checked");
    server.sendContent(F("> Ora Legale (DST)</label></div>"));

    html = R"rawliteral(

            <h2>Impostazioni Dispositivo</h2>
            <label>ID Dashboard</label>
            <input type="text" name="gateway_id" value=")rawliteral";
    server.sendContent(html);
    server.sendContent(String(gateway_id));

    html = R"rawliteral(">

            <h2>Configurazione IP</h2>
            <div class="radio-group">
                <label><input type="radio" name="network_mode" value="dhcp" onclick="toggleIP()" )rawliteral";
    server.sendContent(html);
    if (strcmp(network_mode, "dhcp") == 0) server.sendContent("checked");
    
    html = R"rawliteral(> Automatico (DHCP)</label>
                <label><input type="radio" name="network_mode" value="static" onclick="toggleIP()" )rawliteral";
    server.sendContent(html);
    if (strcmp(network_mode, "static") == 0) server.sendContent("checked");

    html = R"rawliteral(> Statico</label>
            </div>)rawliteral";
    server.sendContent(html);

    // --- Static IP Fields (Generated dynamically to avoid HTML issues) ---
    String staticHtml = F("<div id=\"static-fields\" style=\"display: ");
    staticHtml += (strcmp(network_mode, "static") == 0 ? "block" : "none");
    staticHtml += F(";\">");

    staticHtml += F("<label>Indirizzo IP</label><input type=\"text\" name=\"static_ip\" placeholder=\"es. 192.168.1.50\" value=\"");
    staticHtml += String(static_ip);
    staticHtml += F("\"><br>");

    staticHtml += F("<label>Gateway</label><input type=\"text\" name=\"static_gateway\" placeholder=\"es. 192.168.1.1\" value=\"");
    staticHtml += String(static_gateway);
    staticHtml += F("\"><br>");

    staticHtml += F("<label>Subnet Mask</label><input type=\"text\" name=\"static_subnet\" placeholder=\"es. 255.255.255.0\" value=\"");
    staticHtml += String(static_subnet);
    staticHtml += F("\"><br>");

    staticHtml += F("<label>DNS Server</label><input type=\"text\" name=\"static_dns\" placeholder=\"es. 8.8.8.8\" value=\"");
    staticHtml += String(static_dns);
    staticHtml += F("\"><br></div>");
    
    staticHtml += F("<button type=\"submit\" class=\"btn-save\">💾 Salva Configurazione</button></form>");
    server.sendContent(staticHtml);

    html = R"rawliteral(
        
        <form action="/reboot" method="post" onsubmit="return confirm('Sei sicuro di voler riavviare il dispositivo?');">
            <button type="submit" class="btn-reboot">🔄 Riavvia Dispositivo</button>
        </form>
        
        <form action="/reset_ap" method="post" onsubmit="return confirm('ATTENZIONE: Questo cancellerà le impostazioni WiFi e riavvierà il dispositivo in modalità Access Point. Continuare?');">
            <button type="submit" class="btn-reboot" style="background-color: #dc3545; color: white; margin-top: 10px;">⚠️ Reset to AP Mode</button>
        </form>
    </div>
</body>
</html>)rawliteral";
    server.sendContent(html);
    server.sendContent(""); // Terminate chunked response
}

void handleConfigSave() {
    if (server.hasArg("mqtt_server")) {
        if (!LittleFS.begin()) LittleFS.begin(true);
        DynamicJsonDocument doc(4096);
        
        // Aggiorna variabili globali e JSON
        doc["wifi_ssid"] = server.arg("wifi_ssid");
        doc["wifi_password"] = server.arg("wifi_password");
        doc["mqtt_server"] = server.arg("mqtt_server");
        doc["mqtt_port"] = server.arg("mqtt_port").toInt();
        doc["mqtt_user"] = server.arg("mqtt_user");
        doc["mqtt_password"] = server.arg("mqtt_password");
        doc["mqtt_topic_prefix"] = server.arg("mqtt_topic_prefix");
        doc["gateway_id"] = server.arg("gateway_id");
        doc["network_mode"] = server.arg("network_mode");
        doc["static_ip"] = server.arg("static_ip");
        doc["static_gateway"] = server.arg("static_gateway");
        doc["static_subnet"] = server.arg("static_subnet");
        doc["static_dns"] = server.arg("static_dns");
        
        doc["ntp_server"] = server.arg("ntp_server");
        doc["ntp_timezone"] = server.arg("ntp_timezone").toFloat();
        doc["ntp_dst"] = server.hasArg("ntp_dst");

        // Aggiorna array char locali
        strlcpy(wifi_ssid, server.arg("wifi_ssid").c_str(), sizeof(wifi_ssid));
        strlcpy(wifi_password, server.arg("wifi_password").c_str(), sizeof(wifi_password));
        strlcpy(saved_wifi_ssid, wifi_ssid, sizeof(saved_wifi_ssid));
        strlcpy(saved_wifi_password, wifi_password, sizeof(saved_wifi_password));
        
        strlcpy(mqtt_server, server.arg("mqtt_server").c_str(), sizeof(mqtt_server));
        mqtt_port = server.arg("mqtt_port").toInt();
        strlcpy(mqtt_user, server.arg("mqtt_user").c_str(), sizeof(mqtt_user));
        strlcpy(mqtt_password, server.arg("mqtt_password").c_str(), sizeof(mqtt_password));
        strlcpy(mqtt_topic_prefix, server.arg("mqtt_topic_prefix").c_str(), sizeof(mqtt_topic_prefix));
        strlcpy(gateway_id, server.arg("gateway_id").c_str(), sizeof(gateway_id));
        
        strlcpy(network_mode, server.arg("network_mode").c_str(), sizeof(network_mode));
        strlcpy(static_ip, server.arg("static_ip").c_str(), sizeof(static_ip));
        strlcpy(static_gateway, server.arg("static_gateway").c_str(), sizeof(static_gateway));
        strlcpy(static_subnet, server.arg("static_subnet").c_str(), sizeof(static_subnet));
        strlcpy(static_dns, server.arg("static_dns").c_str(), sizeof(static_dns));
        
        strlcpy(ntp_server, server.arg("ntp_server").c_str(), sizeof(ntp_server));
        ntp_timezone = server.arg("ntp_timezone").toFloat();
        ntp_dst = server.hasArg("ntp_dst");
        
        gmt_offset_sec = (long)(ntp_timezone * 3600);
        daylight_offset_sec = ntp_dst ? 3600 : 0;
        
        File configFile = LittleFS.open("/config.json", "w");
        if (configFile) {
            if (serializeJson(doc, configFile) == 0) {
                 DevLog.println("Errore: 0 bytes scritti in config.json");
            }
            configFile.close();
            wifi_credentials_loaded = true;
            
            DevLog.println("Configurazione salvata. Riavvio...");
            
            server.send(200, "text/html", "Configurazione salvata. Riavvio...");
            server.client().flush(); // Ensure response is sent
            delay(100);
            
            apTransitionPending = true;
            apTransitionStart = millis();
        } else {
            server.send(500, "text/plain", "Errore scrittura file");
        }
    } else {
        server.send(400, "text/plain", "Dati mancanti");
    }
}

void handleResetToAP() {
    DevLog.println("[CMD] Richiesto Reset to AP Mode...");
    
    if (LittleFS.begin(true)) {
        DynamicJsonDocument doc(4096);
        
        // Carica configurazione esistente
        if (LittleFS.exists("/config.json")) {
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                deserializeJson(doc, configFile);
                configFile.close();
            }
        }
        
        // Resetta solo WiFi
        doc["wifi_ssid"] = "";
        doc["wifi_password"] = "";
        
        File configFile = LittleFS.open("/config.json", "w");
        if (configFile) {
            serializeJson(doc, configFile);
            configFile.close();
            DevLog.println("[CMD] Credenziali WiFi rimosse. Riavvio in corso...");
            server.send(200, "text/html", "<h1>Reset completato</h1><p>Il dispositivo si sta riavviando in modalita' Access Point (AP).</p><p>Connettiti alla rete WiFi 'ESP32-Dashboard-Config' per riconfigurarlo.</p>");
            delay(1000);
            ESP.restart();
        } else {
            server.send(500, "text/plain", "Errore salvataggio config");
        }
    } else {
        server.send(500, "text/plain", "Errore filesystem");
    }
}

void handleReboot() {
    server.send(200, "text/html", "Riavvio...");
    delay(1000);
    ESP.restart();
}

void startConfigAP() {
    configMode = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("ESP32-Dashboard-Config", "");
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    
    dnsServer.start(53, "*", apIP);
    
    server.on("/", HTTP_GET, handleConfigRoot);
    server.on("/scan", HTTP_GET, []() {
        String networks = generateNetworksList();
        server.send(200, "text/plain", networks);
    });
    server.on("/save", HTTP_POST, handleConfigSave);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/reset_ap", HTTP_POST, handleResetToAP);
    server.onNotFound(handleConfigRoot);
    
    server.begin();
    DevLog.println("AP Mode avviato: 192.168.4.1");
}

void stopConfigAP() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    configMode = false;
}

String generateNetworksList() {
    String networks = "";
    int n = WiFi.scanNetworks();
    if (n == 0) {
        networks = "<option value=''>Nessuna rete trovata</option>";
    } else {
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            String encryption;
            switch (WiFi.encryptionType(i)) {
                case WIFI_AUTH_OPEN: encryption = " (Aperto)"; break;
                default: encryption = " (Protetto)"; break;
            }
            ssid.replace("\"", "&quot;");
            networks += "<option value=\"" + ssid + "\"";
            if (strcmp(wifi_ssid, ssid.c_str()) == 0) { networks += " selected"; }
            networks += ">" + ssid + encryption + "</option>";
        }
    }
    return networks;
}

// --- MQTT Callback ---
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    DevLog.printf("[MQTT] Rcv Topic: %s\n", topic);
    DevLog.print("[MQTT] Payload: ");
    DevLog.println(message);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        DevLog.print("[MQTT] Errore JSON su topic ");
        DevLog.print(topicStr);
        DevLog.println(": " + String(error.c_str()));
        DevLog.println("[MQTT] Raw Payload: " + message);
        return;
    }

    // 1. Gateway Status / Heartbeat
    if (topicStr.endsWith("/gateway/status")) {
        int statusIndex = topicStr.lastIndexOf("/gateway/status");
        if (statusIndex > 0) {
            String foundPrefix = topicStr.substring(0, statusIndex);
            bool exists = false;
            for (const auto& p : knownPrefixes) {
                if (p == foundPrefix) { exists = true; break; }
            }
            if (!exists) {
                knownPrefixes.push_back(foundPrefix);
                DevLog.printf("[MQTT] Nuovo prefisso rilevato: %s\n", foundPrefix.c_str());
                saveDiscoveredPrefixes();
            }
            
            String gwId = doc["gatewayId"].as<String>();
            if (gwId.length() > 0 && gwId != "null" && gwId != "NULL") {
                gateways[gwId].mqttPrefix = foundPrefix;
            }
        }

        String gwId = doc["gatewayId"].as<String>();
        if (gwId.length() > 0 && gwId != "null" && gwId != "NULL") {
            GatewayInfo& gw = gateways[gwId];
            gw.id = gwId;
            gw.lastSeen = millis();
            
            if (doc.containsKey("IP")) gw.ip = doc["IP"].as<String>();
            else if (doc.containsKey("ip")) gw.ip = doc["ip"].as<String>();
            
            if (doc.containsKey("uptime")) gw.uptime = doc["uptime"].as<unsigned long>();
            
             gw.mqttStatus = "N/A";

             JsonVariant mqttVar = doc["mqtt"];
             if (mqttVar.isNull()) mqttVar = doc["MQTT"];
             if (mqttVar.isNull()) mqttVar = doc["Mqtt"];

             if (!mqttVar.isNull()) {
                 if (mqttVar.is<JsonObject>()) {
                      JsonObject mqttObj = mqttVar.as<JsonObject>();
                      if (mqttObj.containsKey("status")) {
                          gw.mqttStatus = mqttObj["status"].as<String>();
                      } else if (mqttObj.containsKey("Status")) {
                          gw.mqttStatus = mqttObj["Status"].as<String>();
                      } else if (mqttObj.containsKey("state")) {
                          gw.mqttStatus = mqttObj["state"].as<String>();
                      } else if (mqttObj.containsKey("connected")) {
                          JsonVariant connVar = mqttObj["connected"];
                          if (connVar.is<bool>()) {
                              gw.mqttStatus = connVar.as<bool>() ? "Connected" : "Disconnected";
                          } else if (connVar.is<String>()) {
                              String s = connVar.as<String>();
                              if (s.equalsIgnoreCase("true") || s.equalsIgnoreCase("1")) gw.mqttStatus = "Connected";
                              else gw.mqttStatus = "Disconnected";
                          } else {
                              gw.mqttStatus = connVar.as<bool>() ? "Connected" : "Disconnected";
                          }
                      } else {
                          gw.mqttStatus = "Obj Found (No Status)";
                      }
                 } else if (mqttVar.is<String>()) {
                      gw.mqttStatus = mqttVar.as<String>();
                 } else if (mqttVar.is<bool>()) {
                      gw.mqttStatus = mqttVar.as<bool>() ? "Connected" : "Disconnected";
                 }
             } else if (doc.containsKey("mqtt_status")) {
                 gw.mqttStatus = doc["mqtt_status"].as<String>();
             } else if (doc.containsKey("MQTT_Status")) {
                 gw.mqttStatus = doc["MQTT_Status"].as<String>();
             }

             if (doc.containsKey("eventType")) {
                 String evt = doc["eventType"].as<String>();
                 String msg = doc.containsKey("message") ? doc["message"].as<String>() : "";
                 
                 if (evt.startsWith("ota_") || evt == "error" || evt == "peer_removed") {
                    DevLog.printf("[GW-EVENT] %s: %s - %s\n", gwId.c_str(), evt.c_str(), msg.c_str());
                }
                
                if (evt == "ota_start") gw.mqttStatus = "OTA Update...";
                else if (evt == "ota_success") gw.mqttStatus = "OTA Success!";
                else if (evt == "ota_failed") gw.mqttStatus = "OTA Failed";
                else if (evt == "error") gw.mqttStatus = "Error: " + msg;
                else if (evt == "peer_removed") {
                     gw.mqttStatus = "Peer Removed";
                     // Gestione Rimozione Peer
                     if (msg.startsWith("Peer removed: ")) {
                         String nodeIdToRemove = msg.substring(14);
                         nodeIdToRemove.trim();
                         if (nodeIdToRemove.length() > 0) {
                             bool removed = false;
                             // Try direct erase (if key is NodeId)
                             if (peers.erase(nodeIdToRemove)) {
                                 removed = true;
                             } else {
                                 // Try linear search (if key is MAC)
                                 for (auto it = peers.begin(); it != peers.end(); ) {
                                     if (it->second.nodeId == nodeIdToRemove) {
                                         it = peers.erase(it);
                                         removed = true;
                                         break; // Assume unique NodeId
                                     } else {
                                         ++it;
                                     }
                                 }
                             }

                             if (removed) {
                                 DevLog.printf("[AUTO-UPDATE] Nodo rimosso: %s\n", nodeIdToRemove.c_str());
                                 saveNetworkState();
                                 broadcastUpdate(); // Aggiorna Frontend
                             } else {
                                 DevLog.printf("[AUTO-UPDATE] Nodo %s non trovato in lista locale\n", nodeIdToRemove.c_str());
                             }
                         }
                     }
                }
             }

             if (gw.mqttStatus == "N/A") {
                 if (doc.containsKey("status")) {
                     String generalStatus = doc["status"].as<String>();
                     if (generalStatus == "ALIVE" || generalStatus == "ONLINE") {
                         gw.mqttStatus = "Connected"; 
                     } else {
                         gw.mqttStatus = generalStatus;
                     }
                 } else {
                     gw.mqttStatus = "Connected";
                 }
             }
            
            if (doc.containsKey("MAC")) gw.mac = doc["MAC"].as<String>();
            else if (doc.containsKey("mac")) gw.mac = doc["mac"].as<String>();

            if (doc.containsKey("version")) gw.version = doc["version"].as<String>();
            else if (doc.containsKey("Version")) gw.version = doc["Version"].as<String>();
            else if (doc.containsKey("firmware")) gw.version = doc["firmware"].as<String>();
            else if (doc.containsKey("fw_version")) gw.version = doc["fw_version"].as<String>();
            
            if (doc.containsKey("buildDate")) gw.buildDate = doc["buildDate"].as<String>();
            else if (doc.containsKey("Build")) gw.buildDate = doc["Build"].as<String>();
            else if (doc.containsKey("build")) gw.buildDate = doc["build"].as<String>();
            
            saveNetworkState();
        }
    }
    // 2. Node Status / Peer Status
    else if (topicStr.endsWith("/nodo/status")) {
        String nodeId, nodeType, status, gatewayId, mac, firmwareVersion;
        
        if (doc.containsKey("nodeId")) nodeId = doc["nodeId"].as<String>();
        else if (doc.containsKey("Node")) nodeId = doc["Node"].as<String>();
        
        if (doc.containsKey("nodeType")) nodeType = doc["nodeType"].as<String>();
        else if (doc.containsKey("Type")) nodeType = doc["Type"].as<String>();
        
        if (doc.containsKey("status")) status = doc["status"].as<String>();
        else if (doc.containsKey("Status")) status = doc["Status"].as<String>();
        
        if (doc.containsKey("gatewayId")) {
            gatewayId = doc["gatewayId"].as<String>();
        } else {
            String baseTopic = topicStr.substring(0, topicStr.length() - 12);
            int lastSlash = baseTopic.lastIndexOf('/');
            if (lastSlash != -1) {
                gatewayId = baseTopic.substring(lastSlash + 1);
            }
        }
        
        if (doc.containsKey("mac")) mac = doc["mac"].as<String>();
        else if (doc.containsKey("MAC")) mac = doc["MAC"].as<String>();
        if (doc.containsKey("firmwareVersion")) firmwareVersion = doc["firmwareVersion"].as<String>();

        if (nodeId.length() > 0) {
            String key = nodeId;
            if (mac.length() > 0) {
                key = mac;
            } else {
                // Anti-Ghost: Try to find existing peer by nodeId if MAC is missing
                for (auto& kv : peers) {
                    if (kv.second.nodeId == nodeId) {
                        key = kv.first;
                        break;
                    }
                }
            }

            PeerInfo& peer = peers[key];
            peer.nodeId = nodeId;
            // Update nodeType if present and valid (Filter out non-structural types)
            if (nodeType.length() > 0 && nodeType != "null" && nodeType != "UNKNOWN" && 
                nodeType != "COMMAND" && nodeType != "RESPONSE" && nodeType != "ACK" && nodeType != "FEEDBACK" &&
                nodeType != "REGISTRATION" && nodeType != "HEARTBEAT" && nodeType != "DISCOVERY") {
                peer.nodeType = nodeType;
            } else if (peer.nodeType.length() == 0 && nodeType == "UNKNOWN") {
                // If we have nothing, accept UNKNOWN but it's not ideal
                peer.nodeType = nodeType;
            }
            if (gatewayId.length() > 0) peer.gatewayId = gatewayId;
            if (status.length() > 0) peer.status = status;
            if (mac.length() > 0) peer.mac = mac;
            if (firmwareVersion.length() > 0) peer.firmwareVersion = firmwareVersion;
            
            if (doc.containsKey("attributes")) {
                peer.attributes = doc["attributes"].as<String>();
            }
            
            peer.lastSeen = millis();
            
            saveNetworkState();
        }
    }
    // 4. Dashboard Discovery Request
    else if (topicStr.endsWith("/dashboard/discovery")) {
        DevLog.println("[MQTT] Ricevuta richiesta discovery dashboard");
        
        // Publish Dashboard Status (Retained)
        String statusTopic = String(mqtt_topic_prefix) + "/dashboard/status";
        String statusPayload = "{\"id\":\"" + String(gateway_id) + "\", \"ip\":\"" + WiFi.localIP().toString() + "\", \"status\":\"online\"}";
        mqttClient.publish(statusTopic.c_str(), statusPayload.c_str(), true); // true = retained
        DevLog.printf("[MQTT] Dashboard Announce sent to %s\n", statusTopic.c_str());
        return;
    }
    // 3. Gestione Report e Liste
    else if (topicStr.endsWith("/gateway/report") || topicStr.endsWith("/gateway/response")) {
        if (doc.containsKey("peers") && doc["peers"].is<JsonArray>()) {
             JsonArray peersArr = doc["peers"];
             for (JsonObject p : peersArr) {
                 String mac = p["mac"].as<String>();
                 String nodeId = p["nodeId"].as<String>();
                 if (mac.length() > 0 || nodeId.length() > 0) {
                     String key = nodeId;
                     if (mac.length() > 0) {
                         key = mac;
                     } else {
                         // Anti-Ghost: Try to find existing peer by nodeId
                         for (auto& kv : peers) {
                             if (kv.second.nodeId == nodeId) {
                                 key = kv.first;
                                 break;
                             }
                         }
                     }
                     PeerInfo& peer = peers[key];
                     peer.nodeId = nodeId;
                     if (p.containsKey("type")) peer.nodeType = p["type"].as<String>();
                     if (p.containsKey("nodeType")) peer.nodeType = p["nodeType"].as<String>();
                     if (p.containsKey("firmwareVersion")) peer.firmwareVersion = p["firmwareVersion"].as<String>();
                     if (p.containsKey("attributes")) peer.attributes = p["attributes"].as<String>();
                     
                     String newGwId = "";
                     if (doc.containsKey("gatewayId")) newGwId = doc["gatewayId"].as<String>();
                     else if (p.containsKey("gatewayId")) newGwId = p["gatewayId"].as<String>();
                     
                     if (newGwId.length() > 0 && newGwId != "null") peer.gatewayId = newGwId;
                     
                     peer.lastSeen = millis();
                 }
             }
             saveNetworkState();
        }
        
        if (doc.containsKey("ping_results") && doc["ping_results"].is<JsonArray>()) {
            JsonArray results = doc["ping_results"];
            for (JsonObject res : results) {
                String mac = res["mac"].as<String>();
                bool online = res["success"].as<bool>() || (res["status"] == "online");
                
                if (mac.length() > 0 && peers.count(mac)) {
                    PeerInfo& peer = peers[mac];
                    peer.status = online ? "online" : "offline";
                    if (online) peer.lastSeen = millis();
                }
            }
            saveNetworkState();
        }
    }
}

// --- MQTT Connection ---
void connectMQTT() {
    if (mqttClient.connected()) return;

    String clientId = "ESP32_Dashboard_" + String(random(0xffff), HEX);
    DevLog.print("Connessione MQTT...");
    
    bool connected = false;
    
    // Configurazione LWT (Last Will and Testament)
    String lwtTopic = String(mqtt_topic_prefix) + "/dashboard/status";
    String lwtPayload = "{\"id\":\"" + String(gateway_id) + "\", \"status\":\"offline\"}";
    
    if (strlen(mqtt_user) > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password, lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    } else {
        connected = mqttClient.connect(clientId.c_str(), lwtTopic.c_str(), 1, true, lwtPayload.c_str());
    }

    if (connected) {
        DevLog.println("Connesso!");
        String subGateway = "+/gateway/status"; 
        String subNode = "+/nodo/status";       
        String subReport = "+/gateway/report";
        String subResponse = "+/gateway/response";
        String subDiscovery = "+/dashboard/discovery";
        
        mqttClient.subscribe(subGateway.c_str());
        mqttClient.subscribe(subNode.c_str());
        mqttClient.subscribe(subReport.c_str());
        mqttClient.subscribe(subResponse.c_str());
        mqttClient.subscribe(subDiscovery.c_str());
        
        delay(100);

        String cmdTopic = String(mqtt_topic_prefix) + "/gateway/command";
        String payload = "{\"command\":\"LIST_PEERS\"}";
        mqttClient.publish(cmdTopic.c_str(), payload.c_str());

        // Publish Dashboard Status (Retained) for Auto-Discovery
        String statusTopic = String(mqtt_topic_prefix) + "/dashboard/status";
        String statusPayload = "{\"id\":\"" + String(gateway_id) + "\", \"ip\":\"" + WiFi.localIP().toString() + "\", \"status\":\"online\"}";
        mqttClient.publish(statusTopic.c_str(), statusPayload.c_str(), true); // true = retained
        DevLog.printf("[MQTT] Dashboard Announce sent to %s\n", statusTopic.c_str());
    } else {
        DevLog.print("Fallito, rc=");
        DevLog.println(mqttClient.state());
    }
}

// --- Web Server Handlers ---
void handleRoot() {
    server.send(200, "text/html", index_html);
}

String getFormattedTime() {
    struct tm timeinfo;
    // Reduce timeout to 10ms to prevent blocking the loop if NTP is down
    if(!getLocalTime(&timeinfo, 10)){
        return "N/A";
    }
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S %d/%m/%Y", &timeinfo);
    return String(timeStringBuff);
}

void populateJson(DynamicJsonDocument& doc) {
    // Info Dashboard
    JsonObject dashboardObj = doc.createNestedObject("dashboard");
    dashboardObj["id"] = gateway_id;
    dashboardObj["version"] = FIRMWARE_VERSION;
    dashboardObj["buildDate"] = String(BUILD_DATE) + " " + String(BUILD_TIME);
    dashboardObj["uptime"] = millis();
    dashboardObj["mqttPrefix"] = mqtt_topic_prefix;
    dashboardObj["wifiSignal"] = WiFi.RSSI();
    dashboardObj["ip"] = WiFi.localIP().toString();
    dashboardObj["mac"] = WiFi.macAddress();
    dashboardObj["time"] = getFormattedTime();
    
    // System Stats
    dashboardObj["freeHeap"] = ESP.getFreeHeap();
    dashboardObj["maxAllocHeap"] = ESP.getMaxAllocHeap();
    dashboardObj["heapSize"] = ESP.getHeapSize();
    dashboardObj["cpuFreq"] = ESP.getCpuFreqMHz();
    
    // Filesystem Stats
    if (LittleFS.begin()) { 
         dashboardObj["fsUsed"] = LittleFS.usedBytes();
         dashboardObj["fsTotal"] = LittleFS.totalBytes();
    } else {
         dashboardObj["fsUsed"] = 0;
         dashboardObj["fsTotal"] = 0;
    }

    JsonObject gatewaysObj = doc.createNestedObject("gateways");
    for (auto const& [id, gw] : gateways) {
        JsonObject g = gatewaysObj.createNestedObject(id);
        g["id"] = gw.id;
        g["ip"] = gw.ip;
        g["mqttStatus"] = gw.mqttStatus;
        g["uptime"] = gw.uptime;
        g["lastSeen"] = gw.lastSeen;
        g["mac"] = gw.mac;
        g["version"] = gw.version;
        g["buildDate"] = gw.buildDate;
        g["mqttPrefix"] = gw.mqttPrefix;
    }
    
    JsonObject peersObj = doc.createNestedObject("peers");
    for (auto const& [id, peer] : peers) {
        JsonObject p = peersObj.createNestedObject(id);
        p["nodeId"] = peer.nodeId;
        p["nodeType"] = peer.nodeType;
        p["gatewayId"] = peer.gatewayId;
        p["status"] = peer.status;
        p["mac"] = peer.mac;
        p["firmwareVersion"] = peer.firmwareVersion;
        p["attributes"] = peer.attributes;
        p["lastSeen"] = peer.lastSeen;
    }
    
    // System Updates
    JsonObject updatesObj = doc.createNestedObject("updates");
    
    if (systemUpdates.dashboard.available) {
        JsonObject dashUpd = updatesObj.createNestedObject("dashboard");
        dashUpd["available"] = true;
        dashUpd["version"] = systemUpdates.dashboard.version;
        dashUpd["url"] = systemUpdates.dashboard.url;
        dashUpd["notes"] = systemUpdates.dashboard.notes;
        DevLog.printf("[JSON] Dash Update added: v%s\n", systemUpdates.dashboard.version.c_str());
    }

    if (systemUpdates.gateway.available) {
        JsonObject gwUpd = updatesObj.createNestedObject("gateway");
        gwUpd["version"] = systemUpdates.gateway.version;
        gwUpd["url"] = systemUpdates.gateway.url;
        gwUpd["notes"] = systemUpdates.gateway.notes;
        DevLog.printf("[JSON] Gateway Update added: v%s\n", systemUpdates.gateway.version.c_str());
    }
    
    JsonObject nodesUpdObj = updatesObj.createNestedObject("nodes");
    for (auto const& [type, info] : systemUpdates.nodes) {
        if (info.available) {
            JsonObject n = nodesUpdObj.createNestedObject(type);
            n["version"] = info.version;
            n["url"] = info.url;
            n["notes"] = info.notes;
            DevLog.printf("[JSON] Node Update added for %s: v%s\n", type.c_str(), info.version.c_str());
        }
    }
    
    updatesObj["lastCheck"] = systemUpdates.lastCheck;
    updatesObj["lastResult"] = systemUpdates.lastResult;
}

void handleApiData() {
    DynamicJsonDocument doc(4096);
    populateJson(doc);
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleApiCommand() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"Body missing\"}");
        return;
    }
    
    DynamicJsonDocument doc(512);
    deserializeJson(doc, server.arg("plain"));
    
    // 0. GESTIONE COMANDI AMMINISTRATIVI (Gateway-Level)
    if (doc.containsKey("command")) {
        String cmdVal = doc["command"];
        if (cmdVal == "REMOVE_PEER" || cmdVal == "NODE_FACTORY_RESET") {
             String nodeId = doc.containsKey("nodeId") ? doc["nodeId"].as<String>() : "";
             String gwId = "";
             
             // Cerca GatewayId se non fornito
             if (doc.containsKey("gatewayId")) {
                 gwId = doc["gatewayId"].as<String>();
             } else if (nodeId.length() > 0 && peers.count(nodeId)) {
                 gwId = peers[nodeId].gatewayId;
             }

             // Trova prefisso
             String targetPrefix = String(mqtt_topic_prefix);
             if (gwId.length() > 0 && gateways.count(gwId) && gateways[gwId].mqttPrefix.length() > 0) {
                 targetPrefix = gateways[gwId].mqttPrefix;
             }

             DynamicJsonDocument cmdDoc(512);
             cmdDoc["command"] = cmdVal;
             
             if (cmdVal == "REMOVE_PEER") {
                 if (doc.containsKey("mac")) cmdDoc["mac"] = doc["mac"];
                 cmdDoc["gatewayId"] = gwId; 
             } else {
                 cmdDoc["nodeId"] = nodeId; 
             }

             String payload;
             serializeJson(cmdDoc, payload);
             
             String topic = targetPrefix + "/gateway/command";
             mqttClient.publish(topic.c_str(), payload.c_str());
             
             DevLog.printf("[ADM-CMD] Inviato %s per %s su %s\n", cmdVal.c_str(), nodeId.c_str(), topic.c_str());
             server.send(200, "application/json", "{\"status\":\"sent\", \"type\":\"ADMIN\"}");
             return;
        }
    }

    // 1. PRIORITÀ AL COMANDO NODO (Specifico)
    if (doc.containsKey("nodeId") && doc.containsKey("topic") && doc.containsKey("command")) {
        String nodeId = doc["nodeId"];
        String topicSuffix = doc["topic"]; 
        String cmdVal = doc["command"];    
        
        String targetPrefix = String(mqtt_topic_prefix); 
        bool prefixFound = false;
        
        if (peers.count(nodeId)) {
            String gwId = peers[nodeId].gatewayId;
            if (gwId.length() > 0 && gateways.count(gwId)) {
                if (gateways[gwId].mqttPrefix.length() > 0) {
                    targetPrefix = gateways[gwId].mqttPrefix;
                    prefixFound = true;
                }
            }
        }
        
        if (!prefixFound) {
            for (auto const& [key, peer] : peers) {
                if (peer.nodeId == nodeId) {
                    String gwId = peer.gatewayId;
                    if (gwId.length() > 0 && gateways.count(gwId)) {
                        if (gateways[gwId].mqttPrefix.length() > 0) {
                            targetPrefix = gateways[gwId].mqttPrefix;
                            prefixFound = true;
                        } else {
                            DevLog.printf("[CMD] Warn: Gateway %s ha prefisso vuoto\n", gwId.c_str());
                        }
                    } else {
                        DevLog.printf("[CMD] Warn: Gateway %s non trovato per nodo %s\n", gwId.c_str(), nodeId.c_str());
                    }
                    break;
                }
            }
        }
        
        DevLog.printf("[CMD] NodeId: %s -> Target Prefix: %s (Found: %s)\n", nodeId.c_str(), targetPrefix.c_str(), prefixFound ? "Yes" : "No");

        DynamicJsonDocument cmdDoc(512);
        cmdDoc["Node"] = nodeId;
        cmdDoc["Topic"] = topicSuffix;
        cmdDoc["Command"] = cmdVal;
        cmdDoc["Type"] = "COMMAND";
        cmdDoc["Status"] = "REQUEST";
        
        String payload;
        serializeJson(cmdDoc, payload);
        
        String topic = targetPrefix + "/nodo/command";
        mqttClient.publish(topic.c_str(), payload.c_str());
        
        DevLog.printf("[CMD] Inviato comando nodo su topic: %s\n", topic.c_str());
        
        server.send(200, "application/json", "{\"status\":\"sent\", \"payload\":" + payload + ", \"prefix\":\"" + targetPrefix + "\"}");
        return;
    }

    // 2. COMANDI SISTEMA / GATEWAY (Generici)
    if (doc.containsKey("command")) {
        String cmd = doc["command"].as<String>();
        
        if (knownPrefixes.empty()) {
             knownPrefixes.push_back(String(mqtt_topic_prefix));
        }

        if (cmd == "RESET_NETWORK") {
            DevLog.println("[CMD] Reset rete richiesto (RESET_NETWORK)");
            
            // 1. Clear Data
            gateways.clear();
            peers.clear();
            saveNetworkState(); // Salva lo stato vuoto su LittleFS
            
            // 2. Clear Prefixes (Keep only default)
            knownPrefixes.clear();
            if (strlen(mqtt_topic_prefix) > 0) {
                knownPrefixes.push_back(String(mqtt_topic_prefix));
            }
            saveDiscoveredPrefixes(); // Aggiorna prefixes.json
            
            server.send(200, "application/json", "{\"status\":\"network_reset\"}");
            return;
        }

        if (cmd == "LIST_PEERS_GLOBAL") {
            DevLog.println("[CMD] Avvio scansione rete globale (LIST_PEERS)...");
            
            for (const auto& prefix : knownPrefixes) {
                String cmdTopic = prefix + "/gateway/command";
                
                DevLog.printf("[CMD] Sending Discovery to prefix: %s\n", prefix.c_str());

                mqttClient.publish(cmdTopic.c_str(), "{\"command\":\"GATEWAY_HEARTBEAT\"}");
                delay(20);
                
                mqttClient.publish(cmdTopic.c_str(), "{\"command\":\"LIST_PEERS\"}");
                delay(20);
                
                mqttClient.publish(cmdTopic.c_str(), "{\"command\":\"PING_NETWORK\"}");
            }
            
            server.send(200, "application/json", "{\"status\":\"discovery_sent_global\"}");
            return;
        } 
        else {
            String payload;
            serializeJson(doc, payload); 
            
            DevLog.printf("[CMD] Inoltro comando generico: %s\n", cmd.c_str());
            DevLog.print("[CMD] Payload: ");
            DevLog.println(payload);

            for (const auto& prefix : knownPrefixes) {
                String cmdTopic = prefix + "/gateway/command";
                mqttClient.publish(cmdTopic.c_str(), payload.c_str());
            }
            
            server.send(200, "application/json", "{\"status\":\"command_forwarded_global\", \"command\":\"" + cmd + "\"}");
            return;
        }
    }
    
    server.send(400, "application/json", "{\"error\":\"Invalid params or missing command\"}");
}

// --- System Update Handlers ---
void handleUpdateFromUrl() {
    if (!server.hasArg("url")) {
        server.send(400, "text/plain", "Missing URL");
        return;
    }
    String url = server.arg("url");
    
    DevLog.printf("[OTA] Starting update from: %s\n", url.c_str());
    
    // Invia risposta immediata perché l'update bloccherà/riavvierà
    server.send(200, "text/plain", "Update started. Device will reboot.");
    delay(500); 
    
    WiFiClientSecure client;
    client.setInsecure();
    
    httpUpdate.setLedPin(-1);
    httpUpdate.rebootOnUpdate(true);
    
    t_httpUpdate_return ret = httpUpdate.update(client, url);
    
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            DevLog.printf("[OTA] Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            DevLog.println("[OTA] No updates");
            break;
        case HTTP_UPDATE_OK:
            DevLog.println("[OTA] Update OK");
            break;
    }
}

void handleSystemUpdate() {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        String error = "Update Failed. Error Code: " + String(Update.getError());
        DevLog.println(error);
        server.send(500, "text/plain", error);
    } else {
        server.send(200, "text/plain", "Update OK");
        DevLog.println("Update OK. Rebooting...");
        delay(1000);
        ESP.restart();
    }
}

void handleSystemUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        DevLog.printf("System Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
            Update.printError(DevLog);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(DevLog);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { 
            DevLog.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
            Update.printError(DevLog);
        }
    }
}

void cleanupStaleDevices() {
    // DISABILITATO SU RICHIESTA UTENTE
}

// --- Helper SemVer Comparison ---
// Returns: 1 if v1 > v2, -1 if v1 < v2, 0 if equal
int compareVersions(String v1, String v2) {
    int i = 0, j = 0;
    while (i < v1.length() || j < v2.length()) {
        int num1 = 0;
        // Skip non-digit characters for v1
        while (i < v1.length() && !isdigit(v1.charAt(i))) i++;
        
        while (i < v1.length() && isdigit(v1.charAt(i))) {
            num1 = num1 * 10 + (v1.charAt(i) - '0');
            i++;
        }
        
        int num2 = 0;
        // Skip non-digit characters for v2
        while (j < v2.length() && !isdigit(v2.charAt(j))) j++;

        while (j < v2.length() && isdigit(v2.charAt(j))) {
            num2 = num2 * 10 + (v2.charAt(j) - '0');
            j++;
        }
        
        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
        
        // Skip dots or other separators
        while (i < v1.length() && !isdigit(v1.charAt(i))) i++;
        while (j < v2.length() && !isdigit(v2.charAt(j))) j++;
    }
    return 0;
}

// --- GitHub Update Checker ---
void checkGithubUpdates() {
    if (WiFi.status() != WL_CONNECTED) {
        systemUpdates.lastResult = "No WiFi Connection";
        systemUpdates.lastCheck = millis();
        broadcastUpdate();
        return;
    }

    DevLog.println("[UPDATER] Checking for updates on GitHub...");
    DevLog.printf("[UPDATER] Free Heap: %d bytes\n", ESP.getFreeHeap());

    // Check DNS resolution
    IPAddress remote_ip;
    if (!WiFi.hostByName("raw.githubusercontent.com", remote_ip)) {
        DevLog.println("[UPDATER] DNS Failed for raw.githubusercontent.com");
        systemUpdates.lastResult = "DNS Failed";
        systemUpdates.lastCheck = millis(); // Update check time to notify frontend
        broadcastUpdate();
        return;
    }
    DevLog.printf("[UPDATER] DNS Resolved: %s\n", remote_ip.toString().c_str());
    
    HTTPClient http;
    // Usa setInsecure per semplicità
    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
        client->setInsecure();
        client->setHandshakeTimeout(20000); // 20s handshake timeout
        
        // Add cache busting parameter
        String url = String(GITHUB_VERSION_URL) + "?t=" + String(millis());
        
        if (http.begin(*client, url)) {
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setTimeout(20000); // 20s HTTP timeout

            DevLog.println("[UPDATER] Connecting to GitHub...");
            int httpCode = http.GET();
            DevLog.printf("[UPDATER] HTTP Code: %d\n", httpCode);

            systemUpdates.lastCheck = millis(); // Update timestamp regardless of result

            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DevLog.println("[UPDATER] Versions.json downloaded");
                // DevLog.println(payload); // Optional: Debug payload
                
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, payload);
                
                if (!error) {
                    systemUpdates.lastResult = "Success";
                    
                    // Dashboard Check
                    String onlineDashVer = doc["dashboard"]["version"].as<String>();
                    DevLog.printf("[UPDATER] Online Dash: %s, Current: %s\n", onlineDashVer.c_str(), FIRMWARE_VERSION);

                    if (compareVersions(onlineDashVer, FIRMWARE_VERSION) > 0) {
                        systemUpdates.dashboard.available = true;
                        systemUpdates.dashboard.version = onlineDashVer;
                        systemUpdates.dashboard.url = doc["dashboard"]["url"].as<String>();
                        systemUpdates.dashboard.notes = doc["dashboard"]["notes"].as<String>();
                        DevLog.printf("[UPDATER] Dashboard update found: %s\n", onlineDashVer.c_str());
                    } else {
                         systemUpdates.dashboard.available = false;
                    }
                    
                    // Gateway Check
                    if (doc.containsKey("gateway")) {
                        systemUpdates.gateway.version = doc["gateway"]["version"].as<String>();
                        systemUpdates.gateway.url = doc["gateway"]["url"].as<String>();
                        systemUpdates.gateway.notes = doc["gateway"]["notes"].as<String>();
                        systemUpdates.gateway.available = true;
                        DevLog.printf("[UPDATER] Gateway online ver: %s\n", systemUpdates.gateway.version.c_str());
                    }

                    // Node Check
                    if (doc.containsKey("nodes")) {
                        JsonObject nodesObj = doc["nodes"];
                        systemUpdates.nodes.clear();
                        
                        for (JsonPair kv : nodesObj) {
                            String typeName = kv.key().c_str();
                            JsonObject nodeData = kv.value().as<JsonObject>();
                            
                            UpdateInfo info;
                            info.version = nodeData["version"].as<String>();
                            info.url = nodeData["url"].as<String>();
                            info.notes = nodeData["notes"].as<String>();
                            info.available = true; 
                            
                            systemUpdates.nodes[typeName] = info;
                            DevLog.printf("[UPDATER] Node %s online ver: %s\n", typeName.c_str(), info.version.c_str());
                        }
                    } else if (doc.containsKey("node")) {
                        UpdateInfo info;
                        info.version = doc["node"]["version"].as<String>();
                        info.url = doc["node"]["url"].as<String>();
                        info.notes = doc["node"]["notes"].as<String>();
                        info.available = true;
                        systemUpdates.nodes["DEFAULT"] = info;
                    }
                    
                } else {
                    DevLog.println("[UPDATER] JSON parse error");
                    systemUpdates.lastResult = "JSON Parse Error";
                }
            } else {
                DevLog.printf("[UPDATER] HTTP Error: %d\n", httpCode);
                systemUpdates.lastResult = "HTTP Error: " + String(httpCode);
            }
            http.end();
        } else {
             DevLog.println("[UPDATER] Unable to connect");
             systemUpdates.lastResult = "Connection Failed";
             systemUpdates.lastCheck = millis();
        }
        
        broadcastUpdate(); // Always broadcast result
        delete client;
    } else {
        systemUpdates.lastResult = "Client Alloc Failed";
        systemUpdates.lastCheck = millis();
        broadcastUpdate();
    }
}

// --- WebSocket & Broadcast Helper ---

void broadcastUpdate() {
    DynamicJsonDocument doc(8192); 
    populateJson(doc);
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            break;
        case WStype_CONNECTED:
            {
                broadcastUpdate(); 
            }
            break;
        case WStype_TEXT:
            break;
    }
}

// --- Setup & Loop ---

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(3000); 
    
    DevLog.println("\n\n----------------------------------");
    DevLog.println("--- ESP32 Dashboard Controller ---");
    DevLog.println("----------------------------------");
    DevLog.print("Firmware Version: "); DevLog.println(FIRMWARE_VERSION);
    DevLog.print("Build Date: "); DevLog.print(BUILD_DATE); DevLog.print(" "); DevLog.println(BUILD_TIME);

    randomSeed(micros());
    
    DevLog.println("[SETUP] Caricamento configurazione...");
    loadConfiguration();
    loadDiscoveredPrefixes();
    loadNetworkState();

    bool prefixFound = false;
    for (const auto& p : knownPrefixes) {
        if (p == String(mqtt_topic_prefix)) { prefixFound = true; break; }
    }
    if (!prefixFound) {
        knownPrefixes.push_back(String(mqtt_topic_prefix));
        DevLog.printf("[SETUP] Aggiunto prefisso configurato: %s\n", mqtt_topic_prefix);
        saveDiscoveredPrefixes();
    }
    
    bool wifiConnected = false;
    
    if (strlen(wifi_ssid) > 0) {
        // Init NTP
        configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
        DevLog.printf("🕒 NTP Configured: %s, GMT: %ld, DST: %d\n", ntp_server, gmt_offset_sec, daylight_offset_sec);

        DevLog.printf("[SETUP] Tentativo connessione WiFi a: %s\n", wifi_ssid);
        WiFi.mode(WIFI_STA);
        
        if (strcmp(network_mode, "static") == 0 && strlen(static_ip) > 0) {
            DevLog.println("[SETUP] Configurazione IP Statico...");
            IPAddress ip, gw, sn, dns;
            if (ip.fromString(static_ip) && gw.fromString(static_gateway) && sn.fromString(static_subnet)) {
                if (strlen(static_dns) > 0 && dns.fromString(static_dns)) {
                    WiFi.config(ip, gw, sn, dns);
                    DevLog.printf("[SETUP] Static IP: %s, GW: %s, SN: %s, DNS: %s\n", static_ip, static_gateway, static_subnet, static_dns);
                } else {
                    WiFi.config(ip, gw, sn);
                    DevLog.printf("[SETUP] Static IP: %s, GW: %s, SN: %s\n", static_ip, static_gateway, static_subnet);
                }
            } else {
                DevLog.println("[SETUP] Errore parsing IP statici!");
            }
        }
        
        WiFi.begin(wifi_ssid, wifi_password);
        DevLog.print("[SETUP] Connessione in corso");
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            DevLog.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            DevLog.println("\n[SETUP] WiFi Connesso!");
            DevLog.print("[SETUP] IP Dashboard: ");
            DevLog.println(WiFi.localIP());
            wifiConnected = true;
        } else {
            DevLog.println("\n[SETUP] WiFi Fallito (Timeout).");
        }
    } else {
        DevLog.println("[SETUP] Nessun SSID configurato.");
    }
    
    if (wifiConnected) {
        configMode = false;
        
        DevLog.println("[SETUP] Configurazione MQTT...");
        mqttClient.setServer(mqtt_server, mqtt_port);
        mqttClient.setCallback(onMqttMessage);
        mqttClient.setBufferSize(4096); 
        mqttClient.setKeepAlive(60);    
        
        DevLog.println("[SETUP] Configurazione Web Server...");
        server.on("/", handleRoot);
        server.on("/api/data", HTTP_GET, handleApiData);
        server.on("/api/command", HTTP_POST, handleApiCommand);
        
        server.on("/debug", HTTP_GET, []() {
            server.send(200, "text/html", debug_html);
        });
        server.on("/api/logs", HTTP_GET, []() {
            server.send(200, "application/json", DevLog.getJSON());
        });
        server.on("/api/logs/clear", HTTP_POST, []() {
            DevLog.clear();
            server.send(200, "text/plain", "Cleared");
        });

        // Serve uploaded firmware for Nodes
        server.serveStatic("/temp_firmware.bin", LittleFS, "/temp_firmware.bin");

        // --- Firmware Upload Handler (Dashboard -> Dashboard FS) ---
        static File uploadFile;
        server.on("/api/upload_node_fw", HTTP_POST, [](){
            server.send(200, "text/plain", "OK");
        }, [](){
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                String filename = "/temp_firmware.bin";
                DevLog.printf("[UPLOAD] Inizio upload firmware nodo: %s\n", filename.c_str());
                if (LittleFS.exists(filename)) LittleFS.remove(filename);
                uploadFile = LittleFS.open(filename, "w");
                if (!uploadFile) DevLog.println("[UPLOAD] Errore apertura file");
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    uploadFile.write(upload.buf, upload.currentSize);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    DevLog.printf("[UPLOAD] Upload completato: %u bytes\n", upload.totalSize);
                } else {
                    DevLog.println("[UPLOAD] Errore: File non aperto alla fine dell'upload");
                }
            }
        });

        // --- Serve Temp Firmware for Node ---
        server.on("/temp_firmware.bin", HTTP_GET, [](){
            if (!LittleFS.exists("/temp_firmware.bin")) {
                server.send(404, "text/plain", "Firmware not found");
                return;
            }
            File f = LittleFS.open("/temp_firmware.bin", "r");
            server.streamFile(f, "application/octet-stream");
            f.close();
        });

        server.on("/settings", HTTP_GET, handleConfigRoot);
        server.on("/scan", HTTP_GET, []() {
            String networks = generateNetworksList();
            server.send(200, "text/plain", networks);
        });
        server.on("/save", HTTP_POST, handleConfigSave);
        server.on("/reboot", HTTP_POST, handleReboot);
        server.on("/reset_ap", HTTP_POST, handleResetToAP);
        
        server.on("/system/update", HTTP_POST, handleSystemUpdate, handleSystemUpdateUpload);
        server.on("/api/update_from_url", HTTP_POST, handleUpdateFromUrl);

        // --- Manual Update Check Endpoint ---
        server.on("/api/check_updates", HTTP_POST, []() {
            DevLog.println("[CMD] Manual Update Check requested via Web UI");
            checkGithubUpdates(); // Force check
            server.send(200, "application/json", "{\"status\":\"checked\"}");
        });

        webSocket.begin();
        webSocket.onEvent(webSocketEvent);
        DevLog.println("[SETUP] WebSocket Server avviato su porta 81");

        // --- Debug Command Handler ---
    server.on("/api/debug/command", HTTP_POST, []() {
        if (!server.hasArg("command")) {
            server.send(400, "text/plain", "Missing command");
            return;
        }
        
        String command = server.arg("command");
        DevLog.printf("[WEB-CMD] Ricevuto comando: %s\n", command.c_str());
        
        if (command == "status") {
            DevLog.println("\n=== DASHBOARD STATUS ===");
            DevLog.printf("WiFi SSID: %s (RSSI: %d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
            DevLog.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            DevLog.printf("MQTT Server: %s:%d\n", mqtt_server, mqtt_port);
            DevLog.printf("MQTT Status: %s\n", mqttClient.connected() ? "Connected" : "Disconnected");
            DevLog.printf("Gateway ID: %s\n", gateway_id);
            DevLog.printf("Known Prefixes: %d\n", knownPrefixes.size());
            DevLog.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
            DevLog.printf("Uptime: %lu min\n", millis() / 60000);
            DevLog.println("========================");
        } else if (command == "restart") {
            DevLog.println("Rebooting...");
            server.send(200, "text/plain", "Rebooting...");
            delay(1000);
            ESP.restart();
            return;
        } else if (command == "reset") {
            DevLog.println("Resetting WiFi config...");
            handleResetToAP(); // Riutilizza la funzione esistente
            return;
        } else if (command == "help") {
            DevLog.println("\n=== AVAILABLE COMMANDS ===");
            DevLog.println("status   - Show system status");
            DevLog.println("restart  - Reboot device");
            DevLog.println("reset    - Reset WiFi settings");
            DevLog.println("scan     - Scan WiFi networks");
            DevLog.println("mqtt     - Show MQTT info");
            DevLog.println("==========================");
        } else if (command == "scan") {
            DevLog.println("Scanning WiFi networks...");
            int n = WiFi.scanNetworks();
            DevLog.printf("Found %d networks:\n", n);
            for (int i = 0; i < n; ++i) {
                DevLog.printf("%d: %s (%d dBm)\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            }
        } else if (command == "mqtt") {
            DevLog.printf("MQTT Broker: %s\n", mqtt_server);
            DevLog.printf("Client ID: %s\n", gateway_id);
            DevLog.printf("Connected: %s\n", mqttClient.connected() ? "YES" : "NO");
        } else {
            DevLog.println("Unknown command. Type 'help' for list.");
        }
        
        server.send(200, "text/plain", "OK");
    });

    server.begin();
    DevLog.println("[SETUP] Web Server (Dashboard) avviato");
    } else {
        DevLog.println("[SETUP] Avvio AP Mode di configurazione...");
        startConfigAP();
    }
    
    DevLog.println("[SETUP] Setup completato.");
}

void loop() {
    if (configMode) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
    
    if (apTransitionPending && (millis() - apTransitionStart > 2000)) {
        DevLog.println("[LOOP] Riavvio programmato...");
        ESP.restart();
    }
    
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 10000) {
        lastCleanup = millis();
        cleanupStaleDevices();
    }
    
    if (!configMode && WiFi.status() == WL_CONNECTED) {
        webSocket.loop();
        
        // Check GitHub Updates
        if (millis() - systemUpdates.lastCheck > UPDATE_CHECK_INTERVAL || systemUpdates.lastCheck == 0) {
            checkGithubUpdates();
        }
        
        // Handle pending network state saves (Non-blocking)
        handleNetworkSave();
        
        if (dataChanged && (millis() - lastBroadcast > 250)) {
            broadcastUpdate();
            dataChanged = false;
            lastBroadcast = millis();
        }

        if (!mqttClient.connected()) {
            unsigned long now = millis();
            if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
                lastMqttReconnectAttempt = now;
                connectMQTT();
            }
        } else {
            mqttClient.loop();
        }
    }
    
    server.handleClient();
}
