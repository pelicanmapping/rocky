# Rocky
This is a 3D geospatial map & terrain engine for Vulkan.

## Build on Windows
We use `vcpkg` for dependencies. Start by bootstraping a Visual Studio build with a command like this:
```
cmake -S repo -B build -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWIN32_USE_MP=ON -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=H:\devel\vcpkg\repo\scripts\buildsystems\vcpkg.cmake
```
..where `repo` is the location of your source code, `build` is your (out-of-source) build folder, and `install` is where you want to install the results.
