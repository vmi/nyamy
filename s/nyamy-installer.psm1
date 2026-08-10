#
# Helpers shared by install.ps1 and uninstall.ps1.
#
# install.ps1 copies this module to the install directory alongside
# uninstall.ps1 (only install.* is held back), so the uninstaller finds it
# next to itself later on.
#

# Previous versions are renamed to a sibling of the install directory with
# this prefix and deleted afterwards; whatever cannot be deleted yet is
# retried on the next run.
$NYamyStashPrefix = 'NYamy.old-'

# Per-user Startup shortcut, fixed name so re-running --startup overwrites it.
$NYamyShortcutPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'NYamy.lnk'

# Every process that may hold a file from the install directory.
$NYamyProcessNames = @('nyamy', 'nyamy-scripter', 'nyamyd32')

function Write-NYamyPhase([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Test-NYamyShortcutTarget([string]$Dir) {
    # True when the Startup shortcut points into $Dir, i.e. it is ours to
    # remove.  A shortcut left by another installation stays untouched.
    if (-not (Test-Path $NYamyShortcutPath)) {
        return $false
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcutTarget = $shell.CreateShortcut($NYamyShortcutPath).TargetPath
    if ([string]::IsNullOrEmpty($shortcutTarget)) {
        return $false
    }
    $resolvedDir = [System.IO.Path]::GetFullPath($Dir).TrimEnd('\')
    $shortcutTargetDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($shortcutTarget))
    return $shortcutTargetDir.Equals($resolvedDir, [System.StringComparison]::OrdinalIgnoreCase)
}

function New-NYamyStartupShortcut([string]$Dir) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($NYamyShortcutPath)
    $shortcut.TargetPath = Join-Path $Dir 'nyamy.exe'
    $shortcut.WorkingDirectory = $Dir
    $shortcut.Save()
    Write-Host "Startup shortcut created: $NYamyShortcutPath"
}

function Remove-NYamyStartupShortcut([string]$Dir) {
    if (-not (Test-Path $NYamyShortcutPath)) {
        Write-Host "No Startup shortcut to remove."
        return
    }
    if (-not (Test-NYamyShortcutTarget $Dir)) {
        Write-Host "Startup shortcut points elsewhere; not removed: $NYamyShortcutPath"
        return
    }
    Remove-Item $NYamyShortcutPath -Force
    Write-Host "Startup shortcut removed: $NYamyShortcutPath"
}

function Get-NYamyBlockingProcess([string]$Dir) {
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

function Assert-NYamyNotRunning([string]$Dir, [string]$Action) {
    # Renaming works on running binaries too, so the file system no longer
    # tells us that NYamy is still up.  Ask the process list instead.
    $prefix = $Dir.TrimEnd('\') + '\'
    $running = @()
    foreach ($process in Get-Process -Name $NYamyProcessNames -ErrorAction SilentlyContinue) {
        # An inaccessible path means an elevated instance: assume it is ours.
        $exePath = $null
        try { $exePath = $process.Path } catch { }
        if (-not $exePath -or $exePath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $running += "$($process.ProcessName) (PID $($process.Id))"
        }
    }
    if ($running) {
        throw ("NYamy is still running. Quit it and run $Action again:`n  " + ($running -join "`n  "))
    }
}

function Get-NYamyStashPath([string]$Root) {
    # A not-yet-existing sibling directory to rename the old files into.
    Join-Path $Root ($NYamyStashPrefix + [System.Guid]::NewGuid().ToString('N').Substring(0, 8))
}

function Remove-NYamyStash([string]$Root) {
    # Delete every leftover stash, including one just created.  Files that are
    # still mapped elsewhere survive; the next install picks them up.
    $leftovers = @()
    foreach ($stash in @(Get-ChildItem -Path $Root -Directory -Force -Filter "$NYamyStashPrefix*" -ErrorAction SilentlyContinue)) {
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

Export-ModuleMember `
    -Function @(
        'Write-NYamyPhase',
        'Test-NYamyShortcutTarget',
        'New-NYamyStartupShortcut',
        'Remove-NYamyStartupShortcut',
        'Get-NYamyBlockingProcess',
        'Assert-NYamyNotRunning',
        'Get-NYamyStashPath',
        'Remove-NYamyStash'
    ) `
    -Variable @(
        'NYamyStashPrefix',
        'NYamyShortcutPath',
        'NYamyProcessNames'
    )
