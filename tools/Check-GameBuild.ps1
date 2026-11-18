<#
.SYNOPSIS
  Reports the scripting backend of an SCP: Secret Laboratory install and fingerprints the files SLMenu depends on.
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\Check-GameBuild.ps1
  powershell -ExecutionPolicy Bypass -File tools\Check-GameBuild.ps1 -GameDir "D:\SteamLibrary\steamapps\common\SCP Secret Laboratory"
#>
param(
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\SCP Secret Laboratory'
)

$ErrorActionPreference = 'Stop'

function Show-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Host ("  {0,-24} missing" -f (Split-Path $Path -Leaf))
        return
    }
    $f = Get-Item -LiteralPath $Path
    $sha = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    Write-Host ("  {0,-24} {1,10} bytes  {2:yyyy-MM-dd HH:mm}  ver={3}" -f $f.Name, $f.Length, $f.LastWriteTime, $f.VersionInfo.FileVersion)
    Write-Host ("  {0,-24} sha256={1}" -f '', $sha)
}

if (-not (Test-Path -LiteralPath $GameDir)) {
    Write-Error "Game folder not found: $GameDir"
}

$exe      = Join-Path $GameDir 'SCPSL.exe'
$ga       = Join-Path $GameDir 'GameAssembly.dll'
$metadata = Join-Path $GameDir 'SCPSL_Data\il2cpp_data\Metadata\global-metadata.dat'
$acs      = Join-Path $GameDir 'SCPSL_Data\Managed\Assembly-CSharp.dll'

Write-Host "Game folder: $GameDir"
Write-Host ("SCPSL.exe present:                  {0}" -f (Test-Path -LiteralPath $exe))
Write-Host ("GameAssembly.dll present (IL2CPP):  {0}" -f (Test-Path -LiteralPath $ga))
Write-Host ("global-metadata.dat present:        {0}" -f (Test-Path -LiteralPath $metadata))
Write-Host ("Managed\Assembly-CSharp.dll (Mono): {0}" -f (Test-Path -LiteralPath $acs))

if (Test-Path -LiteralPath $ga) {
    Write-Host "Scripting backend: IL2CPP -> SLMenu applies." -ForegroundColor Green
} elseif (Test-Path -LiteralPath $acs) {
    Write-Host "Scripting backend: MONO -> SLMenu (IL2CPP) does not apply." -ForegroundColor Red
} else {
    Write-Host "Scripting backend: undetermined." -ForegroundColor Yellow
}

if (Test-Path -LiteralPath $metadata) {
    $fs = [IO.File]::OpenRead($metadata)
    try {
        $head = New-Object byte[] 8
        [void]$fs.Read($head, 0, 8)
    } finally {
        $fs.Dispose()
    }
    $magic = ($head[0..3] | ForEach-Object { $_.ToString('X2') }) -join ' '
    Write-Host ("Metadata magic: {0} ({1})  header version: {2}" -f $magic,
        $(if ($magic -eq 'AF 1B B1 FA') { 'plain' } else { 'NOT plain: encrypted or obfuscated' }),
        [BitConverter]::ToInt32($head, 4))
}

Write-Host ""
Write-Host "Fingerprints:"
Show-File (Join-Path $GameDir 'UnityPlayer.dll')
Show-File $ga
Show-File $metadata
