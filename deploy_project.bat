@echo off
echo ========================================================
echo   PROJECT DEPLOYMENT - GIT SYNC
echo ========================================================
echo.
echo Questo script esegue il backup di TUTTO il progetto su GitHub.
echo (git add . + git commit + git push)
echo.

set /p COMMIT_MSG="Inserisci messaggio di commit (invio per default): "

if "%COMMIT_MSG%"=="" (
    set COMMIT_MSG=Update Project %date% %time%
)

echo.
echo [1/3] Aggiunta file (git add .)...
git add .

echo.
echo [2/3] Commit (git commit)...
git commit -m "%COMMIT_MSG%"

echo.
echo [3/3] Push (git push)...
git push

echo.
echo ========================================================
echo   OPERAZIONE COMPLETATA!
echo ========================================================
pause
