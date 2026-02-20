Getting Started with Rocky
==========================

This guide will help you build the Rocky SDK and run your first Rocky application.

Prerequisites
-------------

Before building Rocky, ensure you have the following installed:

**Required:**

* **CMake** 3.7 or newer
* **C++17** compatible compiler (Visual Studio 2019+, GCC, or Clang)
* **Vulkan SDK** 1.3.268 or newer

**Optional:**

* **ImGui** 1.92 or newer (for full dynamic font support)

Supported Platforms
-------------------

Rocky is actively maintained on:

* Windows (Visual Studio 2019+)
* Linux (GCC, Clang)
* macOS (experimental)

Building the SDK
----------------

Rocky uses CMake for building and vcpkg for dependency management.

Windows
~~~~~~~

Rocky comes with a convenient batch file that automates the build setup:

.. code-block:: batch

   bootstrap-vcpkg.bat

This script will:

1. Download and set up vcpkg
2. Download and build all dependencies (this may take a while)
3. Configure CMake
4. Generate your Visual Studio solution file

The generated solution will be in the ``build`` directory.

Linux
~~~~~

For Linux, use the shell script:

.. code-block:: bash

   ./bootstrap-vcpkg.sh

This will perform the same steps as the Windows batch file.

Alternative: Manual Dependency Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you prefer not to use vcpkg, you can manually install the dependencies using your system package manager (e.g., ``apt`` on Linux) or build them from source.

See the :doc:`../api/library_root` for the complete list of dependencies.

Environment Setup
-----------------

Rocky is generally good at finding its data files, but you may need to set some environment variables in certain situations.

Windows
~~~~~~~

.. code-block:: batch

   set ROCKY_FILE_PATH=%rocky_install_dir%/share/rocky
   set ROCKY_DEFAULT_FONT=C:/windows/fonts/arialbd.ttf
   set PROJ_DATA=%proj_install_dir%/share/proj

If you built with vcpkg, add the dependencies folder to your PATH:

.. code-block:: batch

   set PATH=%PATH%;%rocky_dir%/vcpkg_installed/x64-windows/bin

Linux
~~~~~

.. code-block:: bash

   export ROCKY_FILE_PATH=/path/to/rocky/share
   export ROCKY_DEFAULT_FONT=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf
   export PROJ_DATA=/usr/share/proj

Running the Demo
----------------

Once built, you can run the main demo application:

.. code-block:: bash

   rocky_demo

This will launch the Rocky demo with a basic map.

.. image:: https://user-images.githubusercontent.com/326618/236200807-73567789-a5a3-46d5-a98d-e9c1f24a0f62.png
   :alt: Rocky Demo Screenshot
   :align: center
   :width: 600px

Loading Map Data
~~~~~~~~~~~~~~~~

Rocky includes several example map configuration files in the ``data`` folder. You can load them using the ``--map`` option:

.. code-block:: bash

   rocky_demo --map data/openstreetmap.map.json

This will load OpenStreetMap data with proper attribution.

.. image:: https://github.com/user-attachments/assets/9590cca6-a170-4418-8588-1ee1d2b72924
   :alt: OpenStreetMap Example
   :align: center
   :width: 600px

Next Steps
----------

Now that you have Rocky built and running, check out:

* :doc:`../examples/hello_world` - Create your first Rocky application
* :doc:`maps_and_layers` - Learn about loading map data
* :doc:`ecs_system` - Understand the Entity Component System

Troubleshooting
---------------

Build Issues
~~~~~~~~~~~~

**Problem:** CMake cannot find dependencies

**Solution:** Ensure you're using the vcpkg toolchain file or that dependencies are installed in standard system locations.

**Problem:** Vulkan SDK not found

**Solution:** Download and install the Vulkan SDK from https://vulkan.lunarg.com/ and ensure it's in your PATH.

Runtime Issues
~~~~~~~~~~~~~~

**Problem:** Application fails to find data files

**Solution:** Set the ``ROCKY_FILE_PATH`` environment variable to point to the ``share/rocky`` directory in your installation.

**Problem:** Fonts not rendering correctly

**Solution:** Set ``ROCKY_DEFAULT_FONT`` to point to a valid TrueType font file on your system.

For More Help
~~~~~~~~~~~~~

* Check the `GitHub Issues <https://github.com/pelicanmapping/rocky/issues>`_
* Review the :doc:`../examples/hello_world` for a minimal working example
* Consult the :doc:`../api/library_root` for detailed API information
