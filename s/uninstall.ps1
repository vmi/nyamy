$ErrorActionPreference = 'Stop'

$installDir = $PSScriptRoot

# The installed files are moved to a sibling directory with this prefix before
# they are deleted; must match the prefix install.ps1 sweeps up.
$installRoot = Split-Path $installDir -Parent
$stashPrefix = 'NYamy.old-'

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

function Get-BlockingProcesses([string]$Dir) {
    # Processes that still have a module mapped from $Dir.  A mapped image
    # cannot be deleted, so this is what a failed removal should report.
    $prefix = $Dir.TrimEnd('\') + '\'
    $found = @()
    foreach ($process in Get-Process) {
        try {
            foreach ($module in $process.Modules) {
                if ($module.FileName -and
                    $module.FileName.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                    $found += "$($process.ProcessName) (PID $($process.Id)) holds $(Split-Path $module.FileName -Leaf)"
                }
            }
        }
        catch {
            # The process exited, or its module list is not readable from here.
        }
    }
    $found | Sort-Object -Unique
}

function Assert-NyamyNotRunning([string]$Dir) {
    $prefix = $Dir.TrimEnd('\') + '\'
    $running = @()
    foreach ($process in Get-Process -Name 'nyamy', 'nyamy-scripter', 'nyamyd32' -ErrorAction SilentlyContinue) {
        # An inaccessible path means an elevated instance: assume it is ours.
        $exePath = $null
        try { $exePath = $process.Path } catch { }
        if (-not $exePath -or $exePath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $running += "$($process.ProcessName) (PID $($process.Id))"
        }
    }
    if ($running) {
        throw ("NYamy is still running. Quit it and run uninstall again:`n  " + ($running -join "`n  "))
    }
}

function Move-InstalledFilesAside([string]$Dir) {
    # Returns the directory the files were moved to, or $null if nothing moved.
    #
    # Files that are still mapped into another process - the global hook DLLs
    # stay mapped for a while after NYamy exits - cannot be deleted, but they
    # can be renamed.  Moving them out first lets the install location itself
    # disappear right away, even when deleting the files has to be retried in
    # the background.
    #
    # uninstall.* stays behind: this script is being read from that directory
    # by the shell running it, which breaks if the file moves under it.
    $items = @(
        Get-ChildItem -LiteralPath $Dir -Force |
            Where-Object { $_.Name -notmatch '^uninstall\..+$' }
    )
    if (-not $items) {
        return $null
    }

    $stash = Join-Path `
        $installRoot `
        ($stashPrefix + [System.Guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $stash -Force | Out-Null

    $stuck = @()
    foreach ($item in $items) {
        try {
            Move-Item -LiteralPath $item.FullName -Destination (Join-Path $stash $item.Name) -ErrorAction Stop
        }
        catch {
            # Leave it in place; the cleanup process deletes it there instead.
            $stuck += $item.Name
        }
    }

    if ($stuck) {
        Write-Host ("Could not move aside: " + ($stuck -join ', '))
        foreach ($entry in Get-BlockingProcesses $Dir) {
            Write-Host "  $entry"
        }
    }
    if (-not (Get-ChildItem -LiteralPath $stash -Force)) {
        Remove-Item $stash -Force -ErrorAction SilentlyContinue
        return $null
    }
    return $stash
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
# NYamy must be gone before its files are removed.  Renaming and deleting say
# nothing about that: a running binary can be renamed just fine.
#
Assert-NyamyNotRunning $installDir

Write-Host ""
Write-Host "NOTE: %LOCALAPPDATA%\NYamy will NOT be removed."
Write-Host ""

#
# Remove the Startup shortcut, if any, before scheduling self-destruction
# (this does not depend on this process exiting, unlike the directory removal below)
#
Remove-StartupShortcut $installDir

#
# Get the files out of the way, then have a detached process delete both what
# was moved aside and the install directory itself once this one is gone
#
# A directory cannot be renamed or removed while any process has it as its
# current directory, this one included.  uninstall.cmd already steps out of it,
# but the script may be started by other means.  Set-Location would not do:
# it leaves the process-wide current directory, which is what Windows holds,
# untouched.
#
if ([Environment]::CurrentDirectory.TrimEnd('\').StartsWith(
        $installDir.TrimEnd('\'), [System.StringComparison]::OrdinalIgnoreCase)) {
    [Environment]::CurrentDirectory = $env:TEMP
}

Write-Phase "Removing $installDir..."
$stashDir = Move-InstalledFilesAside $installDir

$doomedDirs = @($installDir)
if ($stashDir) {
    $doomedDirs += $stashDir
}
$rmdirLines = ($doomedDirs | ForEach-Object { "rmdir /s /q `"$_`" 2>nul" }) -join "`n"
$existsLine = 'if ' + (($doomedDirs | ForEach-Object { "not exist `"$_`" " }) -join 'if ') + 'goto done'

#
# Create self-destruct cmd.  The removal is retried: this process still has to
# exit, the shell that started it may still sit in the directory, and hook DLLs
# are unmapped from other processes only as those get around to it.
#
$cleanupCmd = Join-Path `
    $env:TEMP `
    ("NYamy-Uninstall-" + [System.Guid]::NewGuid().ToString() + ".cmd")

$cmd = @"
@echo off
setlocal

rem ping, not timeout: timeout fails outright when stdin is redirected, which
rem would turn both loops below into a spin that gives up immediately.

:wait

tasklist /FI "PID eq $PID" | find "$PID" >nul
if not errorlevel 1 (
    ping -n 2 127.0.0.1 >nul
    goto wait
)

set /a tries=0

:retry

$rmdirLines
$existsLine

set /a tries+=1
if %tries% geq 30 goto done
ping -n 3 127.0.0.1 >nul
goto retry

:done

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
if ($stashDir) {
    Write-Host "Files a running process still holds are retried for a minute in"
    Write-Host "$stashDir; whatever survives that is harmless,"
    Write-Host "and installing NYamy again removes it."
}

exit 0
