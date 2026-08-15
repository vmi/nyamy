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
Usage: install.ps1 [options]

Options:
  --help                Show this help and exit

  --startup             Create/recreate the Startup shortcut (also runs the normal install)
  --startup-only        Create/recreate the Startup shortcut only (no file copy)
  --no-startup          Remove the Startup shortcut if present (also runs the normal install)
  --no-startup-only     Remove the Startup shortcut only (no file copy)
  --skip-startup        Leave the Startup shortcut as it is, without asking

  --startmenu           Create/recreate the Start Menu shortcut (same as the default)
  --startmenu-only      Create/recreate the Start Menu shortcut only (no file copy)
  --no-startmenu        Remove the Start Menu shortcut if present (also runs the normal install)
  --no-startmenu-only   Remove the Start Menu shortcut only (no file copy)
  --skip-startmenu      Leave the Start Menu shortcut as it is

With no option, the normal install runs, the Start Menu shortcut is created,
and you are asked whether to create or remove a Startup shortcut.

One option from each group may be given.  Any --*-only option skips the file
copy and leaves the group it does not name alone, so --startup-only does not
create the Start Menu shortcut that a plain install would.
--skip-startup --skip-startmenu together install the files and nothing else.
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
$startmenuModes = @('--startmenu', '--startmenu-only', '--no-startmenu', '--no-startmenu-only', '--skip-startmenu')
$startupMode = $null
$startmenuMode = $null

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
    elseif ($startmenuModes -contains $arg) {
        if ($startmenuMode) {
            Write-Host "ERROR: Only one of $($startmenuModes -join ', ') may be specified." -ForegroundColor Red
            Show-Usage
            exit 1
        }
        $startmenuMode = $arg
    }
    else {
        Write-Host "ERROR: Unknown option: $arg" -ForegroundColor Red
        Show-Usage
        exit 1
    }
}

# An --*-only option asks for shortcut maintenance without a file copy.  It also
# means "just this one": the group it does not name is left alone, so that
# --startup-only does not quietly create the Start Menu shortcut a plain install
# would have created.
$onlyModes = @('--startup-only', '--no-startup-only', '--startmenu-only', '--no-startmenu-only')
$shortcutOnly = ($onlyModes -contains $startupMode) -or ($onlyModes -contains $startmenuMode)

if ($shortcutOnly -and -not (Test-Path (Join-Path $targetDir 'nyamy.exe'))) {
    throw "NYamy is not installed in $targetDir yet. Run install without any --*-only option first."
}

if (-not $shortcutOnly) {
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
# Start Menu shortcut.  Unlike the Startup one this is created by default and
# without asking, so that a plain install always leaves a way to start NYamy:
# the install directory itself is not somewhere anyone goes looking.
#
if ($startmenuMode) {
    switch ($startmenuMode) {
        '--startmenu'         { New-NYamyStartMenuShortcut $targetDir }
        '--startmenu-only'    { New-NYamyStartMenuShortcut $targetDir }
        '--no-startmenu'      { Remove-NYamyStartMenuShortcut $targetDir }
        '--no-startmenu-only' { Remove-NYamyStartMenuShortcut $targetDir }
        '--skip-startmenu'    { }
    }
}
elseif (-not $shortcutOnly) {
    New-NYamyStartMenuShortcut $targetDir
}

#
# Startup shortcut.  Asked about rather than assumed: starting with Windows is
# a choice, and the answer defaults to leaving things as they are.
#
if ($startupMode) {
    switch ($startupMode) {
        '--startup'         { New-NYamyStartupShortcut $targetDir }
        '--startup-only'    { New-NYamyStartupShortcut $targetDir }
        '--no-startup'      { Remove-NYamyStartupShortcut $targetDir }
        '--no-startup-only' { Remove-NYamyStartupShortcut $targetDir }
        '--skip-startup'    { }
    }
}
elseif (-not $shortcutOnly) {
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
