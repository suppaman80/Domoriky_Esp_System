# 🔌 4 Relay Controller Node

## Panoramica
Il **4 Relay Controller** è un nodo attuatore progettato per controllare carichi elettrici (luci, valvole, motori) tramite 4 relè. 
A differenza degli altri dispositivi, questo nodo **NON si connette al WiFi** di casa, ma comunica esclusivamente tramite protocollo **ESP-NOW** con il Gateway. Questo garantisce maggiore sicurezza, minor consumo, raggio d'azione esteso e nessuna congestione della rete WiFi domestica.

## Hardware Richiesto
- **MCU:** ESP8266 (es. Wemos D1 Mini) o ESP32.
- **Periferiche:** Modulo Relè a 4 canali (attivo alto o basso configurabile).

## Funzionalità Principali

### 1. Controllo Carichi
- Gestione indipendente di 4 canali (Relè).
- Supporto comandi ON/OFF/TOGGLE.

### 2. Comunicazione ESP-NOW
- Invia lo stato dei relè al Gateway ogni volta che cambia.
- Riceve comandi dal Gateway istantaneamente.
- **Zero Configurazione IP:** Non necessita di indirizzo IP, DHCP o password WiFi della rete domestica.

### 3. Configurazione Semplificata
- **Modalità Setup:** Avviando il dispositivo con un ponticello (o pulsante) tra `GPIO0` e `GND`, crea una rete WiFi temporanea.
- Tramite interfaccia web locale (192.168.4.1) è possibile impostare:
  - **Nome Nodo:** Identificativo univoco (es. "LUCI_SALOTTO").
  - **Gateway ID:** Identificativo del gateway a cui deve collegarsi.
  - **Mappatura Pin:** Quali GPIO controllano quali relè.

## Pinout Default (Configurabile)
- **Relè 1:** GPIO 12 (D6)
- **Relè 2:** GPIO 13 (D7)
- **Relè 3:** GPIO 14 (D5)
- **Relè 4:** GPIO 15 (D8)
- **Setup Mode:** GPIO 0 (D3)

## Installazione
1. Collegare il modulo relè ai pin configurati.
2. Alimentare il dispositivo.
3. Per configurare:
   - Spegnere.
   - Tenere premuto il tasto FLASH (GPIO0 a massa).
   - Accendere.
   - Collegarsi al WiFi "Domoriky_4_RelayNode".
   - Aprire `192.168.4.1` e configurare il nome e il gateway target.
   - Riavviare.
