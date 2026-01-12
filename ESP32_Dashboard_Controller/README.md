# 📱 ESP32 Dashboard Controller

## Panoramica
Il **Dashboard Controller** è il cuore pulsante e l'interfaccia utente del sistema domotico **Domoriky**. Basato su ESP32, funge da server centrale che aggrega i dati provenienti dai Gateway e dai Nodi, offrendo un'interfaccia web moderna e reattiva per il controllo della casa.

## Hardware Richiesto
- **MCU:** ESP32 (qualsiasi variante standard devkit).
- **Connessione:** WiFi (2.4GHz).

## Funzionalità Principali

### 1. Interfaccia Web (Dashboard)
- Ospita un server web integrato che serve una **Single Page Application (SPA)**.
- **Real-time:** Utilizza **WebSocket** per aggiornare lo stato dei dispositivi istantaneamente senza ricaricare la pagina.
- **Discovery:** Rileva automaticamente Gateway e Nodi presenti nella rete MQTT.

### 2. Gestione Aggiornamenti (OTA Centralizzato)
- **Check Versioni:** Controlla periodicamente un repository GitHub remoto per verificare la presenza di nuovi firmware per tutti i componenti del sistema (Dashboard, Gateway, Nodi).
- **Distribuzione:** Agisce come "orchestatore" per gli aggiornamenti. Scarica i metadati e permette all'utente di avviare l'aggiornamento dei singoli dispositivi direttamente dall'interfaccia web.

### 3. Logica e Comunicazione
- **Client MQTT:** Si sottoscrive ai topic di stato dei Gateway e invia comandi tramite MQTT.
- **Configurazione Dinamica:** Permette di configurare parametri di rete (WiFi, MQTT Broker) tramite interfaccia web o portale Captive Portal al primo avvio.

## Struttura del Codice
- `ESP32_Dashboard_Controller.ino`: Logica principale, setup WiFi/MQTT, loop.
- `index_html.h`: Contiene l'intero frontend (HTML/CSS/JS) compresso e ottimizzato.
- `DataManager.h/cpp`: Gestisce la persistenza dei dati (file JSON su LittleFS).
- `Structs.h`: Definisce le strutture dati condivise per il parsing JSON.

## Installazione e Avvio
1. Caricare lo sketch su ESP32.
2. Al primo avvio, se non configurato, crea un Access Point (AP) WiFi.
3. Connettersi all'AP e configurare SSID, Password WiFi e indirizzo Broker MQTT.
4. Una volta connesso, l'indirizzo IP verrà mostrato nel monitor seriale e il dispositivo inizierà a cercare Gateway sulla rete MQTT.
