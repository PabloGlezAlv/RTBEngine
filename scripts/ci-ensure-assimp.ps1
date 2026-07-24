# Ensures Assimp import libraries and DLL exist for the requested MSVC toolset.
# If missing (e.g. vc143 on GitHub Actions but only vc145 is in LFS), installs via vcpkg
# and copies binaries into ThirdParty paths expected by the vcxproj / BuildSDK layout.
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [Parameter(Mandatory = $true)][string]$AssimpToolset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$thirdParty = Join-Path $EngineRoot 'RTBEngine\ThirdParty'
$assimpLib = Join-Path $thirdParty "assimp\lib\Release\x64\assimp-$AssimpToolset-mt.lib"
$assimpDll = Join-Path $thirdParty "assimp\bin\x64\assimp-$AssimpToolset-mt.dll"

if ((Test-Path $assimpLib) -and (Test-Path $assimpDll)) {
    Write-Host "Assimp binaries already present for $AssimpToolset"
    return
}

Write-Host "Assimp $AssimpToolset binaries missing. Installing via vcpkg..."

# Reuse VCPKG_ROOT if set; otherwise use runner temp (cached by the workflow).
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $env:RUNNER_TEMP 'vcpkg' }
if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
    if (-not (Test-Path $vcpkgRoot)) {
        git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgRoot
    }
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat')
}

& (Join-Path $vcpkgRoot 'vcpkg.exe') install assimp:x64-windows --triplet x64-windows
if ($LASTEXITCODE -ne 0) {
    throw 'vcpkg install assimp:x64-windows failed'
}

$installed = Join-Path $vcpkgRoot 'installed\x64-windows'
$vcpkgLib = Get-ChildItem -Path (Join-Path $installed 'lib') -Filter 'assimp*.lib' | Select-Object -First 1
$vcpkgDll = Get-ChildItem -Path (Join-Path $installed 'bin') -Filter 'assimp*.dll' | Select-Object -First 1

if (-not $vcpkgLib -or -not $vcpkgDll) {
    throw 'vcpkg assimp install did not produce expected lib/dll files.'
}

# Place files where ci-build-sdk.ps1 and the vcxproj expect them.
$libReleaseDir = Join-Path $thirdParty 'assimp\lib\Release\x64'
$binDir = Join-Path $thirdParty 'assimp\bin\x64'
New-Item -ItemType Directory -Force -Path $libReleaseDir | Out-Null
New-Item -ItemType Directory -Force -Path $binDir | Out-Null

Copy-Item -Force $vcpkgLib.FullName (Join-Path $libReleaseDir "assimp-$AssimpToolset-mt.lib")
Copy-Item -Force $vcpkgDll.FullName (Join-Path $binDir "assimp-$AssimpToolset-mt.dll")

Write-Host "Installed Assimp to ThirdParty for $AssimpToolset"
