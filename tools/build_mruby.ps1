# build_mruby.ps1 - Build mruby (x64) and copy artifacts to scripter/mruby-dist/
#
# This script targets x64 only.  The MSVC environment is automatically
# switched to amd64 if it is not already active.
#
# Prerequisites:
#   - Ruby (ruby.exe, native Windows — not Cygwin) in PATH or a standard location
#   - Visual Studio with MSVC x64 toolchain installed
#   - scripter/mruby/ populated (git submodule update --init)
#
# Usage:
#   .\tools\build_mruby.ps1 [Release] [Debug]
#
# Builds mruby and copies the resulting library and headers into:
#   scripter/mruby-dist/lib/x64/<Config>/mruby.lib
#   scripter/mruby-dist/include/

[CmdletBinding()]
param(
	# Positional args: one or both of 'Release' and 'Debug'.
	# Defaults to Release only when no arguments are given.
	# Examples:
	#   .\build_mruby.ps1
	#   .\build_mruby.ps1 Debug
	#   .\build_mruby.ps1 Release Debug
	[Parameter(Position = 0)]
	[string]$First  = '',

	[Parameter(Position = 1)]
	[string]$Second = ''
)

# Collect and deduplicate the requested configs
$Config = @($First, $Second) | Where-Object { $_ -ne '' } | Select-Object -Unique
if ($Config.Count -eq 0) { $Config = @('Release') }

foreach ($cfg in $Config) {
	if ($cfg -notin @('Release', 'Debug')) {
		Write-Error "Invalid argument '$cfg'. Valid values: Release, Debug."
	}
}

$ErrorActionPreference = 'Stop'

# Locate a native Windows Ruby (RubyInstaller or MSYS2 mingw/ucrt).
# Cygwin Ruby is excluded: its path translation layer is incompatible with
# the MSVC toolchain used by mruby's build system.
#
# Tries (in order):
#   1. ruby already on PATH — accepted only if not a Cygwin build
#   2. RubyInstaller default: C:\Ruby*\bin\ruby.exe  (newest version first)
#   3. MSYS2 native sub-environments under C:\msys64\:
#      ucrt64\bin, mingw64\bin, mingw32\bin
#      (usr\bin is Cygwin-style and is intentionally skipped)
# Returns a hashtable: { Exe = <full path>; NeedsPathUpdate = <bool> }
function Find-Ruby {
	# 1. PATH — skip if it turns out to be a Cygwin build
	$cmd = Get-Command ruby -ErrorAction SilentlyContinue
	if ($cmd) {
		$platform = (& $cmd.Source -e 'puts RUBY_PLATFORM' 2>$null)
		if ($platform -notmatch 'cygwin') {
			return @{ Exe = $cmd.Source; NeedsPathUpdate = $false }
		}
		Write-Host "Skipping PATH ruby ($($cmd.Source)): Cygwin Ruby is not supported."
	}

	# 2. RubyInstaller (C:\Ruby<ver>\bin\ruby.exe, newest first)
	$rubyCandidates = Get-Item 'C:\Ruby*\bin\ruby.exe' -ErrorAction SilentlyContinue |
		Sort-Object FullName -Descending
	if ($rubyCandidates) { return @{ Exe = $rubyCandidates[0].FullName; NeedsPathUpdate = $true } }

	# 3. MSYS2 native sub-environments (ucrt64/mingw64/mingw32 only)
	foreach ($sub in @('ucrt64', 'mingw64', 'mingw32')) {
		$p = "C:\msys64\$sub\bin\ruby.exe"
		if (Test-Path $p) { return @{ Exe = $p; NeedsPathUpdate = $true } }
	}

	return $null
}

$rubyResult = Find-Ruby
if (-not $rubyResult) {
	Write-Error (
		"A native Windows ruby.exe was not found.  Searched:`n" +
		"  PATH (Cygwin Ruby is excluded)`n" +
		"  C:\Ruby*\bin\ruby.exe`n" +
		"  C:\msys64\{ucrt64,mingw64,mingw32}\bin\ruby.exe`n" +
		"Install RubyInstaller (https://rubyinstaller.org/) and re-run, or add it to PATH."
	)
}
$rubyExe = $rubyResult.Exe
Write-Host "Using Ruby: $rubyExe"

# When Ruby was found outside PATH, add its bin directory so that rake and
# other gem binaries invoked by minirake (via Kernel#exec) can be found.
if ($rubyResult.NeedsPathUpdate) {
	$rubyBin = Split-Path $rubyExe -Parent
	$env:PATH = "$rubyBin;$env:PATH"
	Write-Host "Added to PATH: $rubyBin"
}

