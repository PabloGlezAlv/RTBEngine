# Builds RTBEngine Release x64 and packages RTBEngine_SDK into dist/.
# CI equivalent of BuildSDK.bat (Release only, no pause, output under dist/).
param(
    [string]$EngineRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$OutputDir = '',
    [string]$AssimpToolset = '',
    [string]$PlatformToolset = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'ci-common.ps1')

# Pick MSVC / Assimp toolset for this machine (v143 on GitHub, v145 locally).
$toolchain = Get-RtbToolchain -PreferredAssimpToolset $AssimpToolset -PreferredPlatformToolset $PlatformToolset
Write-Host "Toolchain: PlatformToolset=$($toolchain.PlatformToolset) Assimp=$($toolchain.AssimpToolset)"

# Download or build Assimp if the matching vcXXX binaries are not in ThirdParty.
& (Join-Path $PSScriptRoot 'ci-ensure-assimp.ps1') -EngineRoot $EngineRoot -AssimpToolset $toolchain.AssimpToolset

# Vulkan headers/libs (vulkan-1.lib, shaderc_combined.lib) — uses VULKAN_SDK locally or installs on CI.
& (Join-Path $PSScriptRoot 'ci-ensure-vulkan.ps1')

# Fail early if SDL2, FMOD, Bullet, etc. are missing (must be committed in the repo).
$missing = Test-RequiredThirdPartyBinaries -EngineRoot $EngineRoot -Toolchain $toolchain -Configuration Release
if ($missing) {
    Write-Host 'Missing ThirdParty binaries:'
    @($missing) | ForEach-Object { Write-Host "  $_" }
    throw 'Required ThirdParty binaries are missing. Commit them (see scripts/thirdparty-required.txt).'
}

$msbuildProps = @{
    PlatformToolset  = $toolchain.PlatformToolset
    RTBAssimpToolset = $toolchain.AssimpToolset
}

# Compile RTBEngine.dll + RTBEngine.lib (Release x64).
$solution = Join-Path $EngineRoot 'RTBEngine\RTBEngine.sln'
Invoke-MsBuild -SolutionOrProject $solution -Configuration Release -Properties $msbuildProps

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $EngineRoot 'dist\RTBEngine_SDK'
}

# Fresh SDK output folder (same layout as BuildSDK.bat -> RTBEngine_SDK).
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}

$includeRoot = Join-Path $OutputDir 'Include'
$libRelease = Join-Path $OutputDir 'Lib\Release'
$binDir = Join-Path $OutputDir 'Bin'
$defaultDir = Join-Path $OutputDir 'Default'

New-Item -ItemType Directory -Force -Path $includeRoot, $libRelease, $binDir, $defaultDir | Out-Null

$engineDir = Join-Path $EngineRoot 'RTBEngine'
$thirdParty = Join-Path $engineDir 'ThirdParty'

# --- Include/ : engine + third-party headers ---
Write-Host 'Copying SDK headers...'
$engineSource = Join-Path $engineDir 'Engine'
$engineTarget = Join-Path $includeRoot 'RTBEngine'
if (Test-Path $engineTarget) { Remove-Item -Recurse -Force $engineTarget }
Copy-Item -Recurse -Force $engineSource $engineTarget
Copy-Item -Force (Join-Path $engineDir 'Engine\RTBEngine.h') (Join-Path $includeRoot 'RTBEngine.h')

@(
    @{ Src = 'SDL2-2.32.10\include'; Dst = 'SDL2' },
    @{ Src = 'lua\include'; Dst = 'Lua' },
    @{ Src = 'bullet3-3.25\src'; Dst = 'Bullet' },
    @{ Src = 'imgui-1.92.5'; Dst = 'ImGui' },
    @{ Src = 'luaBridge\Source'; Dst = 'LuaBridge' },
    @{ Src = 'glew-2.1.0\include\GL'; Dst = 'GL' },
    @{ Src = 'assimp\include\assimp'; Dst = 'assimp' }
) | ForEach-Object {
    $srcPath = Join-Path $thirdParty $_.Src
    $dstPath = Join-Path $includeRoot $_.Dst
    if (Test-Path $srcPath) {
        New-Item -ItemType Directory -Force -Path $dstPath | Out-Null
        Copy-Item -Recurse -Force (Join-Path $srcPath '*') $dstPath
    }
}

Get-ChildItem -Path (Join-Path $thirdParty 'fmod\api\core\inc') -Include '*.h', '*.hpp' -Recurse | ForEach-Object {
    Copy-Item -Force $_.FullName $includeRoot
}
Copy-Item -Force (Join-Path $thirdParty 'stb\stb_image_write.h') $includeRoot -ErrorAction SilentlyContinue

# --- Lib/Release/ : import libraries for linking ---
Write-Host 'Copying SDK libraries and runtime DLLs...'
Copy-Item -Force (Join-Path $engineDir 'x64\Release\RTBEngine.lib') $libRelease
Copy-Item -Force (Join-Path $engineDir 'x64\Release\RTBEngine.dll') $binDir

@(
    (Join-Path $thirdParty 'SDL2-2.32.10\lib\x64\SDL2.lib'),
    (Join-Path $thirdParty 'SDL2-2.32.10\lib\x64\SDL2main.lib'),
    (Join-Path $thirdParty 'glew-2.1.0\lib\Release\x64\glew32.lib'),
    (Join-Path $thirdParty 'bullet3-3.25\build_msvc\lib\Release\LinearMath.lib'),
    (Join-Path $thirdParty 'bullet3-3.25\build_msvc\lib\Release\BulletCollision.lib'),
    (Join-Path $thirdParty 'bullet3-3.25\build_msvc\lib\Release\BulletDynamics.lib'),
    (Join-Path $thirdParty "assimp\lib\Release\x64\$($toolchain.AssimpLib)"),
    (Join-Path $thirdParty 'lua\lua54.lib'),
    (Join-Path $thirdParty 'fmod\api\core\lib\x64\fmod_vc.lib')
) | ForEach-Object { Copy-Item -Force $_ $libRelease }

# --- Bin/ : runtime DLLs shipped with games / editor ---
@(
    (Join-Path $thirdParty 'SDL2-2.32.10\lib\x64\SDL2.dll'),
    (Join-Path $thirdParty 'glew-2.1.0\bin\Release\x64\glew32.dll'),
    (Join-Path $thirdParty "assimp\bin\x64\$($toolchain.AssimpDll)"),
    (Join-Path $thirdParty 'fmod\api\core\lib\x64\fmod.dll'),
    (Join-Path $thirdParty 'lua\lua54.dll')
) | ForEach-Object { Copy-Item -Force $_ $binDir }

# --- Default/ : built-in engine assets (shaders, etc.) ---
Copy-Item -Recurse -Force (Join-Path $engineDir 'Default\*') $defaultDir

Write-Host "SDK packaged at: $OutputDir"
