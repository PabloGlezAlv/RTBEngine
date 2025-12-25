@echo off
setlocal EnableDelayedExpansion

echo ===========================================
echo Generating RTBEngine SDK...
echo ===========================================

set SOURCE_DIR=RTBEngine\Engine
:: Correction: Build directory relative to this script
set BUILD_DIR=RTBEngine\x64\Debug
set THIRD_PARTY_DIR=RTBEngine\ThirdParty
set OUTPUT_DIR=RTBEngine_SDK

:: 1. Clean output directory
if exist "%OUTPUT_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%OUTPUT_DIR%"
)
mkdir "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\Include"
mkdir "%OUTPUT_DIR%\Lib"
mkdir "%OUTPUT_DIR%\Bin"

:: 2. Copy Engine Headers (maintaining structure)
echo Copying headers...
xcopy "%SOURCE_DIR%\*.h" "%OUTPUT_DIR%\Include\RTBEngine\" /s /y /i >nul

:: Copy RTBEngine.h to root of Include for easier inclusion
copy "RTBEngine\Engine\RTBEngine.h" "%OUTPUT_DIR%\Include\" >nul

:: 3. Copy Third-party Headers
echo Copying third-party headers...
xcopy "%THIRD_PARTY_DIR%\SDL2-2.32.10\include\*.h" "%OUTPUT_DIR%\Include\SDL2\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\lua\include\*.h" "%OUTPUT_DIR%\Include\Lua\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\bullet3-3.25\src\*.h" "%OUTPUT_DIR%\Include\Bullet\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\imgui-1.92.5\*.h" "%OUTPUT_DIR%\Include\ImGui\" /s /y /i >nul
xcopy "%THIRD_PARTY_DIR%\luaBridge\Source\*.h" "%OUTPUT_DIR%\Include\LuaBridge\" /s /y /i >nul

:: 4. Copy Compiled Library (RTBEngine.lib)
echo Copying RTBEngine.lib from %BUILD_DIR%...
if exist "%BUILD_DIR%\RTBEngine.lib" (
    copy "%BUILD_DIR%\RTBEngine.lib" "%OUTPUT_DIR%\Lib\" >nul
) else (
    echo [ERROR] RTBEngine.lib not found in %BUILD_DIR%. Checking Release...
    if exist "RTBEngine\x64\Release\RTBEngine.lib" (
        echo Found in Release! Copying...
        copy "RTBEngine\x64\Release\RTBEngine.lib" "%OUTPUT_DIR%\Lib\" >nul
    ) else (
        echo [FATAL] RTBEngine.lib not found in Debug or Release.
    )
)

:: 5. Copy Static Dependencies
echo Copying static dependencies...
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2main.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\glew-2.1.0\lib\Release\x64\glew32.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\assimp\lib\Release\x64\assimp-vc143-mt.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\fmod\api\core\lib\x64\fmod_vc.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\lua\lua54.lib" "%OUTPUT_DIR%\Lib\" >nul

:: Bullet Libs
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\LinearMath.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\BulletCollision.lib" "%OUTPUT_DIR%\Lib\" >nul
copy "%THIRD_PARTY_DIR%\bullet3-3.25\build_msvc\lib\Release\BulletDynamics.lib" "%OUTPUT_DIR%\Lib\" >nul

:: 6. Copy Runtime DLLs (Bin)
echo Copying DLLs to Bin directory...
copy "%THIRD_PARTY_DIR%\SDL2-2.32.10\lib\x64\SDL2.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\glew-2.1.0\bin\Release\x64\glew32.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\assimp\bin\x64\assimp-vc143-mt.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\fmod\api\core\lib\x64\fmod.dll" "%OUTPUT_DIR%\Bin\" >nul
copy "%THIRD_PARTY_DIR%\lua\lua54.dll" "%OUTPUT_DIR%\Bin\" >nul

echo.
echo ===========================================
echo SDK Generated at: %OUTPUT_DIR%
echo ===========================================
echo Usage Instructions:
echo 1. In your new project, add "%OUTPUT_DIR%\Include" to "Additional Include Directories".
echo 2. Add "%OUTPUT_DIR%\Lib" to "Additional Library Directories".
echo 3. Add "RTBEngine.lib" and other libs (.lib) to "Additional Dependencies".
echo 4. Copy content from "%OUTPUT_DIR%\Bin" next to your final executable.
echo ===========================================
pause
