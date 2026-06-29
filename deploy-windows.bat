@echo off
setlocal enabledelayedexpansion

REM Windows deployment script for Scriptura
REM This script builds the project and bundles all Qt DLLs using windeployqt

set BUILD_DIR=build
set DEPLOY_DIR=deploy

echo === Scriptura Windows Deployment ===
echo.

REM Check if Qt is installed
where windeployqt >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: windeployqt not found in PATH
    echo Please ensure Qt is installed and windeployqt is in your PATH
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    cmake -B "%BUILD_DIR%" -S . -DCMAKE_BUILD_TYPE=Release
)

REM Build the project
echo.
echo Building project...
cmake --build "%BUILD_DIR%" --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

REM Create deployment directory
echo.
echo Creating deployment directory...
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

REM Copy the executable
echo Copying executable...
copy /Y "%BUILD_DIR%\scriptura.exe" "%DEPLOY_DIR%\"

REM Run windeployqt to bundle Qt DLLs
echo.
echo Deploying Qt dependencies...
windeployqt --release --dir "%DEPLOY_DIR%" "%BUILD_DIR%\scriptura.exe"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: windeployqt failed
    pause
    exit /b 1
)

REM Copy additional resources
echo.
echo Copying additional resources...
if exist "icon.png" copy /Y "icon.png" "%DEPLOY_DIR%\"
if exist "scriptura.desktop" copy /Y "scriptura.desktop" "%DEPLOY_DIR%\"
if exist "README.md" copy /Y "README.md" "%DEPLOY_DIR%\"

REM Create a ZIP archive
echo.
echo Creating ZIP archive...
powershell -Command "Compress-Archive -Path '%DEPLOY_DIR%\*' -DestinationPath 'scriptura-windows.zip' -Force"

echo.
echo === Deployment Complete ===
echo Executable and dependencies are in: %DEPLOY_DIR%\
echo Distribution archive: scriptura-windows.zip
echo.
pause