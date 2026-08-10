$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'nyamy-installer.psm1') -Force

# Source directory (directory containing this script)
$sourceDir = $PSScriptRoot

# Destination directory
$targetDir = Join-Path $env:LOCALAPPDATA 'Programs\NYamy'

# Old versions are stashed next to the install directory, not inside it.
$installRoot = Split-Path $targetDir -Parent

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

function Move-InstalledFilesAside([string]$Dir) {
    if (-not (Get-ChildItem -LiteralPath $Dir -Force)) {
        return
    }

    $stash = Get-NYamyStashPath $installRoot

    # A file mapped into a running process cannot be deleted, but it can still
    # be renamed, and so can a directory containing one.  Global hook DLLs stay
    # mapped in other processes for a while after NYamy exits, which is why
    # deleting the old files outright is not reliable.
    try {
        Move-Item -LiteralPath $Dir -Destination $stash -ErrorAction Stop
        New-Item -ItemType Directory -Path $Dir -Force | Out-Null
        return
    }
    catch {
        # The directory itself is held open - a shell sitting in it, say.
        # Fall back to moving its entries one by one.
        Write-Host "Could not move the directory aside as a whole; moving its contents instead."
    }

    New-Item -ItemType Directory -Path $stash -Force | Out-Null
    $moved = @()
    foreach ($item in Get-ChildItem -LiteralPath $Dir -Force) {
        $destination = Join-Path $stash $item.Name
        try {
            Move-Item -LiteralPath $item.FullName -Destination $destination -ErrorAction Stop
            $moved += , @($destination, $item.FullName)
        }
        catch {
            # Put back what was already moved, so a failed install changes nothing.
            foreach ($pair in $moved) {
                Move-Item -LiteralPath $pair[0] -Destination $pair[1] -ErrorAction SilentlyContinue
            }
            Remove-Item $stash -Recurse -Force -ErrorAction SilentlyContinue

            $blocking = Get-NYamyBlockingProcess $Dir
            $hint = ''
            if ($blocking) {
                $hint = "`nStill in use by:`n  " + ($blocking -join "`n  ")
            }
            throw "Cannot move aside: $($item.FullName)`n$($_.Exception.Message)$hint"
        }
    }
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
    Write-NYamyPhase "Installation started: $targetDir"

    if ($sourceDir.TrimEnd('\').Equals($targetDir.TrimEnd('\'), [System.StringComparison]::OrdinalIgnoreCase) -or
        $sourceDir.StartsWith($targetDir.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Run install from the distribution folder, not from inside $targetDir."
    }

    Assert-NYamyNotRunning $targetDir 'install'

    # Create destination directory if it does not exist
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    #
    # Rename the previous version out of the way instead of deleting it
    #
    Write-NYamyPhase "Moving the previous version aside..."
    Move-InstalledFilesAside $targetDir

    #
    # Copy all files except install.*
    #
    Write-NYamyPhase "Copying files..."
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

    #
    # Delete the previous version, plus anything left over from earlier runs
    #
    Write-NYamyPhase "Removing the previous version..."
    Remove-NYamyStash $installRoot

    Write-NYamyPhase "Installation completed: $targetDir"
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
        'Create' { New-NYamyStartupShortcut $targetDir }
        'Remove' { Remove-NYamyStartupShortcut $targetDir }
    }
}
else {
    switch ($startupMode) {
        '--startup'         { New-NYamyStartupShortcut $targetDir }
        '--startup-only'    { New-NYamyStartupShortcut $targetDir }
        '--no-startup'      { Remove-NYamyStartupShortcut $targetDir }
        '--no-startup-only' { Remove-NYamyStartupShortcut $targetDir }
        '--skip-startup'    { }
    }
}
