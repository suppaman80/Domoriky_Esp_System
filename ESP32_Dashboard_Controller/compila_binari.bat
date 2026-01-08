@echo off
echo ===================================================
echo COMPILAZIONE BINARI ESP32 DASHBOARD
echo ===================================================
echo.

echo 🆙 Fase 0: Aggiornamento Versione...
set /p vtype="Tipo aggiornamento? [P]atch (default), [m]inor, [M]ajor: "
if "%vtype%"=="" set vtype=patch
if /i "%vtype%"=="p" set vtype=patch
if /i "%vtype%"=="m" set vtype=minor
if /i "%vtype%"=="M" set vtype=major

set /p desc="Inserisci descrizione modifica (Invio per saltare): "
powershell -ExecutionPolicy Bypass -Command "& './update_version.ps1' -Description '%desc%' -Type '%vtype%'"
echo.

REM Imposta il percorso delle librerie relative al progetto
set LIB_PATH=..\libraries
echo Librerie custom: %LIB_PATH%
echo.

echo 📦 Fase 1: Compilazione con export binari...
cd /d "%~dp0"
arduino-cli compile --fqbn esp32:esp32:esp32 --libraries "%LIB_PATH%" --export-binaries .

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Compilazione riuscita!
    echo I file binari aggiornati si trovano nella cartella 'build'.
) else (
    echo.
    echo [ERRORE] Compilazione fallita.
)

pause
