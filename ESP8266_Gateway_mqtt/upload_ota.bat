@echo off
setlocal EnableDelayedExpansion
echo ===================================================
echo UPLOAD OTA ESP8266 GATEWAY
echo ===================================================
echo.

REM Richiedi IP
set /p TARGET_IP="Inserisci IP del Gateway (es. 192.168.99.19): "

if "%TARGET_IP%"=="" (
    echo IP non valido.
    pause
    exit /b
)

REM Aggiornamento automatico versione
echo Aggiornamento versione firmware...
powershell -ExecutionPolicy Bypass -File update_version.ps1
echo.

REM Imposta il percorso delle librerie relative al progetto
set LIB_PATH=..\libraries
echo Librerie custom: %LIB_PATH%
echo.

echo [1/2] Compilazione in corso...
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --libraries "%LIB_PATH%" --export-binaries .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERRORE] Compilazione fallita!
    pause
    exit /b
)

echo.
echo [2/2] Avvio upload OTA su %TARGET_IP%...
echo Assicurati che il dispositivo sia online.
echo Password OTA preimpostata: admin
echo.

REM Upload via Network usando la cartella build generata e password admin
arduino-cli upload -p %TARGET_IP% --fqbn esp8266:esp8266:nodemcuv2 --input-dir build\esp8266.esp8266.nodemcuv2 --upload-field password=admin

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Upload OTA completato con successo!
) else (
    echo.
    echo [ERRORE] Upload OTA fallito.
    echo Verifica che:
    echo 1. L'IP %TARGET_IP% sia corretto e pingabile.
    echo 2. Il Gateway abbia gia' un firmware con OTA abilitato.
    echo 3. La password OTA sul dispositivo corrisponda a 'admin'.
)

pause
