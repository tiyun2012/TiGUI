@echo off
setlocal EnableExtensions

rem ============================================================================
rem MyUI build script
rem ============================================================================
rem
rem This is the main developer entry point.
rem
rem Supported commands:
rem
rem     make.bat
rem     make.bat debug
rem     make.bat release
rem     make.bat clean
rem     make.bat rebuild
rem     make.bat rebuild release
rem     make.bat help
rem
rem The actual build is still handled by CMake + Visual Studio.
rem This script just gives us a stable, simple workflow.
rem ============================================================================

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"

rem Expected Visual Studio generator from your current environment.
set "GENERATOR=Visual Studio 18 2026"

set "ARCH=x64"

rem Default settings.
set "CONFIG=Debug"
set "ACTION=build"

rem ============================================================================
rem Parse first parameter
rem ============================================================================

if /I "%~1"=="debug"   set "CONFIG=Debug"
if /I "%~1"=="release" set "CONFIG=Release"
if /I "%~1"=="clean"   set "ACTION=clean"
if /I "%~1"=="rebuild" set "ACTION=rebuild"
if /I "%~1"=="help"    goto :help
if /I "%~1"=="/?"      goto :help

rem ============================================================================
rem Parse optional second parameter
rem
rem Example:
rem
rem     make.bat rebuild release
rem ============================================================================

if /I "%~2"=="debug"   set "CONFIG=Debug"
if /I "%~2"=="release" set "CONFIG=Release"

echo.
echo ============================================================
echo MyUI - Milestone 1 Extended
echo ============================================================
echo Root      : %ROOT%
echo Build     : %BUILD_DIR%
echo Generator : %GENERATOR%
echo Platform  : %ARCH%
echo Config    : %CONFIG%
echo Action    : %ACTION%
echo ============================================================
echo.

rem ============================================================================
rem Configure
rem
rem We configure only when the build directory has no CMake cache.
rem ============================================================================

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [CMAKE] Configuring project...

    cmake -S "%ROOT%" ^
          -B "%BUILD_DIR%" ^
          -G "%GENERATOR%" ^
          -A %ARCH%

    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configuration failed.
        exit /b 1
    )
)

rem ============================================================================
rem Clean
rem ============================================================================

if "%ACTION%"=="clean" (
    echo [BUILD] Cleaning %CONFIG%...

    cmake --build "%BUILD_DIR%" ^
          --config %CONFIG% ^
          --target clean

    if errorlevel 1 (
        echo.
        echo [ERROR] Clean failed.
        exit /b 1
    )

    echo.
    echo [DONE] Clean completed.
    exit /b 0
)

rem ============================================================================
rem Rebuild
rem ============================================================================

if "%ACTION%"=="rebuild" (
    echo [BUILD] Rebuilding %CONFIG%...

    cmake --build "%BUILD_DIR%" ^
          --config %CONFIG% ^
          --clean-first

    if errorlevel 1 (
        echo.
        echo [ERROR] Rebuild failed.
        exit /b 1
    )

    goto :done
)

rem ============================================================================
rem Normal build
rem ============================================================================

echo [BUILD] Building %CONFIG%...

cmake --build "%BUILD_DIR%" ^
      --config %CONFIG%

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed.
    exit /b 1
)

:done

echo.
echo ============================================================
echo Build completed successfully.
echo ============================================================
echo Executable:
echo %BUILD_DIR%\%CONFIG%\MyUI_Milestone1.exe
echo.
echo Log:
echo %ROOT%logs\MyUI.log
echo.

exit /b 0

:help

echo.
echo MyUI build commands
echo.
echo   make.bat                  Build Debug
echo   make.bat debug            Build Debug
echo   make.bat release          Build Release
echo   make.bat clean            Clean Debug
echo   make.bat rebuild          Clean and build Debug
echo   make.bat rebuild release  Clean and build Release
echo   make.bat help             Show this help
echo.

exit /b 0
