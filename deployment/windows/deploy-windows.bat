@echo off
setlocal enabledelayedexpansion

REM Windows deployment script for Scriptura
REM This script builds the application and deploys it with all Qt dependencies
REM Usage: deploy-windows.bat [Debug^|Release]

set BUILD_TYPE=%~1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=cmake-build-%BUILD_TYPE%
set DEPLOY_DIR=deploy\%BUILD_TYPE%

echo === Deploying Scriptura (%BUILD_TYPE%) ===
echo.

REM Convert icon to ICO format if needed
if exist "icon.png" (
    echo Converting icon to ICO format...
    powershell -ExecutionPolicy Bypass -File "%~dp0convert-icon.ps1" -InputFile "icon.png" -OutputFile "icon.ico"
)

REM Check if CMake is available
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake and add it to your PATH
    pause
    exit /b 1
)

REM Check if windeployqt is available
where windeployqt >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: windeployqt not found in PATH
    echo Please install Qt and add it to your PATH
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" (
    echo Creating build directory: %BUILD_DIR%
    cmake -B "%BUILD_DIR%" -S . -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
)

REM Build the project
echo Building project...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% -j

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

REM Create deploy directory
echo.
echo Creating deploy directory: %DEPLOY_DIR%
if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%"

REM Copy the executable
echo Copying executable...
copy /Y "%BUILD_DIR%\scriptura.exe" "%DEPLOY_DIR%\"

REM Deploy Qt dependencies using windeployqt
echo.
echo Deploying Qt dependencies with windeployqt...
windeployqt --%BUILD_TYPE% --no-translations --no-system-d3d-compiler --no-opengl-sw "%DEPLOY_DIR%\scriptura.exe"

REM Copy additional resources
echo.
echo Copying additional resources...


REM Copy any data files that might be needed
if exist "resources" (
    echo Copying resources...
    xcopy /E /I /Y "resources" "%DEPLOY_DIR%\resources"
)

REM Copy icon if available
if exist "icon.png" (
    copy /Y "icon.png" "%DEPLOY_DIR%\"
)

echo.
echo === Deployment complete ===
echo Output directory: %DEPLOY_DIR%
echo.
echo To test the deployment, run: %DEPLOY_DIR%\scriptura.exe
echo.
pause