# Ensure the MSVC environment targets x64.
# VSCMD_ARG_TGT_ARCH is set by Launch-VsDevShell.ps1 / Developer PowerShell.
# If it is absent or not amd64, locate Launch-VsDevShell.ps1 via vswhere and activate it.
if ($env:VSCMD_ARG_TGT_ARCH -ne 'amd64') {
	$vswhere = @(
		(Join-Path $env:PROGRAMFILES          'Microsoft Visual Studio\Installer\vswhere.exe'),
		(Join-Path ${env:ProgramFiles(x86)}   'Microsoft Visual Studio\Installer\vswhere.exe')
	) | Where-Object { Test-Path $_ } | Select-Object -First 1
	if (-not $vswhere) {
		Write-Error (
			"MSVC x64 environment is not active and vswhere.exe was not found.`n" +
			"Install Visual Studio and re-run, or run from a 'Developer PowerShell for VS' (x64)."
		)
	}

	$vsInstallPath = & $vswhere -latest -products '*' `
		-requires 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64' `
		-property installationPath
	if (-not $vsInstallPath) {
		Write-Error (
			"No Visual Studio installation with MSVC x64 toolchain found.`n" +
			"Install Visual Studio with 'Desktop development with C++' workload and re-run."
		)
	}

	$launchScript = Join-Path $vsInstallPath 'Common7\Tools\Launch-VsDevShell.ps1'
	if (-not (Test-Path $launchScript)) {
		Write-Error "Launch-VsDevShell.ps1 not found at: $launchScript"
	}

	Write-Host "Activating MSVC x64 environment via: $launchScript"
	& $launchScript -Arch amd64 -SkipAutomaticLocation
	Write-Host "MSVC x64 environment activated."
}

$root     = Split-Path $PSScriptRoot -Parent
$mrubyDir = Join-Path $root 'scripter\mruby'
$buildDir = Join-Path $root 'scripter\mruby-build'
$distDir  = Join-Path $root 'scripter\mruby-dist'
$configRb = Join-Path $PSScriptRoot 'mruby_build_config.rb'

# Verify the submodule has been initialised
if (-not (Test-Path (Join-Path $mrubyDir 'minirake'))) {
	Write-Error "scripter/mruby/ is empty. Run: git submodule update --init scripter/mruby"
}

# Build all requested configs in a single minirake run.
# mruby_build_config.rb reads MRUBY_CONFIGS to decide which named builds to define.
$env:MRUBY_CONFIGS = $Config -join ','

Push-Location $mrubyDir
try {
	& $rubyExe minirake `
		"MRUBY_BUILD_DIR=$buildDir" `
		"MRUBY_CONFIG=$configRb"
} finally {
	Pop-Location
}

Remove-Item Env:\MRUBY_CONFIGS -ErrorAction SilentlyContinue

# Copy artifacts for each requested config.
# Each named build outputs to build/<Config>/lib/libmruby.lib.
#
# Headers are taken from the build output (build/<Config>/include), not the
# source tree (scripter/mruby/include): the build generates additional headers
# there (mruby/presym/id.h, mruby/presym/table.h, and per-gem headers under
# mruby/gems/) that do not exist in the source tree.  The generated headers are
# identical across configs, so the first requested config is used as the source.
$incSrc  = Join-Path $buildDir "$($Config[0])\include"
$incDest = Join-Path $distDir 'include'

if (-not (Test-Path $incSrc)) {
	Write-Error "Generated include directory not found: $incSrc"
}

# Replace the destination outright so stale headers from a previous source-tree
# copy do not linger.
if (Test-Path $incDest) { Remove-Item $incDest -Recurse -Force }
New-Item -ItemType Directory -Force -Path $incDest | Out-Null
Copy-Item (Join-Path $incSrc '*') $incDest -Recurse -Force
Write-Host "Copied headers from build/$($Config[0])/include -> scripter/mruby-dist/include"

foreach ($cfg in $Config) {
	$libSrc  = Join-Path $buildDir "$cfg\lib\libmruby.lib"
	$libDest = Join-Path $distDir "lib\x64\$cfg"
	New-Item -ItemType Directory -Force -Path $libDest | Out-Null
	Copy-Item $libSrc (Join-Path $libDest 'mruby.lib') -Force
	Write-Host "Copied: $cfg -> scripter/mruby-dist/lib/x64/$cfg/mruby.lib"
}
