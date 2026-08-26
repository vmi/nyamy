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

# The shortcuts NYamy installs, both with a fixed name so re-running the
# installer overwrites them instead of piling up duplicates.  'Programs' is the
# per-user Start Menu; the all-users one needs elevation, which this installer
# never has.
$NYamyStartupShortcutPath   = Join-Path ([Environment]::GetFolderPath('Startup'))  'NYamy.lnk'
$NYamyStartMenuShortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'NYamy.lnk'

# Tooltip, and what Start Menu search matches besides the name.  Same wording
# as the FileDescription in nyamy.exe.
$NYamyShortcutDescription = 'NYamy keyboard remapper'

# Every process that may hold a file from the install directory.
$NYamyProcessNames = @('nyamy', 'nyamy-scripter', 'nyamyd32')

function Write-NYamyPhase([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Test-NYamyShortcutTarget([string]$ShortcutPath, [string]$Dir) {
    # True when $ShortcutPath points into $Dir, i.e. it is ours to remove.
    # A shortcut left by another installation stays untouched.
    if (-not (Test-Path $ShortcutPath)) {
        return $false
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcutTarget = $shell.CreateShortcut($ShortcutPath).TargetPath
    if ([string]::IsNullOrEmpty($shortcutTarget)) {
        return $false
    }
    $resolvedDir = [System.IO.Path]::GetFullPath($Dir).TrimEnd('\')
    $shortcutTargetDir = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($shortcutTarget))
    return $shortcutTargetDir.Equals($resolvedDir, [System.StringComparison]::OrdinalIgnoreCase)
}

#
# The two shortcuts differ only in where they live and what they are called in
# the log, so creating and removing them is one implementation each.  $Label is
# 'Startup' or 'Start Menu'.
#

function New-NYamyShortcut([string]$ShortcutPath, [string]$Dir, [string]$Label,
                           [string]$Arguments = '') {
    # Save() fails outright if the containing folder is missing.
    New-Item -ItemType Directory -Path (Split-Path $ShortcutPath -Parent) -Force | Out-Null

    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    $shortcut.TargetPath = Join-Path $Dir 'nyamy.exe'
    $shortcut.Arguments = $Arguments
    $shortcut.WorkingDirectory = $Dir
    $shortcut.Description = $NYamyShortcutDescription
    # IconLocation is deliberately left alone: empty means "whatever the target
    # uses", and nyamy.exe carries its own icon.
    $shortcut.Save()
    Write-Host "$Label shortcut created: $ShortcutPath"
}

function Remove-NYamyShortcut([string]$ShortcutPath, [string]$Dir, [string]$Label) {
    if (-not (Test-Path $ShortcutPath)) {
        Write-Host "No $Label shortcut to remove."
        return
    }
    if (-not (Test-NYamyShortcutTarget $ShortcutPath $Dir)) {
        Write-Host "$Label shortcut points elsewhere; not removed: $ShortcutPath"
        return
    }
    Remove-Item $ShortcutPath -Force
    Write-Host "$Label shortcut removed: $ShortcutPath"
}

function New-NYamyStartupShortcut([string]$Dir) {
    # --startup tells nyamy.exe that losing the race against a manually started
    # instance is normal, so it exits without a dialog instead of greeting the
    # user with an error at every slow login.  The Start Menu shortcut gets no
    # such argument: a second launch by hand should say something.
    New-NYamyShortcut $NYamyStartupShortcutPath $Dir 'Startup' '--startup'
}

function Remove-NYamyStartupShortcut([string]$Dir) {
    Remove-NYamyShortcut $NYamyStartupShortcutPath $Dir 'Startup'
}

function New-NYamyStartMenuShortcut([string]$Dir) {
    New-NYamyShortcut $NYamyStartMenuShortcutPath $Dir 'Start Menu'
}

function Remove-NYamyStartMenuShortcut([string]$Dir) {
    Remove-NYamyShortcut $NYamyStartMenuShortcutPath $Dir 'Start Menu'
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
        'New-NYamyStartMenuShortcut',
        'Remove-NYamyStartMenuShortcut',
        'Get-NYamyBlockingProcess',
        'Assert-NYamyNotRunning',
        'Get-NYamyStashPath',
        'Remove-NYamyStash'
    ) `
    -Variable @(
        'NYamyStashPrefix',
        'NYamyStartupShortcutPath',
        'NYamyStartMenuShortcutPath',
        'NYamyProcessNames'
    )
