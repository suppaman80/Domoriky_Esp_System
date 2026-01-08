# Changelog

## [1.4.0] - 2026-01-06
- Nuova architettura OTA: Upload su Dashboard e download diretto dal nodo (bypass carico Gateway)

## [1.3.5] - 2026-01-03
- Aggiunti bottoni ai badge di riavvio nodo , eliminazione nodo, return to Ap mode

## [1.3.0] - 2026-01-03
- Aggiornati dati di stato

## [1.2.8] - 2025-12-29
- UI Update: Riorganizzato pannello "Stato Sistema" per replicare esattamente il layout dei Gateway (vedi immagine di riferimento)
- Feature: Aggiunto MAC Address della Dashboard nel pannello di stato
- Fix WebServer: Aggiunta terminazione corretta della risposta chunked in handleConfigRoot (server.sendContent("")) per evitare caricamenti infiniti

- Fix Critical Config HTML: Corretto errore di chiusura rawliteral che inglobava codice C++ nella stringa HTML
- Refactor Config HTML: Riscritto blocco generazione campi IP Statico per evitare errori di rendering e chiusura tag

## [1.2.3] - 2025-12-29
- Fix Config HTML: Corretta generazione attributi value nei campi IP statico

## [1.2.2] - 2025-12-29
- Fix HTML: Corretto rendering campi input IP statico (aggiunti doppi apici mancanti)
- Fix UI: Aggiunti spaziatori per migliorare layout configurazione IP

## [1.2.1] - 2025-12-29
- Ottimizzazione prestazioni: Debouncing salvataggio stato rete su LittleFS (evita blocchi loop)
- Ottimizzazione NTP: Timeout ridotto a 10ms per evitare blocchi se server non raggiungibile
- Fix dashboard: Aggiunto fallback per versione/build date mancanti

## [1.2.0] - 2025-12-29
- Aggiunto DNS nei settaggi per Ip statico e sisteamazine ora legale

## [1.1.2] - 2025-12-29
- Sistemato bug NTP che non funzionava correttamente

## [1.1.1] - 2025-12-29
- inserito comando di ascolto di discovery da parte dei gateway

## [1.1.0] - 2025-12-29
- Inserito protocollo NTP


