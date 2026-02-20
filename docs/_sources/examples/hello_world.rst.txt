Hello World
===========

This example demonstrates the minimal code needed to create a Rocky application that displays a map with imagery.

Overview
--------

The easiest way to write a turnkey Rocky application is to use the :cpp:class:`rocky::Application` class. It will create a viewer, a default map, and a scene graph to store everything you want to visualize.

Complete Example
----------------

main.cpp
~~~~~~~~

.. code-block:: cpp

   #include <rocky/rocky.h>

   int main(int argc, char** argv)
   {
       rocky::Application app(argc, argv);

       auto imagery = rocky::TMSImageLayer::create();
       imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
       app.mapNode->map->add(imagery);

       return app.run();
   }

This simple application:

1. Creates a :cpp:class:`rocky::Application` instance
2. Creates a :cpp:class:`rocky::TMSImageLayer` for imagery
3. Sets the imagery URI to a public tile service
4. Adds the layer to the map
5. Runs the application event loop

CMakeLists.txt
~~~~~~~~~~~~~~

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.10)
   project(myApp VERSION 0.1.0 LANGUAGES CXX C)

   find_package(rocky CONFIG REQUIRED)

   add_executable(myApp main.cpp)
   target_link_libraries(myApp PRIVATE rocky::rocky)
   install(TARGETS myApp RUNTIME DESTINATION bin)

Building the Example
---------------------

1. Create a new directory for your project
2. Save the files above as ``main.cpp`` and ``CMakeLists.txt``
3. Configure and build:

   .. code-block:: bash

      mkdir build
      cd build
      cmake ..
      cmake --build .

4. Run the application:

   .. code-block:: bash

      ./myApp

Understanding the Code
-----------------------

rocky::Application
~~~~~~~~~~~~~~~~~~

The :cpp:class:`rocky::Application` class is a high-level convenience class that sets up:

* A rendering window with Vulkan/VSG backend
* A :cpp:class:`rocky::MapNode` for rendering the map
* A default :cpp:class:`rocky::Map` data model
* Event handling and camera manipulation
* The main run loop

It accepts command-line arguments, which can be used to configure the application behavior.

TMSImageLayer
~~~~~~~~~~~~~

The :cpp:class:`rocky::TMSImageLayer` class loads tile-based imagery from the network using the `TMS (Tile Map Service) <https://wiki.osgeo.org/wiki/Tile_Map_Service_Specification>`_ specification.

Key properties:

* ``uri`` - The URI pattern for accessing tiles
* ``attribution`` - Copyright/attribution information
* ``profile`` - The tiling profile (auto-detected when possible)

Adding Layers to the Map
~~~~~~~~~~~~~~~~~~~~~~~~~

Layers are added to the map using:

.. code-block:: cpp

   app.mapNode->map->add(imagery);

The map maintains an ordered list of layers. Image layers are composited in the order they are added.

Next Steps
----------

Now that you have a basic application running, you can:

* Add elevation data with :cpp:class:`rocky::ElevationLayer`
* Load local GeoTIFF files with :cpp:class:`rocky::GDALImageLayer`
* Add vector features with :cpp:class:`rocky::ecs::EntityCollectionLayer`
* Customize the viewpoint with :cpp:class:`rocky::Viewpoint`

See Also
--------

* :doc:`../guides/maps_and_layers` - Comprehensive guide to map layers
* :doc:`../guides/ecs_system` - Entity Component System for annotations
* :cpp:class:`rocky::Application` - Full API documentation
* :cpp:class:`rocky::Map` - Map data model API
