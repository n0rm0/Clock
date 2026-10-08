@echo off
setlocal EnableExtensions
REM Clock SD installer launcher.
REM Downloads .source/install/install.py from GitHub and runs it.
REM If Python is missing, installs the user-scoped Python 3 package with winget.

set "PY="
where py.exe >nul 2>nul && set "PY=py.exe -3"
if not defined PY where python.exe >nul 2>nul && set "PY=python.exe"

if not defined PY (
    where winget.exe >nul 2>nul
    if errorlevel 1 (
        echo Python is not installed and winget is unavailable.
        echo Install Python 3 from https://www.python.org/downloads/ and run this again.
        pause
        exit /b 1
    )
    echo Installing Python 3 for the current user...
    winget install --id Python.Python.3.12 -e --scope user --silent --accept-package-agreements --accept-source-agreements
    if errorlevel 1 (
        echo Python installation failed.
        pause
        exit /b 1
    )
    where py.exe >nul 2>nul && set "PY=py.exe -3"
    if not defined PY where python.exe >nul 2>nul && set "PY=python.exe"
)

if not defined PY (
    echo Python was installed but could not be found on PATH yet.
    echo Open a new Command Prompt and run setup_sd.bat again.
    pause
    exit /b 1
)

set "PYFILE=%TEMP%\clock_install_%RANDOM%%RANDOM%.py"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -UseBasicParsing -Uri ('https://raw.githubusercontent.com/n0rm0/Clock/main/.source/install/install.py?t=' + [guid]::NewGuid()) -OutFile '%PYFILE%' } catch { exit 1 }"
if errorlevel 1 (
    echo Could not download .source/install/install.py from GitHub.
    del "%PYFILE%" >nul 2>nul
    pause
    exit /b 1
)
if not exist "%PYFILE%" (
    echo The installer download did not produce a file.
    pause
    exit /b 1
)

%PY% "%PYFILE%" %*
set "RESULT=%ERRORLEVEL%"
del "%PYFILE%" >nul 2>nul
exit /b %RESULT%
