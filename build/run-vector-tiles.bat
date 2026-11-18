@echo off
setlocal
REM Use this build's existing dependency installation and runtime data.
if not defined BUILD_DIR set "BUILD_DIR=%~dp0..\..\build"
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Set BUILD_DIR to the configured Rocky build directory. 1>&2
    exit /b 1
)
for /f "tokens=1,* delims==" %%A in ('findstr /B "VCPKG_INSTALLED_DIR:" "%BUILD_DIR%\CMakeCache.txt"') do set "VECTOR_DEPS_ROOT=%%B"
for /f "tokens=1,* delims==" %%A in ('findstr /B "VCPKG_TARGET_TRIPLET:" "%BUILD_DIR%\CMakeCache.txt"') do set "VECTOR_TRIPLET=%%B"
if not defined VECTOR_DEPS_ROOT set "VECTOR_DEPS_ROOT=%BUILD_DIR%\vcpkg_installed"
if not defined VECTOR_TRIPLET set "VECTOR_TRIPLET=x64-windows-release"
set "VECTOR_DEPS=%VECTOR_DEPS_ROOT%\%VECTOR_TRIPLET%"
set "PATH=%BUILD_DIR%\src\rocky\RelWithDebInfo;%VECTOR_DEPS%\bin;%PATH%"
set "PROJ_DATA=%VECTOR_DEPS%\share\proj"
set "GDAL_DATA=%VECTOR_DEPS%\share\gdal"
set "ROCKY_FILE_PATH=%~dp0..\data;%~dp0..\src\rocky\vsg"
"%BUILD_DIR%\src\apps\rocky_demo_vector_tiles\RelWithDebInfo\rocky_demo_vector_tiles.exe" %*
exit /b %ERRORLEVEL%
