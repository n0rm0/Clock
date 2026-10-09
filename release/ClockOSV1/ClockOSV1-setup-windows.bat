@echo off
if /I "%~1"=="--clock-hidden" goto clock_hidden

set "VBS=%TEMP%\clock_setup_%RANDOM%.vbs"
>"%VBS%" echo Set sh = CreateObject("WScript.Shell")
>>"%VBS%" echo sh.Run Chr(34) ^& "%~f0" ^& Chr(34) ^& " --clock-hidden", 0, False
wscript.exe //nologo "%VBS%" >nul 2>&1
del "%VBS%" >nul 2>&1
exit /b 0

:clock_hidden
shift /1
setlocal EnableExtensions

set "PY="
for /f "delims=" %%P in ('where pyw.exe 2^>nul') do if not defined PY set "PY=%%P"
if not defined PY for /f "delims=" %%P in ('where pythonw.exe 2^>nul') do if not defined PY set "PY=%%P"

if not defined PY goto install_python
if defined PY goto python_ready

:install_python
where winget.exe >nul 2>&1
if not errorlevel 1 winget install --id Python.Python.3.12 -e --scope user --silent --accept-package-agreements --accept-source-agreements >nul 2>&1
for /f "delims=" %%P in ('where pyw.exe 2^>nul') do if not defined PY set "PY=%%P"
if not defined PY for /f "delims=" %%P in ('where pythonw.exe 2^>nul') do if not defined PY set "PY=%%P"
if not defined PY if exist "%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe" set "PY=%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe"
if not defined PY goto download_python
if not defined PY goto python_error

:download_python
set "PYINSTALL=%TEMP%\clock_python_%RANDOM%%RANDOM%.exe"
set "ARCH=%PROCESSOR_ARCHITEW6432%"
if not defined ARCH set "ARCH=%PROCESSOR_ARCHITECTURE%"
if /I "%ARCH%"=="ARM64" set "PYURL=https://www.python.org/ftp/python/3.12.10/python-3.12.10-arm64.exe"
if /I "%ARCH%"=="AMD64" set "PYURL=https://www.python.org/ftp/python/3.12.10/python-3.12.10-amd64.exe"
if /I "%ARCH%"=="x86" set "PYURL=https://www.python.org/ftp/python/3.12.10/python-3.12.10.exe"
if not defined PYURL set "PYURL=https://www.python.org/ftp/python/3.12.10/python-3.12.10-amd64.exe"
powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command "try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%PYURL%' -OutFile '%PYINSTALL%' } catch { exit 1 }" >nul 2>&1
if errorlevel 1 goto python_error
start "" /wait "%PYINSTALL%" /quiet InstallAllUsers=0 PrependPath=0 Include_pip=1 Include_tcltk=1 >nul 2>&1
del "%PYINSTALL%" >nul 2>&1
for /f "delims=" %%P in ('where pyw.exe 2^>nul') do if not defined PY set "PY=%%P"
if not defined PY for /f "delims=" %%P in ('where pythonw.exe 2^>nul') do if not defined PY set "PY=%%P"
if not defined PY if exist "%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe" set "PY=%LOCALAPPDATA%\Programs\Python\Python312\pythonw.exe"
if not defined PY goto python_error

:python_ready
set "PYFILE=%TEMP%\clock_install_%RANDOM%%RANDOM%.py"
powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -UseBasicParsing -Uri ('https://raw.githubusercontent.com/n0rm0/Clock/main/.source/install/install.py?t=' + [guid]::NewGuid()) -OutFile '%PYFILE%' } catch { exit 1 }" >nul 2>&1
if errorlevel 1 goto download_error
if not exist "%PYFILE%" goto download_error

start "" /wait "%PY%" "%PYFILE%" %*
set "RESULT=%ERRORLEVEL%"
del "%PYFILE%" >nul 2>&1
exit /b %RESULT%

:python_error
powershell.exe -NoProfile -WindowStyle Hidden -Command "Add-Type -AssemblyName PresentationFramework; [void][System.Windows.MessageBox]::Show('Python 3 with Tkinter could not be found or installed.','Clock Setup')" >nul 2>&1
exit /b 1

:download_error
del "%PYFILE%" >nul 2>&1
powershell.exe -NoProfile -WindowStyle Hidden -Command "Add-Type -AssemblyName PresentationFramework; [void][System.Windows.MessageBox]::Show('Could not download the Clock installer from GitHub.','Clock Setup')" >nul 2>&1
exit /b 1
