Maps and Layers
===============

.. note::
   This guide is under development. For now, please refer to the :doc:`../api/library_root` for detailed API information.

Overview
--------

Rocky's data model revolves around the :cpp:class:`rocky::Map` class, which contains an ordered collection of :cpp:class:`rocky::Layer` objects.

Map Class
---------

The :cpp:class:`rocky::Map` holds all your map data layers and provides:

* Layer management (add, remove, reorder)
* Coordinate system definition (SRS)
* Callback notifications for changes

Layer Types
-----------

Image Layers
~~~~~~~~~~~~

Image layers display visible map colors:

* :cpp:class:`rocky::TMSImageLayer` - Tile Map Service imagery
* :cpp:class:`rocky::MBTilesImageLayer` - MBTiles database imagery
* :cpp:class:`rocky::GDALImageLayer` - Local GeoTIFF/GDAL imagery
* :cpp:class:`rocky::AzureImageLayer` - Microsoft Azure Maps
* :cpp:class:`rocky::BingImageLayer` - Microsoft Bing Maps

Elevation Layers
~~~~~~~~~~~~~~~~

Elevation layers provide terrain height data:

* :cpp:class:`rocky::TMSElevationLayer` - Tile Map Service elevation
* :cpp:class:`rocky::MBTilesElevationLayer` - MBTiles database elevation
* :cpp:class:`rocky::GDALElevationLayer` - Local GeoTIFF/GDAL elevation
* :cpp:class:`rocky::BingElevationLayer` - Microsoft Bing elevation

See Also
--------

* :cpp:class:`rocky::Map` - Map API documentation
* :cpp:class:`rocky::Layer` - Layer base class
* :doc:`../examples/hello_world` - Simple layer example
