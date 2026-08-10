$ErrorActionPreference = 'Stop'

# Source directory (directory containing this script)
$sourceDir = $PSScriptRoot

# Destination directory
$targetDir = Join-Path $env:LOCALAPPDATA 'Programs\NYamy'

# Per-user Startup shortcut, fixed name so re-running --startup overwrites it
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'NYamy.lnk'

function Write-Phase([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Show-Usage {
    Write-Host @'
Usage: install.ps1 [option]

Options:
  --help              Show this help and exit
  --startup           Create/recreate the Startup shortcut (also runs the normal install)
  --startup-only      Create/recreate the Startup shortcut only (no file copy)
  --no-startup        Remove the Startup shortcut if present (also runs the normal install)
  --no-startup-only   Remove the Startup shortcut only (no file copy)
  --skip-startup      Run the normal install only, without asking about the Startup shortcut

With no option, the normal install runs and then you are asked whether to
create or remove a Startup shortcut.
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

function New-StartupShortcut([string]$Dir) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $Dir 'nyamy.exe'
    $shortcut.WorkingDirectory = $Dir
    $shortcut.Save()
    Write-Host "Startup shortcut created: $shortcutPath"
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
$startupModes = @('--startup', '--startup-only', '--no-startup', '--no-startup-only', '--skip-startup')
$startupMode = $null

foreach ($arg in $args) {
    if ($arg -eq '--help') {
        Show-Usage
        exit 0
    }
    elseif ($startupModes -contains $arg) {
        if ($startupMode) {
            Write-Host "ERROR: Only one of $($startupModes -join ', ') may be specified." -ForegroundColor Red
            Show-Usage
            exit 1
        }
        $startupMode = $arg
    }
    else {
        Write-Host "ERROR: Unknown option: $arg" -ForegroundColor Red
        Show-Usage
        exit 1
    }
}

$startupOnly = ($startupMode -eq '--startup-only') -or ($startupMode -eq '--no-startup-only')

if ($startupOnly -and -not (Test-Path (Join-Path $targetDir 'nyamy.exe'))) {
    throw "NYamy is not installed in $targetDir yet. Run install without --startup-only/--no-startup-only first."
}

if (-not $startupOnly) {
    Write-Phase "Installation started: $targetDir"

    # Create destination directory if it does not exist
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    #
    # Verify that all existing files/directories can be be deleted
    #
    Write-Phase "Checking deletable files..."
    $items = Get-ChildItem -Path $targetDir -Recurse -Force -ErrorAction Stop

    foreach ($item in $items) {
        try {
            if ($item.PSIsContainer) {
                # Verify write access to the directory
                $testFile = Join-Path $item.FullName ([System.Guid]::NewGuid().ToString() + '.tmp')
                [System.IO.File]::WriteAllText($testFile, '')
                Remove-Item $testFile -Force
            }
            else {
                # Verify that the file is not locked
                $stream = [System.IO.File]::Open(
                    $item.FullName,
                    [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::ReadWrite,
                    [System.IO.FileShare]::None
                )
                $stream.Dispose()
            }
        }
        catch {
            throw "Cannot delete file or directory: $($item.FullName)`n$($_.Exception.Message)"
        }
    }
    Write-Phase "All files are deletable."

    #
    # Remove all existing contents
    #
    Write-Phase "Removing existing files..."
    Get-ChildItem -Path $targetDir -Force |
        ForEach-Object {
            Write-Host "Removing: $($_.Name)"
            Remove-Item $_.FullName -Recurse -Force
        }

    #
    # Copy all files except install.*
    #
    Write-Phase "Copying files..."
    Get-ChildItem -Path $sourceDir -Recurse -File -Force |
        Where-Object {
            $_.Name -notmatch '^install\..+$'
        } |
        ForEach-Object {
            $relativePath = $_.FullName.Substring($sourceDir.Length).TrimStart('\')
            $destination = Join-Path $targetDir $relativePath

            $destinationDir = Split-Path $destination -Parent
            New-Item -ItemType Directory -Path $destinationDir -Force | Out-Null

            Write-Host "Copying: $relativePath"
            Copy-Item $_.FullName $destination -Force
        }

    Write-Phase "Installation completed: $targetDir"
}

#
# Startup shortcut
#
if (-not $startupMode) {
    Write-Host ""
    Write-Host "Create a Startup shortcut for NYamy?"
    $action = $null
    while (-not $action) {
        $answer = (Read-Host "[C]reate / [R]emove / [S]kip (default: Skip)").Trim().ToUpperInvariant()
        switch ($answer) {
            'C' { $action = 'Create' }
            'R' { $action = 'Remove' }
            '' { $action = 'Skip' }
            'S' { $action = 'Skip' }
            default { Write-Host "Please answer C, R, S, or press Enter." }
        }
    }
    switch ($action) {
        'Create' { New-StartupShortcut $targetDir }
        'Remove' { Remove-StartupShortcut $targetDir }
    }
}
else {
    switch ($startupMode) {
        '--startup'        { New-StartupShortcut $targetDir }
        '--startup-only'   { New-StartupShortcut $targetDir }
        '--no-startup'      { Remove-StartupShortcut $targetDir }
        '--no-startup-only' { Remove-StartupShortcut $targetDir }
        '--skip-startup'    { }
    }
}
