@echo off
setlocal EnableDelayedExpansion

echo ========================================================
echo   DOMOTICA SYSTEM - MULTI-RELEASE DEPLOYMENT
echo ========================================================
echo.
echo Questo script gestisce release INDIPENDENTI per ogni componente.
echo.

REM 1. Esegui script PS per aggiornare manifesto e ottenere versioni
echo [1/4] Rilevamento versioni e aggiornamento versions.json...
set versions_output=
for /f "delims=" %%i in ('powershell -ExecutionPolicy Bypass -File "update_manifest.ps1"') do set versions_output=%%i

if "%versions_output%"=="" (
    echo [ERRORE] Impossibile rilevare le versioni.
    goto :error_generic
)

REM Parsing delle versioni (formato: Dash;Gw;Node)
for /f "tokens=1,2,3 delims=;" %%a in ("%versions_output%") do (
    set VER_DASH=%%a
    set VER_GW=%%b
    set VER_NODE=%%c
)

REM Pulizia spazi
set VER_DASH=%VER_DASH: =%
set VER_GW=%VER_GW: =%
set VER_NODE=%VER_NODE: =%

echo Versioni Rilevate:
echo   - Dashboard: v%VER_DASH%
echo   - Gateway:   v%VER_GW%
echo   - Nodo:      v%VER_NODE%
echo.

REM 2. Definizione File Binari e Copia in /bin
echo [2/4] Preparazione Binari...
set BIN_DASH="ESP32_Dashboard_Controller\build\esp32.esp32.esp32\ESP32_Dashboard_Controller.ino.bin"
set BIN_GW="ESP8266_Gateway_mqtt\build\esp8266.esp8266.nodemcuv2\ESP8266_Gateway_mqtt.ino.bin"
set BIN_NODE="4_RELAY_CONTROLLER\build\esp8266.esp8266.nodemcuv2\4_RELAY_CONTROLLER.ino.bin"

REM Verifica e Compilazione Automatica
if not exist %BIN_DASH% (
    echo [INFO] Binario Dashboard mancante. Avvio compilazione...
    pushd "ESP32_Dashboard_Controller"
    arduino-cli compile --fqbn esp32:esp32:esp32 --libraries "..\libraries" --export-binaries .
    popd
    if not exist %BIN_DASH% ( echo [ERRORE] Compilazione Dashboard fallita & goto :error_generic )
)

if not exist %BIN_GW% (
    echo [INFO] Binario Gateway mancante. Avvio compilazione...
    pushd "ESP8266_Gateway_mqtt"
    arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --libraries "..\libraries" --export-binaries .
    popd
    if not exist %BIN_GW% ( echo [ERRORE] Compilazione Gateway fallita & goto :error_generic )
)

if not exist %BIN_NODE% (
    echo [INFO] Binario Nodo mancante. Avvio compilazione...
    pushd "4_RELAY_CONTROLLER"
    arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --libraries "..\libraries" --export-binaries .
    popd
    if not exist %BIN_NODE% ( echo [ERRORE] Compilazione Nodo fallita & goto :error_generic )
)

if not exist "bin" mkdir "bin"

echo Copia binari in /bin...
copy /Y %BIN_DASH% "bin\ESP32_Dashboard_Controller.ino.bin"
copy /Y %BIN_GW% "bin\ESP8266_Gateway_mqtt.ino.bin"
copy /Y %BIN_NODE% "bin\4_RELAY_CONTROLLER.ino.bin"

REM 3. Commit del manifesto e dei binari
echo [3/4] Commit e Push del manifesto e dei binari...
git add versions.json bin/*.bin
git commit -m "Update versions and binaries: Dash v%VER_DASH%, Gw v%VER_GW%, Node v%VER_NODE%"
git push origin master
echo.

REM 4. Gestione Release per ogni componente
echo [4/4] Gestione Release...

REM --- DASHBOARD ---
call :manage_release "Dashboard" "%VER_DASH%" %BIN_DASH%

REM --- GATEWAY ---
call :manage_release "Gateway" "%VER_GW%" %BIN_GW%

REM --- NODO ---
call :manage_release "Nodo" "%VER_NODE%" %BIN_NODE%

echo.
echo ========================================================
echo   TUTTE LE OPERAZIONI COMPLETATE CON SUCCESSO!
echo ========================================================
pause
exit /b 0

REM ========================================================
REM SUBROUTINE: manage_release
REM Argomenti: %1=NomeComponente, %2=Versione, %3=PercorsoBinario
REM ========================================================
:manage_release
set COMP_NAME=%~1
set COMP_VER=%~2
set COMP_BIN=%~3
set TAG=v%COMP_VER%

echo.
echo --- Gestione %COMP_NAME% (%TAG%) ---

REM 1. Tag Git
git tag %TAG% >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [GIT] Tag %TAG% creato.
    git push origin %TAG%
) else (
    echo [GIT] Tag %TAG% gia' esistente.
)

REM 2. Release GitHub
gh release view "%TAG%" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [GH] Release %TAG% trovata. Procedo all'upload...
) else (
    echo [GH] Creazione nuova release %TAG%...
    gh release create "%TAG%" --target master --title "Release %TAG%" --notes "Release automatica %COMP_NAME% %TAG%"
    
    REM Piccola attesa per propagazione
    timeout /t 2 /nobreak >nul
)

REM 3. Upload Binario
echo [GH] Upload binario per %COMP_NAME%...
gh release upload "%TAG%" %COMP_BIN% --clobber

if %ERRORLEVEL% EQU 0 (
    echo [OK] %COMP_NAME% rilasciato correttamente.
) else (
    echo [ERRORE] Fallito upload per %COMP_NAME%.
)

exit /b 0

:error_generic
echo Si e' verificato un errore critico.
pause
exit /b 1
