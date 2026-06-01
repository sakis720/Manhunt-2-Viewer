@echo off
setlocal
echo === MapViewer Build Script ===

:: Ensure submodules are initialized and updated
if exist .git (
    echo [INFO] Updating submodules...
    git submodule update --init --recursive
)

:: Clean previous build to avoid cached compiler issues
if exist build (
    echo [INFO] Cleaning previous build...
    rmdir /s /q build
)

mkdir build
cd build

echo Configuring with CMake...
:: CMake will auto-detect MSVC on standard Windows environments
cmake ..
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

echo Building Project (Release)...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed.
    pause
    exit /b %ERRORLEVEL%
)

cd ..
echo.
echo === Build Successful ===
if exist "build\Release\MapViewer.exe" (
    echo Executable located at: build\Release\MapViewer.exe
) else if exist "build\MapViewer.exe" (
    echo Executable located at: build\MapViewer.exe
)
echo.
pause
