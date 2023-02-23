@echo off
setlocal
set ERROR_MSG=

set SOURCE_DIR=.

set BUILD_DIR=..\build
set INSTALL_DIR=..\install
set COMPILER="Visual Studio 16 2019"
set ARCHITECTURE=x64

set VCPKG_TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake

if not exist %VCPKG_TOOLCHAIN_FILE% (
    set ERROR_MSG=Please set the VCPKG_DIR environment variable to your vcpkg install location
    goto usage
)

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
if not %3.==. (
    set COMPILER=%3
)
if not %4.==. (
    set ARCHITECTURE=%4
)

call :realpath %BUILD_DIR%
set BUILD_DIR=%RETVAL%

call :realpath %INSTALL_DIR%
set INSTALL_DIR=%RETVAL%

echo Build location = %BUILD_DIR%
echo Install location = %INSTALL_DIR%
echo Compiler = %COMPILER%
echo Architecture = %ARCHITECTURE%
choice /C:YN /M Continue?
if ERRORLEVEL == 2 goto :usage

:run
mkdir %BUILD_DIR%
cmake ^
    -S %SOURCE_DIR% ^
    -B %BUILD_DIR% ^
    -G %COMPILER% ^
    -A %ARCHITECTURE% ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN_FILE%
    
goto end

:usage
if not "%ERROR_MSG%" == "" (
    echo Error: %ERROR_MSG%
)
echo Usage: vcpkg-bootstrap.bat [build folder] [install folder] [compiler] [architecture]
echo Example: 
echo    vcpkg-bootstrap.bat ..\build ..\install "Visual Studio 16 2019" x64 

:end
endlocal
exit /B

:realpath
  set RETVAL=%~f1
  exit /B
 
