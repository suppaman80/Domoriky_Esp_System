@echo off
echo =====================================
echo 🔄 COMPILAZIONE DASHBOARD ESP32
echo =====================================
echo.

echo 🆙 Fase 0: Aggiornamento Versione...
set /p vtype="Tipo aggiornamento? [P]atch (default), [m]inor, [M]ajor: "
if "%vtype%"=="" set vtype=patch
if /i "%vtype%"=="p" set vtype=patch
if /i "%vtype%"=="m" set vtype=minor
if /i "%vtype%"=="M" set vtype=major

set /p desc="Inserisci descrizione modifica (Invio per saltare): "
powershell -ExecutionPolicy Bypass -Command "& './update_version.ps1' -Description '%desc%' -Type '%vtype%'"
echo. 📦 Fase 1: Compilazione...
cd /d "%~dp0"
arduino-cli compile --fqbn esp32:esp32:esp32 ESP32_Dashboard_Controller.ino --libraries ../libraries

if %errorlevel% neq 0 (
    echo ❌ Errore durante la compilazione!
    pause
    exit /b 1
)

echo.
echo ✅ Operazione completata con successo!
pause
