# ESP8266 Gateway MQTT & ESP-NOW

Gateway centrale per il sistema domotico che agisce come ponte tra i nodi ESP-NOW e il broker MQTT (gestito dalla Dashboard o esterno).

## Caratteristiche Principali

*   **Ponte ESP-NOW <-> MQTT**: Riceve messaggi dai nodi sensori/attuatori e li pubblica su MQTT; riceve comandi MQTT e li invia ai nodi via ESP-NOW.
*   **Interfaccia Web Semplificata**:
    *   Visualizzazione stato sistema (RAM, Uptime, Connessioni).
    *   **Tabella Nodi Linkati**: Mostra ID, MAC, Firmware e **Stato Online/Offline** dei nodi associati.
    *   Configurazione WiFi e MQTT.
    *   Web Serial Console per debug remoto.
*   **Discovery Automatico**: Supporta il protocollo di discovery per essere trovato automaticamente dalla Dashboard ESP32.
*   **Aggiornamento OTA**: Supporta l'aggiornamento firmware via OTA direttamente dalla Dashboard.

## Installazione

1.  Aprire il progetto con VS Code e PlatformIO (o Arduino IDE).
2.  Configurare `platformio.ini` se necessario.
3.  Compilare e caricare su ESP8266 (es. Wemos D1 Mini, NodeMCU).

## Configurazione Iniziale

1.  Al primo avvio, il Gateway crea un Access Point WiFi: `ESP8266-Gateway-Config`.
2.  Connettersi alla rete WiFi generata.
3.  Il Captive Portal dovrebbe aprirsi automaticamente (oppure navigare su `192.168.4.1`).
4.  Inserire:
    *   **SSID e Password** della rete WiFi domestica.
    *   **Indirizzo IP del Broker MQTT** (solitamente l'IP della Dashboard ESP32).
    *   **Gateway ID** (nome univoco del gateway).
5.  Salvare e riavviare.

## Comandi Seriali (USB e Web Console)

È possibile inviare comandi tramite la porta seriale (115200 baud) o tramite la Web Console (`/debug`):

| Comando | Descrizione |
| :--- | :--- |
| `status` | Mostra lo stato completo del gateway (WiFi, MQTT, Heap, ecc.) |
| `peers` | Mostra la lista dei nodi ESP-NOW associati (Peers) |
| `queue` | Mostra lo stato della coda messaggi |
| `restart` | Riavvia il dispositivo |
| `reset` | Cancella la configurazione WiFi e riavvia in modalità AP |
| `totalreset` | **Factory Reset**: Cancella TUTTO (WiFi, Peers, Configurazione) |
| `config` | Mostra il contenuto del file di configurazione JSON |
| `help` | Mostra la lista dei comandi disponibili |

## Pulsante Fisico (GPIO 0 / Flash Button)

*   **Pressione breve**: Nessuna azione (per evitare reset accidentali).
*   **Pressione 5-10 secondi**: Reset WiFi (LED fisso). Riavvia in modalità AP mantenendo i peer.
*   **Pressione > 10 secondi**: Factory Reset (LED lampeggio veloce). Cancella tutto.

## API Endpoints

*   `GET /`: Pagina principale di stato.
*   `GET /settings`: Pagina di configurazione.
*   `GET /debug`: Console seriale web.
*   `POST /save`: Salva la configurazione.
*   `POST /update_gateway`: Endpoint per aggiornamento firmware OTA.
*   `POST /trigger_ota`: Invia comando OTA a un nodo specifico (Richiede `nodeId` e `url`).
*   `GET /api/dashboard_info`: JSON per discovery dashboard.
*   `POST /api/dashboard_discover`: Invia pacchetto discovery UDP.
*   `GET /api/nodetypes`: Restituisce la configurazione dei tipi di nodo.
