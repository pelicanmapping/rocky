@echo on
setlocal

cmake -S "%SRC_DIR%" -B build -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
    -DCMAKE_PREFIX_PATH="%LIBRARY_PREFIX%" ^
    -DROCKY_SUPPORTS_IMGUI=OFF ^
    -DROCKY_SUPPORTS_QT=OFF ^
    -DROCKY_SUPPORTS_SLUGHORN=OFF
if errorlevel 1 exit /b 1

cmake --build build --parallel %CPU_COUNT%
if errorlevel 1 exit /b 1

cmake --install build
