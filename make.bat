@echo off
setlocal EnableExtensions

rem ============================================================================
rem MyUI build entry point
rem ============================================================================
rem
rem This is the single command-line entry point for the project. It keeps the
rem developer workflow simple now and gives us a place to add build parameters
rem as later milestones introduce more targets/options.
rem
rem Supported commands:
rem     make.bat
rem     make.bat debug
rem     make.bat release
rem     make.bat clean
rem     make.bat rebuild
rem     make.bat rebuild release
rem     make.bat help
rem
rem Default:
rem     Debug build
rem
rem Expected toolchain:
rem     Visual Studio 2026
rem     CMake
rem     x64
rem ============================================================================

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"
set "GENERATOR=Visual Studio 18 2026"
set "ARCH=x64"
set "CONFIG=Debug"
set "ACTION=build"

rem Parse the first argument.
if /I "%~1"=="debug" set "CONFIG=Debug"
if /I "%~1"=="release" set "CONFIG=Release"
if /I "%~1"=="clean" set "ACTION=clean"
if /I "%~1"=="rebuild" set "ACTION=rebuild"
if /I "%~1"=="help" goto :help
if /I "%~1"=="/?" goto :help

rem Parse an optional second argument, allowing commands such as:
rem     make.bat rebuild release
if /I "%~2"=="debug" set "CONFIG=Debug"
if /I "%~2"=="release" set "CONFIG=Release"

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [CMAKE] Configuring MyUI...
    cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" -A %ARCH%
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed.
        exit /b 1
    )
)

if "%ACTION%"=="clean" goto :clean
if "%ACTION%"=="rebuild" goto :rebuild

echo [BUILD] Configuration: %CONFIG%
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)
goto :done

:clean
echo [BUILD] Cleaning configuration: %CONFIG%
cmake --build "%BUILD_DIR%" --config %CONFIG% --target clean
if errorlevel 1 (
    echo [ERROR] Clean failed.
    exit /b 1
)
goto :done

:rebuild
echo [BUILD] Rebuilding configuration: %CONFIG%
cmake --build "%BUILD_DIR%" --config %CONFIG% --clean-first
if errorlevel 1 (
    echo [ERROR] Rebuild failed.
    exit /b 1
)
goto :done

:done
echo.
echo [DONE] %ACTION% %CONFIG% completed.
echo [EXE]  %BUILD_DIR%\%CONFIG%\MyUI_Milestone1.exe
echo.
exit /b 0

:help
echo.
echo MyUI Milestone 1 build commands:
echo.
echo   make.bat                  Build Debug
necho   make.bat debug            Build Debug
necho   make.bat release          Build Release
necho   make.bat clean            Clean Debug
necho   make.bat rebuild          Clean and build Debug
necho   make.bat rebuild release  Clean and build Release
necho   make.bat help             Show this help
exit /b 0
