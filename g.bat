@echo off
REM ==========================================
REM  Git Commit Automator (no quotes needed)
REM  Uso: g Aggiunta automazione GIT
REM ==========================================
REM Controlla che sia stato passato almeno un argomento
if "%~1"=="" (
    echo Errore: devi specificare un messaggio di commit.
    echo Uso: g messaggio del commit
    exit /b 1
)
REM Ricostruisce il messaggio completo
set msg=
:loop
if "%~1"=="" goto continue
    if defined msg (
        set msg=%msg% %1
    ) else (
        set msg=%1
    )
shift
goto loop
:continue
echo Aggiungo i file modificati...
git add .
echo Creo il commit con messaggio: %msg%
git commit -m "%msg%"
echo Invio al repository remoto...
git push
echo Operazione completata.
