:: Get vcpkg
:: git clone --branch 2026.02.27 git@github.com:microsoft/vcpkg.git
set VCPKG_DIR=\vcpkg

REM Configure the project
cmake -S . ^
      -B ../build ^
      -A x64 ^
      -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_INSTALL_PREFIX=../install ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x64-windows-release
