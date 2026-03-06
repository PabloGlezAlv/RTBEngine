@echo off
setlocal

echo ===========================================
echo Generating RTBEngine SDK...
echo ===========================================

set SOURCE_DIR=RTBEngine\Engine
set THIRD_PARTY_DIR=RTBEngine\ThirdParty
set OUTPUT_DIR=..\RTBEngineEditor\RTBEngineEditor\RTBEngine_SDK

:: 0. Find MSBuild
echo Locating MSBuild...
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VS_INSTALL_PATH=%%i
)

if "%VS_INSTALL_PATH%"=="" (
    echo [ERROR] Visual Studio installation not found.
    pause
    exit /b 1
)

set MSBUILD_PATH=%VS_INSTALL_PATH%\MSBuild\Current\Bin\MSBuild.exe
if not exist "%MSBUILD_PATH%" (
    set MSBUILD_PATH=%VS_INSTALL_PATH%\MSBuild\15.0\Bin\MSBuild.exe
)

if not exist "%MSBUILD_PATH%" (
    echo [ERROR] MSBuild.exe not found.
    pause
    exit /b 1
)

echo Found MSBuild at: "%MSBUILD_PATH%"

:: Detect toolset version dynamically
echo Detecting MSVC toolset version...
for /f "usebackq tokens=*" %%i in (`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationVersion`) do (
    set VS_VERSION=%%i
)
echo Detected Visual Studio version: %VS_VERSION%

:: Extract major version
for /f "tokens=1 delims=." %%i in ("%VS_VERSION%") do set VS_MAJOR_VERSION=%%i

:: Map VS version to toolset
set TOOLSET=vc143
if "%VS_MAJOR_VERSION%"=="17" (
    set TOOLSET=vc143
    echo Using toolset: vc143 (VS2022)
)
if "%VS_MAJOR_VERSION%"=="18" (
    set TOOLSET=vc145
    echo Using toolset: vc145 (VS2026)
)
echo Active toolset: %TOOLSET%

:: 1. Clean output directory
if exist "%OUTPUT_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%OUTPUT_DIR%"
)
mkdir "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\Include"
mkdir "%OUTPUT_DIR%\Lib"
mkdir "%OUTPUT_DIR%\Lib\Debug"
mkdir "%OUTPUT_DIR%\Lib\Release"
mkdir "%OUTPUT_DIR%\Bin"

:: 1.5 Compile Project (Debug and Release)
echo.
echo ===========================================
echo Building RTBEngine [Debug x64]...
echo ===========================================
"%MSBUILD_PATH%" RTBEngine\RTBEngine.sln /p:Configuration=Debug /p:Platform=x64 /t:Rebuild /m
if errorlevel 1 (
    echo [ERROR] Debug build failed.
    pause
    exit /b 1
)

echo.
echo ===========================================
echo Building RTBEngine [Release x64]...
echo ===========================================
"%MSBUILD_PATH%" RTBEngine\RTBEngine.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m
if errorlevel 1 (
    echo [ERROR] Release build failed.
    pause
    exit /b 1
)

:: 2. Copy Engine Headers
echo Copying headers...
xcopy "%SOURCE_DIR%\*.h" "%OUTPUT_DIR%\Include\RTBEngine\" /s /y /i >nul
copy "RTBEngine\Engine\RTBEngine.h" "%OUTPUT_DIR%\Include\" >nul

:: 3. Copy Third-party Headers
echo Copying third-party headers...
xcopy "%THIRD_PARTY_DIR%\SDL2-2.32.10\include\*.h" "%OUTPUT_DIR%\Include\SDL2\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\lua\include\*.h" "%OUTPUT_DIR%\Include\Lua\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\bullet3-3.25\src\*.h" "%OUTPUT_DIR%\Include\Bullet\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\imgui-1.92.5\*.h" "%OUTPUT_DIR%\Include\ImGui\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\luaBridge\Source\*.h" "%OUTPUT_DIR%\Include\LuaBridge\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\glew-2.1.0\include\GL\*.h" "%OUTPUT_DIR%\Include\GL\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\fmod\api\core\inc\*.hpp" "%OUTPUT_DIR%\Include\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\fmod\api\core\inc\*.h" "%OUTPUT_DIR%\Include\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\assimp\include\assimp\*.h" "%OUTPUT_DIR%\Include\assimp\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\assimp\include\assimp\*.hpp" "%OUTPUT_DIR%\Include\assimp\" /s /y /i >nul

