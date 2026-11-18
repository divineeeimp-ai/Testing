<#
.SYNOPSIS
  Reports whether an SCP: Secret Laboratory install is Mono or IL2CPP and fingerprints the files SLMenu builds against.
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
        Write-Host ("  {0,-36} missing" -f (Split-Path $Path -Leaf))
        return
    }
    $f = Get-Item -LiteralPath $Path
    $sha = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    $ver = $f.VersionInfo.FileVersion
    Write-Host ("  {0,-36} {1,10} bytes  {2:yyyy-MM-dd HH:mm}  ver={3}" -f $f.Name, $f.Length, $f.LastWriteTime, $ver)
    Write-Host ("  {0,-36} sha256={1}" -f '', $sha)
}

if (-not (Test-Path -LiteralPath $GameDir)) {
    Write-Error "Game folder not found: $GameDir"
}

$exe       = Join-Path $GameDir 'SCPSL.exe'
$il2cpp    = Join-Path $GameDir 'GameAssembly.dll'
$managed   = Join-Path $GameDir 'SCPSL_Data\Managed'
$acs       = Join-Path $managed 'Assembly-CSharp.dll'
$monoDir   = Join-Path $GameDir 'MonoBleedingEdge'
$metadata  = Join-Path $GameDir 'SCPSL_Data\il2cpp_data\Metadata\global-metadata.dat'

Write-Host "Game folder: $GameDir"
Write-Host ("SCPSL.exe present:                 {0}" -f (Test-Path -LiteralPath $exe))
Write-Host ("GameAssembly.dll present (IL2CPP): {0}" -f (Test-Path -LiteralPath $il2cpp))
Write-Host ("global-metadata.dat present:       {0}" -f (Test-Path -LiteralPath $metadata))
Write-Host ("Managed\Assembly-CSharp.dll (Mono):{0}" -f (Test-Path -LiteralPath $acs))
Write-Host ("MonoBleedingEdge runtime folder:   {0}" -f (Test-Path -LiteralPath $monoDir))

$isMono   = (Test-Path -LiteralPath $acs) -and -not (Test-Path -LiteralPath $il2cpp)
$isIl2cpp = (Test-Path -LiteralPath $il2cpp)
if ($isMono)       { Write-Host "Scripting backend: MONO  -> SLMenu applies." -ForegroundColor Green }
elseif ($isIl2cpp) { Write-Host "Scripting backend: IL2CPP -> SLMenu (Mono) does not apply." -ForegroundColor Red }
else               { Write-Host "Scripting backend: undetermined (neither marker found)." -ForegroundColor Yellow }

Write-Host ""
Write-Host "Unity player:"
Show-File (Join-Path $GameDir 'UnityPlayer.dll')
Write-Host "Files SLMenu compiles against:"
foreach ($n in 'Assembly-CSharp.dll', 'Mirror.dll', 'Pooling.dll', 'UnityEngine.dll', 'UnityEngine.CoreModule.dll',
               'UnityEngine.IMGUIModule.dll', 'UnityEngine.InputLegacyModule.dll', 'UnityEngine.PhysicsModule.dll',
               'UnityEngine.TextRenderingModule.dll') {
    Show-File (Join-Path $managed $n)
}
