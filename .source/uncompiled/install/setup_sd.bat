@echo off
REM Clock SD setup. Double-click = full install (wipes the SD card after 2 warnings).
REM "setup_sd.bat update" = refresh the SD card from GitHub without wiping.
REM Needs Python 3 (with tkinter). Runs hidden; a window with a progress bar appears.
if /i "%~1"=="--hidden" goto :run
set "VBS=%TEMP%\clock_setup_launch.vbs"
>"%VBS%" echo CreateObject("WScript.Shell").Run "cmd /c ""%~f0"" --hidden %~1", 0, False
wscript //nologo "%VBS%"
del "%VBS%" >nul 2>nul
exit /b

:run
set "MODE=%~2"
set "RUNPY="
where pythonw >nul 2>nul && set "RUNPY=pythonw"
if not defined RUNPY where pyw >nul 2>nul && set "RUNPY=pyw"
if not defined RUNPY goto :nopy

set "PY=%TEMP%\clock_install_%RANDOM%.py"
powershell -NoProfile -WindowStyle Hidden -Command "try { Invoke-WebRequest -UseBasicParsing -Uri ('https://raw.githubusercontent.com/n0rm0/Clock/main/.source/uncompiled/install/install.py?t=' + [guid]::NewGuid()) -OutFile '%PY%' } catch { exit 1 }"
if errorlevel 1 goto :nodl
if not exist "%PY%" goto :nodl

start /wait "" %RUNPY% "%PY%" %MODE%
del "%PY%" >nul 2>nul
exit /b

:nopy
powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [void][System.Windows.MessageBox]::Show('Python 3 was not found. Install it from python.org (tick Add to PATH) and run this again.','Clock SD setup')"
exit /b

:nodl
del "%PY%" >nul 2>nul
powershell -NoProfile -Command "Add-Type -AssemblyName PresentationFramework; [void][System.Windows.MessageBox]::Show('Could not download the installer from GitHub. Check your internet connection.','Clock SD setup')"
exit /b
