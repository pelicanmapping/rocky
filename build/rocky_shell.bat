set PATH=%CD%\..\install\bin;%PATH%
set BUILD_DIR=%CD%\..\build
set VCPKG_DEPS_DIR=%BUILD_DIR%\vcpkg_installed\x64-windows-release
set PATH=%VCPKG_DEPS_DIR%\bin;%PATH%
set PATH=%VCPKG_DEPS_DIR%\plugins;%PATH%
set PATH=%VCPKG_DEPS_DIR%\tools\proj;%PATH%
set GDAL_DATA=%VCPKG_DEPS_DIR%\share\gdal
set PROJ_DATA=%VCPKG_DEPS_DIR%\share\proj
