# Ensures LunarG Vulkan SDK is available (headers + vulkan-1.lib + shaderc_combined.lib).
# Local dev: no-op when VULKAN_SDK is already set. CI: silent-install to C:\VulkanSDK\<version>.
param(
    [string]$VulkanVersion = '1.3.296.0'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-VulkanSdkLayout {
    param([string]$Root)

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return $false
    }

    $include = Join-Path $Root 'Include\vulkan\vulkan.h'
    $shaderc = Join-Path $Root 'Lib\shaderc_combined.lib'
    $loader = Join-Path $Root 'Lib\vulkan-1.lib'
    return (Test-Path $include) -and (Test-Path $shaderc) -and (Test-Path $loader)
}

function Set-VulkanSdkEnv {
    param([string]$Root)

    $env:VULKAN_SDK = $Root
    Write-Host "Vulkan SDK: $Root"

    if ($env:GITHUB_ENV) {
        "VULKAN_SDK=$Root" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
        "$(Join-Path $Root 'Bin')" | Out-File -FilePath $env:GITHUB_PATH -Append -Encoding utf8
    }
}

if (Test-VulkanSdkLayout -Root $env:VULKAN_SDK) {
    Write-Host "Vulkan SDK already configured for this session."
    return
}

$sdkRoot = Join-Path 'C:\VulkanSDK' $VulkanVersion
if (Test-VulkanSdkLayout -Root $sdkRoot) {
    Set-VulkanSdkEnv -Root $sdkRoot
    return
}

Write-Host "Installing Vulkan SDK $VulkanVersion..."

$temp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$installer = Join-Path $temp "VulkanSDK-$VulkanVersion-Installer.exe"
$url = "https://sdk.lunarg.com/sdk/download/$VulkanVersion/windows/VulkanSDK-$VulkanVersion-Installer.exe"

Invoke-WebRequest -Uri $url -OutFile $installer
$proc = Start-Process -FilePath $installer -ArgumentList @(
    '--accept-licenses',
    '--default-answer',
    '--confirm-command',
    'install'
) -Wait -PassThru

if ($proc.ExitCode -ne 0) {
    throw "Vulkan SDK installer exited with code $($proc.ExitCode)"
}

if (-not (Test-VulkanSdkLayout -Root $sdkRoot)) {
    throw "Vulkan SDK install finished but expected files are missing under $sdkRoot"
}

Set-VulkanSdkEnv -Root $sdkRoot
