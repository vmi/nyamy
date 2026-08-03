<#
.SYNOPSIS
    Build the NYamy manual (Markdown -> single-page HTML).

.DESCRIPTION
    Thin shim around tools/makedoc.py: it picks a Python interpreter and hands
    over.  Everything else - reading doc\src, link checking, writing the HTML -
    happens in the Python script, which reports a missing Python-Markdown with
    the exact pip command to run.

    Prerequisites:
      - Python 3.10 or later
      - Python-Markdown:  py -m pip install -r tools\requirements.txt

    Interpreter search order: $env:PYTHON -> py -3 -> python -> python3.  The
    py launcher comes first because a bare "python"/"python3" on PATH is often
    a Microsoft Store app-execution-alias stub.  Only Get-Command is used to
    pick one; a trial run is deliberately avoided, because executing such a
    stub can block instead of failing, which would hang the build.

.EXAMPLE
    tools\ps1exec.cmd tools\makedoc.ps1 doc\src doc
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$SrcDir,

    [Parameter(Mandatory = $true, Position = 1)]
    [string]$OutDir
)

$ErrorActionPreference = "Stop"

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $toolsDir "makedoc.py"

if (-not (Test-Path $script)) {
    Write-Error "makedoc: can't find $script"
    exit 1
}

# Exe plus any fixed leading arguments.  Kept as hashtables rather than nested
# arrays: PowerShell unrolls a single-element array on return, which would
# collapse @("-3") into the bare string "-3" and, once concatenated with the
# rest, hand py.exe one long argument instead of four.
$candidates = @()
if ($env:PYTHON) {
    $candidates += @{ Exe = $env:PYTHON; Pre = @() }
}
$candidates += @{ Exe = "py";      Pre = @("-3") }
$candidates += @{ Exe = "python";  Pre = @() }
$candidates += @{ Exe = "python3"; Pre = @() }

$python = $null
foreach ($cand in $candidates) {
    if (Get-Command $cand.Exe -ErrorAction SilentlyContinue) {
        $python = $cand
        break
    }
}

if (-not $python) {
    Write-Error "makedoc: no Python interpreter found (tried: py, python, python3). Set %PYTHON% to override."
    exit 1
}

$argv = @()
$argv += $python.Pre
$argv += $script
$argv += $SrcDir
$argv += $OutDir

& $python.Exe @argv
exit $LASTEXITCODE
