@echo off
setlocal
set ERROR_MSG=

:: Try to find VCPKG_DIR if it's not set
if "%VCPKG_DIR%" == "" (
    FOR /F "tokens=*" %%X IN ('where vcpkg.exe') do (SET VCPKG_DIR=%%~dpX)
)

:: Verify vcpkg is available
set VCPKG_TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake

if not exist %VCPKG_TOOLCHAIN_FILE% (
    set ERROR_MSG=Please set the VCPKG_DIR environment variable to your vcpkg install location
    goto :usage
)

:: Verify that the Vulkan SDK is installed
if not defined VULKAN_SDK (
    echo Please install the Vulkan SDK and set the VULKAN_SDK enviroment variable to its location:
    echo https://sdk.lunarg.com/sdk/download/1.3.261.1/windows/VulkanSDK-1.3.261.1-Installer.exe
    goto :end
)

:: Detect visual studio version
set VS_VERSION=Visual Studio 17 2022
reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.14" >> nul 2>&1
if %ERRORLEVEL% EQU 0 ( set VS_VERSION=Visual Studio 14 2015 )
reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.15" >> nul 2>&1
if %ERRORLEVEL% EQU 0 ( set VS_VERSION=Visual Studio 15 2017 )
reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.16" >> nul 2>&1
if %ERRORLEVEL% EQU 0 ( set VS_VERSION=Visual Studio 16 2019 )
reg query "HKEY_CLASSES_ROOT\VisualStudio.DTE.17" >> nul 2>&1
if %ERRORLEVEL% EQU 0 ( set VS_VERSION=Visual Studio 17 2022 )

:: Argument parser from: https://stackoverflow.com/a/8162578
setlocal enableDelayedExpansion
set "options=-S:. -B:..\build -I:..\install -G:"Visual Studio 17 2022" -A:x64"
for %%O in (%options%) do for /f "tokens=1,* delims=:" %%A in ("%%O") do set "%%A=%%~B"
:loopArgs
    if not "%~1"=="" (
      set "test=!options:*%~1:=! "
      if "!test!"=="!options! " (
          echo Error: Invalid option %~1
      ) else if "!test:~0,1!"==" " (
          set "%~1=1"
      ) else (
          setlocal disableDelayedExpansion
          set "val=%~2"
          call :escapeVal
          setlocal enableDelayedExpansion
          for /f delims^=^ eol^= %%A in ("!val!") do endlocal&endlocal&set "%~1=%%A" !
          shift /1
      )
      shift /1
      goto :loopArgs
    )
    goto :endArgs
:escapeVal
    set "val=%val:^=^^%"
    set "val=%val:!=^!%"
    exit /b
:endArgs

set COMPILER=!-G!
set ARCHITECTURE=!-A!

:: Expand to absolute paths
call :realpath !-S!
set SOURCE_DIR=%RETVAL%
call :realpath !-B!
set BUILD_DIR=%RETVAL%
call :realpath !-I!
set INSTALL_DIR=%RETVAL%

:: Ask for confirmation:
echo VCPKG_DIR        = %VCPKG_DIR%
echo Source location  = %SOURCE_DIR%
echo Build location   = %BUILD_DIR%
echo Install location = %INSTALL_DIR%
echo Compiler         = %COMPILER%
echo Architecture     = %ARCHITECTURE%
choice /C:YN /M Continue?
if ERRORLEVEL == 2 goto :usage

set MANIFEST_DIR="%SOURCE_DIR%\vcpkg"

if not exist "%MANIFEST_DIR%\vcpkg.json" (
    set ERROR_MSG=No vcpkg.json manifest found. Run this script from the root folder of the git repository
    goto usage
)

:: Run CMAKE
mkdir %BUILD_DIR%

if exist %BUILD_DIR%\CMakeCache.txt (
    del %BUILD_DIR%\CMakeCache.txt
)

cmake ^
    -S "%SOURCE_DIR%" ^
    -B "%BUILD_DIR%" ^
    -G "%COMPILER%" ^
    -A %ARCHITECTURE% ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN_FILE% ^
    -DVCPKG_MANIFEST_DIR="%MANIFEST_DIR%"

goto end

:usage
  if not "%ERROR_MSG%" == "" (
      echo Error: %ERROR_MSG%
  )
  echo Usage: vcpkg-bootstrap.bat -S source_folder -B build_folder -I install_folder -G compiler -A architecture
  echo Example: 
  echo    vcpkg-bootstrap.bat -S . -B ..\build -I ..\install -G "Visual Studio 17 2022" -A x64 

:end
  endlocal
  exit /B


:: Converts a relative path to an absolute path
:realpath
  set RETVAL=%~f1
  exit /B 
