$ErrorActionPreference = 'Stop'

$installDir = $PSScriptRoot

Write-Host "Checking deletable files..."

#
# Check whether all files can be deleted.
# Exclude uninstall.*
#
Get-ChildItem -LiteralPath $installDir -Recurse -Force |
    Where-Object {
        $_.Name -notmatch '^uninstall\..+$'
    } |
    ForEach-Object {

        try {
            if ($_.PSIsContainer) {
                $testFile = Join-Path $_.FullName ([System.Guid]::NewGuid().ToString() + '.tmp')
                Set-Content -LiteralPath $testFile -Value '' -Encoding ASCII
                Remove-Item -LiteralPath $testFile -Force
            }
            else {
                $stream = [System.IO.File]::Open(
                    $_.FullName,
                    [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::ReadWrite,
                    [System.IO.FileShare]::None
                )
                $stream.Dispose()
            }
        }
        catch {
            throw "Cannot delete: $($_.FullName)"
        }
    }

Write-Host "All files are deletable."
Write-Host ""
Write-Host "NOTE: %LOCALAPPDATA%\NYamy will NOT be removed."
Write-Host ""

#
# Create self-destruct cmd
#
$cleanupCmd = Join-Path `
    $env:TEMP `
    ("NYamy-Uninstall-" + [System.Guid]::NewGuid().ToString() + ".cmd")

$cmd = @"
@echo off
setlocal

:wait

tasklist /FI "PID eq $PID" | find "$PID" >nul
if not errorlevel 1 (
    timeout /t 1 /nobreak >nul
    goto wait
)

rmdir /s /q "$installDir"

del "%~f0"
"@

Set-Content `
    -LiteralPath $cleanupCmd `
    -Value $cmd `
    -Encoding ASCII

#
# Launch cleanup process
#
Start-Process `
    -FilePath "$env:SystemRoot\System32\cmd.exe" `
    -ArgumentList "/c `"$cleanupCmd`"" `
    -WindowStyle Hidden

Write-Host "Uninstall scheduled."
Write-Host "The installation directory will be removed after this process exits."

exit 0
