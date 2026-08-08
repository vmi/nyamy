$ErrorActionPreference = 'Stop'

# Source directory (directory containing this script)
$sourceDir = $PSScriptRoot

# Destination directory
$targetDir = Join-Path $env:LOCALAPPDATA 'Programs\NYamy'

# Create destination directory if it does not exist
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

#
# Verify that all existing files/directories can be be deleted
#
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

#
# Remove all existing contents
#
Get-ChildItem -Path $targetDir -Force |
    Remove-Item -Recurse -Force

#
# Copy all files except install.*
#
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

Write-Host "Installation completed."
Write-Host "Target directory: $targetDir"
