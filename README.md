# :mountain: rocky

This is a 3D geospatial map & terrain renderer based on Vulkan.

This project is in its very early stages. It is based on the data model in the osgEarth SDK, a 3D GIS toolkit created in 2008 and still in wide use today.

## Building the Rocky SDK on Windows

### 1. Get vcpkg
Start by cloning `vcpkg` to your location of choice:
```
git clone https://github.com/microsoft/vcpkg.git vcpkg
```
After cloning `vcpkg` you will need to run the `bootstrap-vcpkg` script from the repository folder.

Make sure that the `vcpkg` executable (also in the repo folder) is in your path.

### 2. Configure CMake to use the vcpkg toolchain
Next, in your local repository folder, there is a batch file `bootstrap-vcpkg.bat` that will set up the project to use `vcpkg`. You may wish to edit the file before running it to tell it where to find `vcpkg`, `gdal` if necessary, and to customize your folder setup. By default, it will create:

* The out-of-source build folder at `..\build`
* The installation folder at `..\install`

Optional dependences (these are `ON` by default; disable by setting to `OFF` either in CMAKE-GUI or on the command line):

* HTTP - for reading data over HTTP: `-DROCKY_SUPPORTS_HTTP=ON`
* HTTPS - adds SSL support to HTTP: `-DROCKY_SUPPORTS_HTTPS=ON`
* TMS - read from an OSGGeo [TileMapService](https://wiki.osgeo.org/wiki/Tile_Map_Service_Specification) endpoint: `-DROCKY_SUPPORTS_TMS=ON`
* GDAL - read from GeoTIFFs, JP2Ks, WMS, WMTS, and other [GDAL-supported data formats](https://gdal.org/drivers/raster/index.html): `-DROCKY_SUPPORTS_GDAL=ON`

The first bootstrap will take a long time; it needs to download and build all the dependencies.

### 3. Build it
Open the Visual Studio solution found in your `build` folder and build the `INSTALL` target. Or from the command line, you can build with
```
cmake --build ..\build --target INSTALL --config RelWithDebInfo
```
The resulting files will be installed to the `install` folder you specified during bootstrap.

### 4. Run the sample apps
You will need the dependencies in your path. If you are using the vcpkg toolchain, the dependencies will be in `vcpkg_installed/x64-windows/bin` relative to your build location.

You will also need some environment variables:
```
# PROJ database location:
set PROJ_DATA=%path_to_vcpkg_deps%/share/proj

# Shaders location:
set VSG_FILE_PATH=%path_to_install_dir%/share
```

Now hopefully you will be able to try the viewer or the unit tester:
```
rviewer.exe
rtests.exe
```
Enjoy!