:: 4. Copy Compiled Library
echo Copying RTBEngine.lib [Debug]...
if exist "RTBEngine\x64\Debug\RTBEngine.lib" (
    copy "RTBEngine\x64\Debug\RTBEngine.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
) else (
    echo [ERROR] RTBEngine.lib [Debug] not found.
)

echo Copying RTBEngine.lib [Release]...
if exist "RTBEngine\x64\Release\RTBEngine.lib" (
    copy "RTBEngine\x64\Release\RTBEngine.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
) else (
    echo [ERROR] RTBEngine.lib [Release] not found.
)

:: 5. Copy Static Dependencies
echo Copying static dependencies...

:: Release
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2main.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\glew-2.1.0\lib\Release\x64\glew32.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\LinearMath.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\BulletCollision.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\BulletDynamics.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\assimp\lib\Release\x64\assimp-%TOOLSET%-mt.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\lua\lua54.lib" "%OUTPUT_DIR%\Lib\Release\" >nul
copy "%THIRD_PARTY_DIR%\fmod\api\core\lib\x64\fmod_vc.lib" "%OUTPUT_DIR%\Lib\Release\" >nul

:: Debug
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2main.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\glew-2.1.0\lib\Release\x64\glew32.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Debug\LinearMath_Debug.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Debug\BulletCollision_Debug.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Debug\BulletDynamics_Debug.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\assimp\lib\Debug\x64\assimp-%TOOLSET%-mtd.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\lua\lua54.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul
copy "%THIRD_PARTY_DIR%\fmod\api\core\lib\x64\fmod_vc.lib" "%OUTPUT_DIR%\Lib\Debug\" >nul

:: 6. Copy Runtime DLLs
echo Copying DLLs to Bin directory...
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\glew-2.1.0\bin\Release\x64\glew32.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\assimp\bin\x64\assimp-%TOOLSET%-mt.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\fmod\api\core\lib\x64\fmod.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\lua\lua54.dll" "%OUTPUT_DIR%\Bin\" >nul

:: RTBEngine is now a DLL — copy it alongside the other runtime DLLs
echo Copying RTBEngine.dll [Release] to SDK Bin...
if exist "RTBEngine\x64\Release\RTBEngine.dll" (
    copy "RTBEngine\x64\Release\RTBEngine.dll" "%OUTPUT_DIR%\Bin\" >nul
) else (
    echo [WARN] RTBEngine.dll [Release] not found.
)

:: Copy RTBEngine.dll to the editor binary folders so the editor picks up the latest build
set EDITOR_OUT=..\RTBEngineEditor\RTBEngineEditor\x64
echo Copying RTBEngine.dll [Debug] to editor output...
if exist "RTBEngine\x64\Debug\RTBEngine.dll" (
    if not exist "%EDITOR_OUT%\Debug" mkdir "%EDITOR_OUT%\Debug"
    copy "RTBEngine\x64\Debug\RTBEngine.dll" "%EDITOR_OUT%\Debug\" >nul
) else (
    echo [WARN] RTBEngine.dll [Debug] not found.
)
echo Copying RTBEngine.dll [Release] to editor output...
if exist "RTBEngine\x64\Release\RTBEngine.dll" (
    if not exist "%EDITOR_OUT%\Release" mkdir "%EDITOR_OUT%\Release"
    copy "RTBEngine\x64\Release\RTBEngine.dll" "%EDITOR_OUT%\Release\" >nul
) else (
    echo [WARN] RTBEngine.dll [Release] not found.
)

:: Note: Assimp dependencies (zlib, kubazip, minizip, poly2tri, pugixml) are statically
:: linked into assimp-%TOOLSET%-mt.dll, so no additional dependency DLLs are needed.

:: 7. Copy Default Assets
echo Copying default assets...
xcopy "RTBEngine\Default\*.*" "%OUTPUT_DIR%\Default\" /s /y /i >nul

echo.
echo ===========================================
echo SDK Generated at: %OUTPUT_DIR%
echo ===========================================
pause
