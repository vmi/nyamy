$ErrorActionPreference = 'Stop'

# Source directory (directory containing this script)
$sourceDir = $PSScriptRoot

# Destination directory
$targetDir = Join-Path $env:LOCALAPPDATA 'Programs\NYamy'

# Previous versions are renamed to a sibling directory with this prefix and
# deleted afterwards; whatever cannot be deleted yet is retried on the next run.
$installRoot = Split-Path $targetDir -Parent
$stashPrefix = 'NYamy.old-'

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

function Get-BlockingProcesses([string]$Dir) {
    # Processes that still have a module mapped from $Dir.  A mapped image
    # cannot be deleted, so this is what a failed cleanup should report.
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
    # Renaming works on running binaries too, so the file system no longer
    # tells us that NYamy is still up.  Ask the process list instead.
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
        throw ("NYamy is still running. Quit it and run install again:`n  " + ($running -join "`n  "))
    }
}

function Move-InstalledFilesAside([string]$Dir) {
    if (-not (Get-ChildItem -LiteralPath $Dir -Force)) {
        return
    }

    $stash = Join-Path `
        $installRoot `
        ($stashPrefix + [System.Guid]::NewGuid().ToString('N').Substring(0, 8))

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

            $blocking = Get-BlockingProcesses $Dir
            $hint = ''
            if ($blocking) {
                $hint = "`nStill in use by:`n  " + ($blocking -join "`n  ")
            }
            throw "Cannot move aside: $($item.FullName)`n$($_.Exception.Message)$hint"
        }
    }
}

function Remove-Stashes([string]$Root) {
    # Delete every leftover stash, including the one just created.  Files that
    # are still mapped elsewhere survive; the next install picks them up.
    $leftovers = @()
    foreach ($stash in @(Get-ChildItem -Path $Root -Directory -Force -Filter "$stashPrefix*" -ErrorAction SilentlyContinue)) {
        Remove-Item $stash.FullName -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path $stash.FullName) {
            $leftovers += $stash.FullName
        }
    }
    if ($leftovers) {
        Write-Host "The previous version is still mapped into running processes;"
        Write-Host "these leftovers are harmless and will be removed by the next install:"
        foreach ($leftover in $leftovers) {
            Write-Host "  $leftover"
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
    Write-Phase "Installation started: $targetDir"

    if ($sourceDir.TrimEnd('\').Equals($targetDir.TrimEnd('\'), [System.StringComparison]::OrdinalIgnoreCase) -or
        $sourceDir.StartsWith($targetDir.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Run install from the distribution folder, not from inside $targetDir."
    }

    Assert-NyamyNotRunning $targetDir

    # Create destination directory if it does not exist
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    #
    # Rename the previous version out of the way instead of deleting it
    #
    Write-Phase "Moving the previous version aside..."
    Move-InstalledFilesAside $targetDir

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

    #
    # Delete the previous version, plus anything left over from earlier runs
    #
    Write-Phase "Removing the previous version..."
    Remove-Stashes $installRoot

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
