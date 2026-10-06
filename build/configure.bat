@echo off
setlocal

REM Run from the repository root. BUILD_DIR selects an existing bootstrap build or a new build directory.
if not defined BUILD_DIR set "BUILD_DIR=..\build"

REM Respect the bootstrap variable, the standard vcpkg variable, then the existing CMake configuration.
if defined VCPKG_DIR goto toolchain_from_directory
if defined VCPKG_ROOT set "VCPKG_DIR=%VCPKG_ROOT%"
if defined VCPKG_DIR goto toolchain_from_directory

if exist "%BUILD_DIR%\CMakeCache.txt" (
    for /f "tokens=1,* delims==" %%A in ('findstr /B "CMAKE_TOOLCHAIN_FILE:" "%BUILD_DIR%\CMakeCache.txt"') do (
        set "ROCKY_TOOLCHAIN_FILE=%%B"
    )
)
if defined ROCKY_TOOLCHAIN_FILE if exist "%ROCKY_TOOLCHAIN_FILE%" goto configure

REM Last, look for an installation on PATH. Never assume vcpkg is on the worktree's drive.
for /f "delims=" %%V in ('where vcpkg.exe 2^>nul') do if not defined VCPKG_DIR set "VCPKG_DIR=%%~dpV"
if not defined VCPKG_DIR goto missing_toolchain

:toolchain_from_directory
set "ROCKY_TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"
if not exist "%ROCKY_TOOLCHAIN_FILE%" goto missing_toolchain

:configure
REM Reconfiguration preserves generator, install prefix, triplet, and dependency paths from bootstrap.
REM Additional CMake options can be passed through, e.g. -DVCPKG_MANIFEST_INSTALL=OFF.
if exist "%BUILD_DIR%\CMakeCache.txt" goto reconfigure
if not defined INSTALL_DIR set "INSTALL_DIR=..\install"
cmake -S . -B "%BUILD_DIR%" -A x64 ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%ROCKY_TOOLCHAIN_FILE%" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows-release %*
exit /b %ERRORLEVEL%

:reconfigure
cmake -S . -B "%BUILD_DIR%" -DCMAKE_TOOLCHAIN_FILE="%ROCKY_TOOLCHAIN_FILE%" %*
exit /b %ERRORLEVEL%

:missing_toolchain
echo Cannot find vcpkg. Set VCPKG_DIR or VCPKG_ROOT to its installation directory. 1>&2
exit /b 1
