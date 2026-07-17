param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Configuration,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$Version
)

$ErrorActionPreference = "Stop"

# emulate WScript.Arguments behavior?
# JS: var targetDir = "..\\" + config + "\\";
# JS script is likely run from the 'tools' directory.
# PowerShell script location:
$scriptPath = $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $scriptPath
$rootDir = Split-Path -Parent $toolsDir
$targetDir = Join-Path $rootDir $Configuration

# Ensure target directory exists
if (-not (Test-Path $targetDir)) {
    # Try creating it? JS does: if (fso.FolderExists(targetDir) == false) { fso.CreateFolder(targetDir); }
    # But wait, JS calculates targetDir as "..\\" + config.
    # If config is "Debug", it looks for "../Debug".
    # Usually "Debug" folder is created by Visual Studio.
    # The JS script creates it if missing? That's odd for a "source" dir, but maybe it's just ensuring the path exists before writing the zip?
    # Actually, the zip is written INTO targetDir.
    # AND the files are read FROM targetDir?
    # Let's check JS again.

    # ProcessFiles(targetDir, files, PackFile);
    # PackFile uses 'dir' (which is targetDir) + 'name'.
    # So yes, source and dest are the same directory.

    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
}

$pkgFile = "nyamy-$Version.zip"
$pkgPath = Join-Path $targetDir $pkgFile

# Remove existing zip
if (Test-Path $pkgPath) {
    Remove-Item $pkgPath -Force
}

$files = @(
    "nyamy.ini",
    "104.mayu",
    "109.mayu",
    "default.mayu",
    "emacsedit.mayu",
    "104on109.mayu",
    "109on104.mayu",
    "dot.mayu",
    "workaround.mayu",
    "workaround.reg",
    "readme.txt",
    "nyamy.exe",
    "nyamy32.dll",
    "nyamyd32",
    "nyamy64.dll",
    "nyamy-scripter.exe",
    "nyamy-scripter.dll",
    "nyamy-scripter.lib",
    "nyamy_scripter.h"
)

$filesToArchive = @()

foreach ($file in $files) {
    $fullPath = Join-Path $targetDir $file
    if (Test-Path $fullPath) {
        $filesToArchive += $fullPath
    } else {
        # JS behavior: delete zip and throw error
        if (Test-Path $pkgPath) { Remove-Item $pkgPath -Force }
        Throw "can't pack $fullPath!"
    }
}

# Create Zip
# Note: Compress-Archive places files in the root of the zip if passed as a list of files.
# This matches the behavior of the Shell.Application NameSpace CopyHere method on flat items.
Compress-Archive -Path $filesToArchive -DestinationPath $pkgPath -Force
