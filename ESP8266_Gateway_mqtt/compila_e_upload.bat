@echo off
echo =====================================
echo 🔄 COMPILAZIONE E UPLOAD ESP8266
echo =====================================
echo.

echo 📦 Fase 1: Compilazione...
cd /d "%~dp0"

echo 🆙 Fase 0: Aggiornamento Versione...
set /p vtype="Tipo aggiornamento? [P]atch (default), [m]inor, [M]ajor: "
if "%vtype%"=="" set vtype=patch
if /i "%vtype%"=="p" set vtype=patch
if /i "%vtype%"=="m" set vtype=minor
if /i "%vtype%"=="M" set vtype=major

set /p desc="Inserisci descrizione modifica (Invio per saltare): "
powershell -ExecutionPolicy Bypass -Command "& './update_version.ps1' -Description '%desc%' -Type '%vtype%'"
echo.
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 ESP8266_Gateway_mqtt.ino --libraries ../libraries --verbose

if %errorlevel% neq 0 (
    echo ❌ Errore durante la compilazione!
    pause
    exit /b 1
)

echo.
echo 📤 Fase 2: Upload...
arduino-cli upload -p COM3 --fqbn esp8266:esp8266:nodemcuv2 --upload-property upload.speed=921600 ESP8266_Gateway_mqtt.ino

if %errorlevel% neq 0 (
    echo ❌ Errore durante l'upload!
    pause
    exit /b 1
)

echo.
echo ✅ Operazione completata con successo!
pause