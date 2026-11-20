@echo off
setlocal
set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO_EXE%" (
    >&2 echo Could not find the VS Code-managed PlatformIO executable at "%PIO_EXE%".
    exit /b 1
)
"%PIO_EXE%" %*
exit /b %ERRORLEVEL%
