@echo off
setlocal

rem install.cmd and install.ps1 are in the same directory
set "SCRIPT_DIR=%~dp0"
set "PS_EXE="

rem 1. use %PSHOST%
if defined PSHOST (
    set "PS_EXE=%PSHOST%"
    goto :run
)

rem 2. find pwsh.exe in %PATH%
where pwsh.exe >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%i in ('where pwsh.exe') do (
        set "PS_EXE=%%i"
        goto :run
    )
)

rem 3. find powershell.exe in %PATH%
where powershell.exe >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%i in ('where powershell.exe') do (
        set "PS_EXE=%%i"
        goto :run
    )
)

rem 4. check %ProgramFiles%\PowerShell\7\pwsh.exe
if exist "%ProgramFiles%\PowerShell\7\pwsh.exe" (
    set "PS_EXE=%ProgramFiles%\PowerShell\7\pwsh.exe"
    goto :run
)

rem 5. check %SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe
if exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" (
    set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
    goto :run
)

rem 6. not found
echo ERROR: PowerShell executable not found.>&2
exit /b 1

:run
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install.ps1" %*
exit /b %ERRORLEVEL%
