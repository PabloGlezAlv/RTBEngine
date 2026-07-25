# Shared MSBuild / MSVC helpers for CI release scripts.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Locates MSBuild.exe via the Visual Studio Installer (vswhere).
function Get-MsBuildPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe not found. Install Visual Studio Build Tools.'
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ([string]::IsNullOrWhiteSpace($installPath)) {
        throw 'Visual Studio with MSBuild was not found.'
    }

    $msbuild = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) {
        $msbuild = Join-Path $installPath 'MSBuild\15.0\Bin\MSBuild.exe'
    }
    if (-not (Test-Path $msbuild)) {
        throw "MSBuild.exe not found under $installPath"
    }

    return $msbuild
}

# Detects the installed Visual Studio version and maps it to MSVC / Assimp toolset names.
# VS 2022 (major 17) -> v143 / vc143 (GitHub Actions windows-latest)
# VS 2026 (major 18) -> v145 / vc145 (local dev)
function Get-RtbToolchain {
    param(
        [string]$PreferredAssimpToolset = '',
        [string]$PreferredPlatformToolset = ''
    )

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $version = & $vswhere -latest -products * -property installationVersion
    if ([string]::IsNullOrWhiteSpace($version)) {
        throw 'Could not detect Visual Studio version.'
    }

    $major = [int]($version.Split('.')[0])
    $defaultAssimp = switch ($major) {
        18 { 'vc145' }
        17 { 'vc143' }
        default { 'vc143' }
    }
    $defaultPlatform = switch ($major) {
        18 { 'v145' }
        17 { 'v143' }
        default { 'v143' }
    }

    $assimpToolset = if ([string]::IsNullOrWhiteSpace($PreferredAssimpToolset)) { $defaultAssimp } else { $PreferredAssimpToolset }
    $platformToolset = if ([string]::IsNullOrWhiteSpace($PreferredPlatformToolset)) { $defaultPlatform } else { $PreferredPlatformToolset }

    return [pscustomobject]@{
        VisualStudioVersion = $version
        VisualStudioMajor   = $major
        AssimpToolset       = $assimpToolset
        PlatformToolset     = $platformToolset
        AssimpLib           = "assimp-$assimpToolset-mt.lib"
        AssimpLibDebug      = "assimp-$assimpToolset-mtd.lib"
        AssimpDll           = "assimp-$assimpToolset-mt.dll"
    }
}

# Runs MSBuild on a solution or project with optional /p: overrides.
function Invoke-MsBuild {
    param(
        [Parameter(Mandatory = $true)][string]$SolutionOrProject,
        [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
        [string]$Platform = 'x64',
        [string]$Target = 'Rebuild',
        [hashtable]$Properties = @{}
    )

    $msbuild = Get-MsBuildPath
    $args = @(
        $SolutionOrProject,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/t:$Target",
        '/m',       # parallel build
        '/nologo',
        '/v:m'      # minimal log verbosity
    )

    foreach ($key in $Properties.Keys) {
        $args += "/p:$key=$($Properties[$key])"
    }

    Write-Host "MSBuild $($args -join ' ')"
    & $msbuild @args
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE"
    }
}

function Get-ThirdPartyRoot {
    param([string]$EngineRoot)
    return Join-Path $EngineRoot 'RTBEngine\ThirdParty'
}

# Verifies that prebuilt ThirdParty binaries exist before linking (must be in repo via Git LFS).
function Test-RequiredThirdPartyBinaries {
    param(
        [Parameter(Mandatory = $true)][string]$EngineRoot,
        [Parameter(Mandatory = $true)][object]$Toolchain,
        [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
    )

    $tp = Get-ThirdPartyRoot -EngineRoot $EngineRoot
    $required = @(
        (Join-Path $tp 'SDL2-2.32.10\lib\x64\SDL2.dll'),
        (Join-Path $tp 'SDL2-2.32.10\lib\x64\SDL2.lib'),
        (Join-Path $tp 'SDL2-2.32.10\lib\x64\SDL2main.lib'),
        (Join-Path $tp 'glew-2.1.0\bin\Release\x64\glew32.dll'),
        (Join-Path $tp 'glew-2.1.0\lib\Release\x64\glew32.lib'),
        (Join-Path $tp 'lua\lua54.dll'),
        (Join-Path $tp 'lua\lua54.lib'),
        (Join-Path $tp 'fmod\api\core\lib\x64\fmod.dll'),
        (Join-Path $tp 'fmod\api\core\lib\x64\fmod_vc.lib'),
        (Join-Path $tp "assimp\bin\x64\$($Toolchain.AssimpDll)"),
        (Join-Path $tp "assimp\lib\Release\x64\$($Toolchain.AssimpLib)"),
        (Join-Path $tp 'bullet3-3.25\build_msvc\lib\Release\LinearMath.lib'),
        (Join-Path $tp 'bullet3-3.25\build_msvc\lib\Release\BulletCollision.lib'),
        (Join-Path $tp 'bullet3-3.25\build_msvc\lib\Release\BulletDynamics.lib')
    )

    # Unary comma keeps an empty result as a 0-length array (not $null) for callers using StrictMode.
    return ,@($required | Where-Object { -not (Test-Path $_) })
}
