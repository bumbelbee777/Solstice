# One-time script: convert all tracked text files to LF so Git stops complaining.
# Run from repo root: .\scripts\normalize-line-endings.ps1
# Then: git add -A; git commit -m "Normalize line endings to LF"
#
# If Cursor/IDE still warns "LF will be replaced by CRLF", the system Git has
# core.autocrlf=true. Override it once (terminal):
#   git config --global core.autocrlf false
# Then reload the Cursor window (Ctrl+Shift+P -> "Developer: Reload Window").

$ErrorActionPreference = "Stop"
$repoRoot = (Get-Item $PSScriptRoot).Parent.FullName
Set-Location $repoRoot

# Binary extensions from .gitattributes - do not touch
$binaryExt = @(
    '.png','.jpg','.jpeg','.gif','.ico','.bmp','.tga','.tif','.tiff','.webp','.psd','.xcf',
    '.obj','.fbx','.glb','.blend','.3ds','.stl',
    '.wav','.mp3','.ogg','.flac','.aiff',
    '.mp4','.avi','.mov','.mkv','.webm',
    '.ttf','.otf','.woff','.woff2','.eot',
    '.zip','.tar','.gz','.7z','.rar',
    '.exe','.dll','.so','.dylib','.lib','.a','.pdb','.bin'
)

$count = 0
$files = git ls-files
foreach ($rel in $files) {
    $ext = [System.IO.Path]::GetExtension($rel).ToLowerInvariant()
    if ($binaryExt -contains $ext) { continue }
    $path = Join-Path $repoRoot $rel
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $hasCrLf = $false
    for ($i = 0; $i -lt $bytes.Length - 1; $i++) {
        if ($bytes[$i] -eq 13 -and $bytes[$i+1] -eq 10) { $hasCrLf = $true; break }
    }
    if (-not $hasCrLf) { continue }
    # Replace CRLF with LF in place (preserves encoding/BOM)
    $list = [System.Collections.Generic.List[byte]]::new()
    $i = 0
    while ($i -lt $bytes.Length) {
        if ($i -lt $bytes.Length - 1 -and $bytes[$i] -eq 13 -and $bytes[$i+1] -eq 10) {
            $list.Add(10)
            $i += 2
        } else {
            $list.Add($bytes[$i])
            $i++
        }
    }
    [System.IO.File]::WriteAllBytes($path, $list)
    $count++
    Write-Host "LF: $rel"
}
Write-Host "Converted $count files to LF. Run: git add -A && git status"
