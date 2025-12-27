@echo off
setlocal EnableExtensions

REM ============================================================
REM  ps1exec.cmd
REM  Usage: ps1exec.cmd PSFile [arg ...]
REM  - Search order: pwsh -> powershell
REM  - Start with -NoProfile -ExecutionPolicy Bypass
REM  - Propagate the exit code of the PowerShell process
REM ============================================================

REM --- Argument check ---
if "%~1"=="" (
  echo Usage: %~nx0 PSFile [arg ...]
  exit /b 1
)

REM --- Get the PS script path (first argument) ---
set "PSFILE=%~1"
shift

REM --- Collect the remaining arguments, preserving spaces/special chars ---
set "PSARGS="
:collect_args
if "%~1"=="" goto args_done
    REM Wrap each argument with double-quotes safely
    set "PSARGS=%PSARGS% "%~1""
    shift
    goto collect_args
:args_done

REM --- Locate PowerShell host: prefer pwsh, then powershell ---

REM 1) Use predefined PSHOST
if defined PSHOST goto found

REM 2) Use where (PATH search)
for /f "usebackq delims=" %%P in (`where pwsh 2^>nul`) do (
    set "PSHOST=%%~fP"
    goto found
)
for /f "usebackq delims=" %%P in (`where powershell 2^>nul`) do (
    set "PSHOST=%%~fP"
    goto found
)

REM 3) Check common installation paths (in case PATH is not set)
if exist "%ProgramFiles%\PowerShell\7\pwsh.exe" (
    set "PSHOST=%ProgramFiles%\PowerShell\7\pwsh.exe"
    goto found
)
if exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" (
    set "PSHOST=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
    goto found
)

echo [ERROR] PowerShell executable ^(pwsh / powershell^) was not found.
exit /b 1

REM PSHOST found
:found

REM --- Execute: -NoProfile with temporary ExecutionPolicy Bypass ---
"%PSHOST%" -NoProfile -ExecutionPolicy Bypass -File "%PSFILE%" %PSARGS%
set "ERR=%ERRORLEVEL%"

REM --- Return the exit code ---
exit /b %ERR%
