Rocky SDK Documentation
=======================

Welcome to the Rocky C++ SDK documentation. Rocky is a modern C++ SDK for rendering maps and globes with real geospatial imagery and elevation data.

.. note::
   This project is in its early stages. Expect API and architectural changes before version 1.0.

About Rocky
-----------

Rocky is a C++ toolkit for:

* Rendering interactive 3D maps and globes
* Loading geospatial imagery and elevation data from multiple sources
* Working with real-world coordinate systems and projections
* Building location-based applications with Vulkan SceneGraph

**Key Features:**

* 🗺️ Multiple data source support (TMS, MBTiles, GDAL, Azure, Bing)
* 🌍 Full coordinate system transformation support
* 🎮 Entity Component System (ECS) for annotations
* 🎨 Vulkan-based rendering with VulkanSceneGraph (VSG)
* ⚡ High-performance terrain rendering
* 🔧 Modular architecture with optional components

Quick Links
-----------

* `GitHub Repository <https://github.com/pelicanmapping/rocky>`_
* :doc:`guides/getting_started` - Build and run your first Rocky application
* :doc:`examples/hello_world` - Minimal code example
* :doc:`api/library_root` - Complete API reference

Getting Started
---------------

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   guides/getting_started
   guides/maps_and_layers
   guides/ecs_system

.. toctree::
   :maxdepth: 2
   :caption: Examples

   examples/hello_world

.. toctree::
   :maxdepth: 3
   :caption: API Reference

   api/library_root

Module Overview
---------------

Core Modules
~~~~~~~~~~~~

**rocky** - Core map and layer functionality

* :cpp:class:`rocky::Map` - Main data model holding layers
* :cpp:class:`rocky::Layer` - Base class for all layer types
* :cpp:class:`rocky::ImageLayer` - Image layer base class
* :cpp:class:`rocky::ElevationLayer` - Elevation/terrain data
* :cpp:class:`rocky::SRS` - Spatial reference systems
* :cpp:class:`rocky::GeoPoint` - Geographic point representation

**rocky::ecs** - Entity Component System for annotations

* :cpp:class:`rocky::ecs::Component` - Base component template
* :cpp:class:`rocky::ecs::Registry` - Entity registry/manager
* :cpp:class:`rocky::ecs::Transform` - Transform component
* :cpp:class:`rocky::ecs::Mesh` - 3D mesh component
* :cpp:class:`rocky::ecs::Label` - Text label component

**rocky::vsg** - Vulkan SceneGraph rendering backend

* :cpp:class:`rocky::vsg::Application` - Main application class
* :cpp:class:`rocky::vsg::MapNode` - Scene graph node for rendering maps
* :cpp:class:`rocky::vsg::TerrainNode` - Terrain rendering
* :cpp:class:`rocky::vsg::DisplayManager` - Multi-window management

Data Sources
~~~~~~~~~~~~

Rocky supports multiple geospatial data sources:

* **TMS** - Tile Map Service (TMSImageLayer, TMSElevationLayer)
* **MBTiles** - SQLite-based tile database (MBTilesImageLayer, MBTilesElevationLayer)
* **GDAL** - Geospatial Data Abstraction Library (GDALImageLayer, GDALElevationLayer)
* **Azure Maps** - Microsoft Azure Maps service (AzureImageLayer)
* **Bing Maps** - Microsoft Bing Maps (BingImageLayer, BingElevationLayer)

Requirements
------------

* **CMake** 3.7 or newer
* **C++17** compatible compiler
* **Vulkan SDK** 1.3.268 or newer
* **vcpkg** for dependency management

Supported Platforms
-------------------

* Windows (Visual Studio 2019+)
* Linux (GCC, Clang)
* macOS (experimental)

Building Rocky
--------------

Windows
~~~~~~~

.. code-block:: batch

   bootstrap-vcpkg.bat

Linux
~~~~~

.. code-block:: bash

   ./bootstrap-vcpkg.sh

This will download and build all dependencies using vcpkg and generate the build files.

See :doc:`guides/getting_started` for detailed instructions.

License
-------

Rocky is licensed under the MIT License.

.. code-block:: text

   Copyright 2023 Pelican Mapping

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

Indices and Tables
==================

* :ref:`genindex`
* :ref:`search`
