# Rocky
This is a 3D geospatial map & terrain engine for Vulkan.

## Build on Windows
We use `vcpkg` for dependencies. Start by bootstraping a Visual Studio build with a command like this:
```
cmake -S repo -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWIN32_USE_MP=ON -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=H:\devel\vcpkg\repo\scripts\buildsystems\vcpkg.cmake
```
..where `repo` is the location of your source code, `build` is your (out-of-source) build folder, and `install` is where you want to install the results.

Optional dependences (these are ON by default; disable by setting to `OFF`):

* GDAL - for GeoTIFF and other local geodata support: `-DBUILD_WITH_GDAL=ON`
* OpenSSL - for HTTPS network support: `-DBUILD_WITH_OPENSSL=ON`

