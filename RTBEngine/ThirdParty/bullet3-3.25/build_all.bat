@echo off
setlocal enabledelayedexpansion

echo ===== Building Bullet Physics 3.25 - Debug and Release =====
echo.

REM Set paths
set BULLET_ROOT=c:\Users\pablo\Desktop\Proyectos\RTBEngine\RTBEngine\RTBEngine\ThirdParty\bullet3-3.25
set BUILD_DIR=%BULLET_ROOT%\build_msvc
set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

REM Clean and create build directory
echo [1/5] Creating clean build directory...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Setup VS environment
echo [2/5] Setting up Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Generate project files with CMake
echo [3/5] Generating Visual Studio project files...
cmake "%BULLET_ROOT%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_BULLET2_DEMOS=OFF -DBUILD_CPU_DEMOS=OFF -DBUILD_UNIT_TESTS=OFF -DBUILD_EXTRAS=OFF
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake generation failed!
    pause
    exit /b 1
)

REM Build Debug configuration
echo [4/5] Building Debug configuration...
%MSBUILD% BULLET_PHYSICS.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Debug build failed!
    pause
    exit /b 1
)

REM Build Release configuration
echo [5/5] Building Release configuration...
%MSBUILD% BULLET_PHYSICS.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Release build failed!
    pause
    exit /b 1
)

echo.
echo ===== Build Complete! =====
echo Debug libs: %BUILD_DIR%\lib\Debug
echo Release libs: %BUILD_DIR%\lib\Release
echo.
pause
