@echo off
setlocal
set ERROR_MSG=

set SOURCE_DIR=.
set BUILD_DIR=..\build
set INSTALL_DIR=..\install
set VCPKG_DIR=H:\devel\vcpkg\repo
set VCPKG_TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake

if not exist "vcpkg.json" (
    set ERROR_MSG=Run this script from the root folder of the git repository
    goto usage
)

if not %1.==. (
    set BUILD_DIR=%1
)

if not %2.==. (
    set INSTALL_DIR=%2
)

if not exist %VCPKG_TOOLCHAIN_FILE% (
    set ERROR_MSG=No VCPKG toolchain file at "%VCPKG_TOOLCHAIN_FILE%"
    goto usage
)

:run
mkdir %BUILD_DIR%
cmake ^
    -S %SOURCE_DIR% ^
    -B %BUILD_DIR% ^
    -G "Visual Studio 16 2019" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN_FILE%
    
goto end

:usage
echo Usage: vcpkg-bootstrap.bat [build folder] [install folder]
if not "%ERROR_MSG%" == "" (
    echo Error: %ERROR_MSG%
)

:end
endlocal
