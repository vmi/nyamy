$ErrorActionPreference = 'Stop'

$installDir = $PSScriptRoot

# Per-user Startup shortcut; must match the name install.ps1 uses.
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'NYamy.lnk'

function Write-Phase([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Show-Usage {
    Write-Host @'
Usage: uninstall.ps1 [option]

Options:
  --help    Show this help and exit
  --force   Remove NYamy without asking for confirmation
'@
}

function Test-ShortcutTargetsDir([string]$Dir) {
    if (-not (Test-Path $shortcutPath)) {
        return $false
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcutTarget = $shell.CreateShortcut($shortcutPath).TargetPath
    if ([string]::IsNullOrEmpty($shortcutTarget)) {
        return $false
    }
    $resolvedDir = [System.IO.Path]::GetFullPath($Dir).TrimEnd('\')
    $shortcutTargetDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($shortcutTarget))
    return $shortcutTargetDir.Equals($resolvedDir, [System.StringComparison]::OrdinalIgnoreCase)
}

function Remove-StartupShortcut([string]$Dir) {
    if (-not (Test-Path $shortcutPath)) {
        Write-Host "No Startup shortcut to remove."
        return
    }
    if (-not (Test-ShortcutTargetsDir $Dir)) {
        Write-Host "Startup shortcut points elsewhere; not removed: $shortcutPath"
        return
    }
    Remove-Item $shortcutPath -Force
    Write-Host "Startup shortcut removed: $shortcutPath"
}

#
# Argument parsing
#
$force = $false

foreach ($arg in $args) {
    if ($arg -eq '--help') {
        Show-Usage
        exit 0
    }
    elseif ($arg -eq '--force') {
        $force = $true
    }
    else {
        Write-Host "ERROR: Unknown option: $arg" -ForegroundColor Red
        Show-Usage
        exit 1
    }
}

#
# Confirmation
#
if (-not $force) {
    Write-Host "This will remove NYamy from $installDir."
    $confirmed = $null
    while ($null -eq $confirmed) {
        $answer = (Read-Host "Type 'yes' to continue, or 'no'/Enter to cancel").Trim().ToLowerInvariant()
        switch ($answer) {
            'yes' { $confirmed = $true }
            '' { $confirmed = $false }
            'no' { $confirmed = $false }
            default { Write-Host "Please answer 'yes' or 'no'." }
        }
    }
    if (-not $confirmed) {
        Write-Host "Uninstall cancelled."
        exit 0
    }
}

#
# Check whether all files can be deleted.
# Exclude uninstall.*
#
Write-Phase "Checking deletable files..."

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

Write-Phase "All files are deletable."
Write-Host ""
Write-Host "NOTE: %LOCALAPPDATA%\NYamy will NOT be removed."
Write-Host ""

#
# Remove the Startup shortcut, if any, before scheduling self-destruction
# (this does not depend on this process exiting, unlike the directory removal below)
#
Remove-StartupShortcut $installDir

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

Write-Phase "Uninstall scheduled: $installDir"
Write-Host "The installation directory will be removed after this process exits."

exit 0
