# 🏠 Domoriky Home Automation System

Benvenuto nella documentazione tecnica del sistema domotico **Domoriky**.
Questo progetto è una soluzione modulare, scalabile e completamente locale per la gestione della smart home, basata su microcontrollori ESP32 e ESP8266.

## 📐 Architettura del Sistema

Il sistema si basa su un'architettura a tre livelli, progettata per separare le responsabilità, ottimizzare le comunicazioni radio e garantire robustezza.

graph TD
    User[Utente (Browser/PC/Smartphone)] <-->|HTTP / WebSocket| Dashboard[🖥️ ESP32 Dashboard Controller]
    Dashboard <-->|MQTT (WiFi)| Gateway[📡 ESP8266 Gateway]
    Gateway <-->|ESP-NOW (Proprietario 2.4GHz)| Node1[🔌 Nodo Relè 1]
    Gateway <-->|ESP-NOW| Node2[🔌 Nodo Relè 2]
    Gateway <-->|ESP-NOW| NodeN[🔌 Altri Nodi...]


### 1. 🖥️ Livello Dashboard (Il Cervello)
- **Componente:** `ESP32_Dashboard_Controller`
- **Ruolo:** È l'unica interfaccia verso l'utente. Ospita il server web e la logica di controllo.
- **Funzionamento:** 
  - Si connette al WiFi di casa.
  - Esegue un broker MQTT Client (o si connette a uno esterno se configurato).
  - Fornisce una Web App moderna per visualizzare stati e inviare comandi.
  - Gestisce il sistema di **Aggiornamenti OTA Centralizzati**: scarica i firmware da GitHub e notifica l'utente quando sono disponibili aggiornamenti per qualsiasi componente del sistema.

### 2. 📡 Livello Gateway (Il Traduttore)
- **Componente:** `ESP8266_Gateway_mqtt`
- **Ruolo:** Agisce da ponte trasparente tra il mondo IP (WiFi/MQTT) e il mondo dei sensori (ESP-NOW).
- **Funzionamento:**
  - Da un lato è connesso al broker MQTT (e quindi alla Dashboard).
  - Dall'altro crea una rete privata ESP-NOW.
  - Quando riceve un comando MQTT (`/gateway/command`), lo impacchetta e lo spedisce via radio al nodo specifico.
  - Quando riceve un pacchetto radio da un nodo, lo trasforma in messaggio JSON MQTT (`/gateway/status`) per la Dashboard.

### 3. 🔌 Livello Nodi (Gli Attuatori)
- **Componente:** Es. `4_RELAY_CONTROLLER`
- **Ruolo:** Eseguono il lavoro fisico (accendere luci, leggere sensori).
- **Funzionamento:**
  - **NON usano il WiFi:** Questo è fondamentale. Non hanno indirizzo IP, non appesantiscono il router e consumano meno energia.
  - Parlano solo ESP-NOW con il Gateway.
  - Sono estremamente reattivi (< 50ms latenza).

---

## 🚀 Flusso di Funzionamento (Esempio)

**Scenario: L'utente accende la luce della cucina.**

1. **Click:** L'utente preme il pulsante "Cucina" sulla Web App (Dashboard).
2. **WebSocket:** Il browser invia un messaggio JSON alla Dashboard (ESP32).
3. **MQTT:** La Dashboard pubblica un messaggio MQTT sul topic `domoriky/gateway/command` con payload `{ "target": "NODO_CUCINA", "cmd": "RELAY_1_ON" }`.
4. **Gateway:** Il Gateway riceve il messaggio MQTT.
5. **ESP-NOW:** Il Gateway cerca l'indirizzo MAC associato a "NODO_CUCINA" e trasmette il pacchetto via radio.
6. **Azione:** Il Nodo riceve il pacchetto e attiva il relè fisico.
7. **Feedback:** Il Nodo invia un pacchetto di conferma (nuovo stato) al Gateway via ESP-NOW.
8. **Risalita:** Il Gateway pubblica il nuovo stato su MQTT. La Dashboard aggiorna l'interfaccia utente in tempo reale.

---

## 🛠️ Tecnologie Utilizzate

- **ESP-NOW:** Protocollo di comunicazione peer-to-peer sviluppato da Espressif. Permette comunicazioni rapide e senza connessione (connectionless) tra dispositivi ESP.
- **MQTT:** Protocollo di messaggistica leggero publish/subscribe, standard de facto per l'IoT.
- **ArduinoJson:** Per la serializzazione e deserializzazione dei dati in formato JSON.
- **WebSocket:** Per comunicazioni full-duplex tra browser e Dashboard.
- **LittleFS:** Filesystem per salvare configurazioni e pagine web sui chip.

## 📦 Struttura del Repository

- `/ESP32_Dashboard_Controller`: Codice sorgente del controller centrale.
- `/ESP8266_Gateway_mqtt`: Codice sorgente del gateway.
- `/4_RELAY_CONTROLLER`: Codice sorgente per nodi attuatori a 4 canali.
- `/libraries`: Librerie condivise (es. `DomoticaEspNow` per incapsulare la logica di comunicazione).
- `/bin`: Contiene i file binari compilati per il rilascio.
- `versions.json`: File manifesto per il sistema di aggiornamento automatico.

## 🔄 Sistema di Aggiornamento
Il sistema è progettato per auto-aggiornarsi.
1. La Dashboard controlla periodicamente il file `versions.json` su GitHub.
2. Se trova una nuova versione, mostra un avviso nell'interfaccia web.
3. L'utente può avviare l'aggiornamento con un click.
4. La Dashboard scarica il firmware e aggiorna se stessa, o istruisce i Gateway/Nodi a fare lo stesso.
