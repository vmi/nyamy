param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Configuration,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$Version
)

$ErrorActionPreference = "Stop"

$scriptPath = $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $scriptPath
$rootDir = Split-Path -Parent $toolsDir
$targetDir = Join-Path $rootDir $Configuration

# Ensure target directory exists
if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
}

$suffix = if ($Configuration -eq "Debug") { "-debug" } else { "" }
$pkgName = "nyamy-$Version$suffix"
$pkgPath = Join-Path $targetDir "$pkgName.zip"

# Remove any previously built nyamy-*.zip in the target directory (e.g. left
# over from a different $Version), so stale packages don't accumulate
# alongside the one we're about to build.
Get-ChildItem -Path $targetDir -Filter "nyamy-*.zip" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

# Stage everything under a folder named after the archive itself, so
# extracting the zip creates a single top-level "$pkgName" folder instead of
# scattering files into whatever directory the user extracts into.
$stageDir = Join-Path $targetDir $pkgName
if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

#
# Payload: copy source files that aren't already build outputs into
# $stageDir. This list (source path relative to the repo root -> destination
# path relative to $stageDir) is the single place that defines what ends up
# in the distribution; distrib.vcxproj only tracks these paths as inputs to
# know when to re-run this script.
#
$payload = @(
    @{ Source = "104.mayu.rb";               Dest = "104.mayu.rb" }
    @{ Source = "104on109.mayu.rb";          Dest = "104on109.mayu.rb" }
    @{ Source = "109.mayu.rb";               Dest = "109.mayu.rb" }
    @{ Source = "109on104.mayu.rb";          Dest = "109on104.mayu.rb" }
    @{ Source = "default.mayu.rb";           Dest = "default.mayu.rb" }
    @{ Source = "dot.mayu.rb";               Dest = "dot.mayu.rb" }
    @{ Source = "emacsedit.mayu.rb";         Dest = "emacsedit.mayu.rb" }
    @{ Source = "workaround.mayu.rb";        Dest = "workaround.mayu.rb" }
    @{ Source = "workaround.reg";            Dest = "workaround.reg" }
    @{ Source = "nyamy.ini";                 Dest = "nyamy.ini" }
    @{ Source = "LICENSE.txt";               Dest = "LICENSE.txt" }
    @{ Source = "README.md";                 Dest = "README.md" }
    @{ Source = "scripter\nyamy_scripter.h"; Dest = "nyamy_scripter.h" }
    @{ Source = "doc\README-ja.html";        Dest = "doc\README-ja.html" }
    @{ Source = "doc\style.css";             Dest = "doc\style.css" }
    @{ Source = "doc\syntax.txt";            Dest = "doc\syntax.txt" }
    @{ Source = "doc\HISTORY-ja.html";       Dest = "doc\HISTORY-ja.html" }
    @{ Source = "doc\README-nyamy.html";     Dest = "doc\README-nyamy.html" }
    @{ Source = "doc\README-ymgw.html";      Dest = "doc\README-ymgw.html" }
    @{ Source = "doc\edit-setting-ja.png";   Dest = "doc\edit-setting-ja.png" }
    @{ Source = "doc\investigate-ja.png";    Dest = "doc\investigate-ja.png" }
    @{ Source = "doc\log-ja.png";            Dest = "doc\log-ja.png" }
    @{ Source = "doc\menu-ja.png";           Dest = "doc\menu-ja.png" }
    @{ Source = "doc\pause-ja.png";          Dest = "doc\pause-ja.png" }
    @{ Source = "doc\setting-ja.png";        Dest = "doc\setting-ja.png" }
    @{ Source = "doc\target.png";            Dest = "doc\target.png" }
    @{ Source = "doc\version-ja.png";        Dest = "doc\version-ja.png" }
    # install.ps1 copies everything beside it except install.* to the
    # install target, so these need to land flat next to nyamy.exe.
    @{ Source = "s\install.cmd";             Dest = "install.cmd" }
    @{ Source = "s\install.ps1";             Dest = "install.ps1" }
    @{ Source = "s\uninstall.cmd";           Dest = "uninstall.cmd" }
    @{ Source = "s\uninstall.ps1";           Dest = "uninstall.ps1" }
    # legacy/: mayu-era .mayu assets, kept under their own subfolder so they
    # don't collide with the .mayu.rb files of the same base name above.
    @{ Source = "legacy\104.mayu";                 Dest = "legacy\104.mayu" }
    @{ Source = "legacy\104on109.mayu";            Dest = "legacy\104on109.mayu" }
    @{ Source = "legacy\109.mayu";                 Dest = "legacy\109.mayu" }
    @{ Source = "legacy\109on104.mayu";            Dest = "legacy\109on104.mayu" }
    @{ Source = "legacy\default.mayu";             Dest = "legacy\default.mayu" }
    @{ Source = "legacy\dot.mayu";                 Dest = "legacy\dot.mayu" }
    @{ Source = "legacy\emacsedit.mayu";           Dest = "legacy\emacsedit.mayu" }
    @{ Source = "legacy\mayu-settings.txt";        Dest = "legacy\mayu-settings.txt" }
    @{ Source = "legacy\workaround.mayu";          Dest = "legacy\workaround.mayu" }
    @{ Source = "legacy\contrib\98x1.mayu";        Dest = "legacy\contrib\98x1.mayu" }
    @{ Source = "legacy\contrib\109onAX.mayu";     Dest = "legacy\contrib\109onAX.mayu" }
    @{ Source = "legacy\contrib\ax.mayu";          Dest = "legacy\contrib\ax.mayu" }
    @{ Source = "legacy\contrib\dvorak.mayu";      Dest = "legacy\contrib\dvorak.mayu" }
    @{ Source = "legacy\contrib\dvorak109.mayu";   Dest = "legacy\contrib\dvorak109.mayu" }
    @{ Source = "legacy\contrib\DVORAKon109.mayu"; Dest = "legacy\contrib\DVORAKon109.mayu" }
    @{ Source = "legacy\contrib\keitai.mayu";      Dest = "legacy\contrib\keitai.mayu" }
)

foreach ($item in $payload) {
    $srcPath = Join-Path $rootDir $item.Source
    if (-not (Test-Path $srcPath)) {
        Throw "can't stage $srcPath!"
    }
    $destPath = Join-Path $stageDir $item.Dest
    $destDir = Split-Path -Parent $destPath
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    }
    Copy-Item -Path $srcPath -Destination $destPath -Force
}

# Build outputs already sitting in $targetDir (produced by the other
# projects in the solution) need to be staged alongside the payload too.
$binaries = @(
    "nyamy.exe",
    "nyamy32.dll",
    "nyamyd32",
    "nyamy64.dll",
    "nyamy-scripter.exe",
    "nyamy-scripter.dll",
    "nyamy-scripter.lib"
)

foreach ($bin in $binaries) {
    $srcPath = Join-Path $targetDir $bin
    if (-not (Test-Path $srcPath)) {
        Throw "can't stage $srcPath!"
    }
    Copy-Item -Path $srcPath -Destination (Join-Path $stageDir $bin) -Force
}

# Create Zip
# Compress-Archive keeps the folder itself when -Path is a directory, so
# entries land as "$pkgName\nyamy.exe", "$pkgName\doc\README-ja.html", etc.
Compress-Archive -Path $stageDir -DestinationPath $pkgPath -Force
