Entity Component System
=======================

.. note::
   This guide is under development. For now, please refer to the :doc:`../api/library_root` for detailed API information.

Overview
--------

Rocky uses an Entity Component System (ECS) architecture for creating map annotations and dynamic objects. The ECS is based on the `EnTT <https://github.com/skypjack/entt>`_ library.

Key Concepts
------------

Entities
~~~~~~~~

Entities are lightweight identifiers (handles) that represent objects in your scene. They have no data themselves, but serve as containers for components.

Components
~~~~~~~~~~

Components are data structures that define specific aspects of an entity:

* :cpp:class:`rocky::ecs::Transform` - Position, orientation, scale
* :cpp:class:`rocky::ecs::Mesh` - 3D geometry
* :cpp:class:`rocky::ecs::Line` - Line geometry
* :cpp:class:`rocky::ecs::Point` - Point features
* :cpp:class:`rocky::ecs::Label` - Text labels

Registry
~~~~~~~~

The :cpp:class:`rocky::ecs::Registry` manages all entities and their components.

Basic Usage
-----------

Creating an Entity
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include <rocky/ecs/Registry.h>
   using namespace rocky::ecs;

   Registry registry;
   auto entity = registry.create();

Adding Components
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   auto& transform = registry.emplace<Transform>(entity);
   transform.position = GeoPoint(SRS::WGS84, -122.419, 37.775, 0.0);

   auto& label = registry.emplace<Label>(entity);
   label.text = "Hello, World!";

See Also
--------

* :cpp:class:`rocky::ecs::Component` - Base component template
* :cpp:class:`rocky::ecs::Registry` - Entity registry
* :cpp:namespace:`rocky::ecs` - ECS namespace documentation
