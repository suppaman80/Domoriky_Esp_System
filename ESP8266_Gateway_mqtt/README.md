# 🌐 ESP8266 Gateway MQTT

## Panoramica
Il **Gateway MQTT** è il ponte di comunicazione fondamentale tra la rete locale IP (WiFi/MQTT) e la rete di sensori/attuatori a basso consumo basata su **ESP-NOW**. 
Il suo compito è tradurre i messaggi MQTT provenienti dalla Dashboard in comandi ESP-NOW per i nodi, e viceversa.

## Hardware Richiesto
- **MCU:** ESP8266 (es. Wemos D1 Mini, NodeMCU).
- **Connessione:** WiFi (Station Mode) + ESP-NOW.

## Funzionalità Principali

### 1. Bridge di Protocollo (MQTT <-> ESP-NOW)
- **Downstream (Dashboard -> Nodo):** Ascolta comandi su topic MQTT specifici e li inoltra via ESP-NOW al nodo destinatario (identificato via MAC address o ID).
- **Upstream (Nodo -> Dashboard):** Riceve pacchetti ESP-NOW dai nodi e pubblica il loro stato su topic MQTT per renderli visibili alla Dashboard.

### 2. Gestione Rete Mesh (Star Topology)
- Agisce come "Master" per i nodi ESP-NOW.
- Mantiene una lista dei nodi "pairati" (associati).
- Gestisce la registrazione di nuovi nodi tramite una procedura di pairing automatica o manuale.

### 3. Aggiornamenti OTA
- Supporta l'aggiornamento del proprio firmware via OTA (Over The Air) comandato dalla Dashboard.
- (Opzionale/Avanzato) Può fungere da relay per inviare firmware ai nodi ESP-NOW (funzionalità dipendente dalla versione).

## Struttura del Codice
- `ESP8266_Gateway_mqtt.ino`: Setup e loop principale.
- `EspNowHandler.h/cpp`: Gestione protocollo ESP-NOW (invio/ricezione messaggi raw).
- `MqttHandler.h/cpp`: Gestione connessione al broker MQTT e parsing topic.
- `PeerHandler.h/cpp`: Gestione della lista dei dispositivi connessi (Peers).

## Configurazione
1. Al primo avvio, entra in modalità AP per la configurazione WiFi e MQTT.
2. Una volta configurato, si connette al WiFi e al Broker MQTT.
3. Inizia automaticamente ad ascoltare messaggi ESP-NOW dai nodi circostanti.
