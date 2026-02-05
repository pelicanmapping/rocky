<h1>:mountain: Rocky</h1>

Rocky is a C++ SDK for rendering maps and globes. <img align="right" width="200" alt="Screenshot 2023-02-22 124318" src="https://user-images.githubusercontent.com/326618/220712284-8a17d87a-431f-4966-a425-0f2628b23b40.png">

Rocky will render an accurate 3D or 2D map with real geospatial imagery and elevation data. It supports thousands of map projections and many popular geodata sources including GeoTIFF, TMS, OpenStreetMap, WMTS, WMS, and Azure Maps. Rocky's data model is inspired by the osgEarth SDK, a 3D GIS toolkit created in 2008 and still in wide use today.

**This project is in its early stages so expect a lot of API and architectural changes before version 1.0.**

![Windows](https://github.com/pelicanmapping/rocky/actions/workflows/ci-windows-minimal.yml/badge.svg)
![Documentation](https://github.com/pelicanmapping/rocky/actions/workflows/mkdocs-gh-pages.yml/badge.svg)

<!-- TOC start (generated with https://github.com/derlin/bitdowntoc) -->

- [Setup](#getting-started)
   * [Build the SDK](#build-the-sdk)
   * [Run the Demo](#run-the-demo)
- [Hello World](#hello-world)
   * [main.cpp](#maincpp)
   * [CMakeLists.txt](#cmakeliststxt)
- [Maps](#maps)
   * [Imagery](#imagery)
   * [Terrain Elevation](#terrain-elevation)
   * [Vector Features](#vector-features)
   * [Spatial Reference Systems](#spatial-reference-systems)
- [Annotations](#annotations)
   * [Creating entities and components](#creating-entities-and-components)
   * [The entity registry](#the-registry)
   * [Control components](#control-components)
- [Integrations](#integrations)
   * [Rocky and ImGui](#rocky-and-dear-imgui)
   * [Rocky and VulkanSceneGraph](#rocky-and-vulkanscenegraph)
   * [Rocky and Qt](#rocky-and-qt)
- [Acknowledgements](#acknowledgements)

<!-- TOC end -->

<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/3767d7e8-364c-498f-a09f-2b0e17b45c0a">


<br/><br/>
# Setup

## Build the SDK
Rocky uses CMake, and we maintain the build on Windows and Linux.
Rocky comes with a handy Windows batch file to automatically configure the project using `vcpkg`:
```bat
bootstrap-vcpkg.bat
```
That will download and build all the dependencies (takes a while) and generate your CMake project and Visual Studio solution file.

If you would rather not use vcpkg, you can build and install the [dependencies](#acknowledgements) yourself, or use your favorite package manager (like `apt` on Linux).

> *Note: Rocky requires Vulkan SDK 1.3.268 or newer.*

> *Note: Rocky requires ImGui version 1.92 or newer to get full dynamic font support.*


## Run the Demo
Rocky is pretty good at finding its data files, but if you run into trouble, you might need to set a couple environment variables to help:
```bat
set ROCKY_FILE_PATH=%rocky_install_dir%/share/rocky
set ROCKY_DEFAULT_FONT=C:/windows/fonts/arialbd.ttf
set PROJ_DATA=%proj_install_dir%/share/proj
```

If you built with `vcpkg` you will also need to add the dependencies folder to your path; this will normally be found in `vcpkg_installed/x64-windows` (or whatever platform you are using).

Now we're ready:
```
rocky_demo
```
<img width="500" alt="Screenshot 2023-02-22 124318" src="https://user-images.githubusercontent.com/326618/236200807-73567789-a5a3-46d5-a98d-e9c1f24a0f62.png">

There are some JSON map files in the `data` folder. Load one with the `--map` option:
```
rocky_demo --map data\openstreetmap.map.json
```
<img width="500" alt="Screenshot 2023-02-22 124318" src="https://github.com/user-attachments/assets/9590cca6-a170-4418-8588-1ee1d2b72924">


<br/><br/>
# Hello World

The easiest way to write a turnkey Rocky app is to use the `rocky::Application` object. It will create a viewer, a default map, and a scene graph to store everything you want to visualize.

## main.cpp
```c++
#include <rocky/rocky.h>

int main(int argc, char** argv)
{
    rocky::Application app(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    return app.run();
}
```

## CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.10)
project(myApp VERSION 0.1.0 LANGUAGES CXX C)
find_package(rocky CONFIG REQUIRED)
add_executable(myApp main.cpp)
target_link_libraries(myApp PRIVATE rocky::rocky)
install(TARGETS myApp RUNTIME DESTINATION bin)
```

<br/><br/>
# Maps
To render a map the first thing we need is map data. Map data can be huge and usually will not fit into the application's memory all at once. To solve that problem the standard approach is to process the source data into a hierarchy of *map tiles* called a *tile pyramid*.

Rocky supports a number of different map data *layers* that will read either *tile pyramids* from the network, or straight raster data from your local disk.

## Imagery
*Image layers* display the visible colors of the map. It might be satellite or aerial imagery, or it might be a rasterized cartographic map. Here are some ways to load image layers into Rocky.

### Loading Imagery from the Network
Let's start with a simple TMS ([OSGeo Tile Map Service](https://wiki.osgeo.org/wiki/Tile_Map_Service_Specification)) layer:
```c++
#include <rocky/rocky.h>
using namespace rocky;
...

// The map data model lives in our Application object.
Application app;
auto& map = app.mapNode->map;
...

// A TMS layer can use the popular TMS specification:
auto tms = TMSImageLayer::create();
tms->uri = "https://readymap.org/readymap/tiles/1.0.0/7";

map->add(tms);
```

We can also use the `TMSImageLayer` to load generic "XYZ" tile pyramids from the network. In this example, we are loading OpenStreetMap data.
```c++
// This will load the rasterizered OpenStreetMap data.
// We comply with the TOS by including attribution too.
auto osm = TMSImageLayer::create();
osm->uri = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
osm->attribution = rocky::Hyperlink{ "\u00a9 OpenStreetMap contributors", "https://openstreetmap.org/copyright" };

// This data source doesn't report any metadata, so we need to tell Rocky
// about its tiling profile. Most online data sources use "spherical-meractor".
osm->profile = rocky::Profile("spherical-mercator");

map->add(osm);
```

### Loading a Local File
You can load imagery datasets from your local disk as well. To do so we will use the `GDALImageLayer`. This layer is based on the [GDAL](https://gdal.org/en/stable/) toolkit which supports a vast array of raster formats.
```c++
auto local = GDALImageLayer::create();
local->uri = "data/world.tif"; // a local GeoTIFF file

map->add(local);
```
You can add as many image layers as you want - Rocky will composite them together at runtime.

## Terrain Elevation
Terrain elevation adds 3D heightmap data to your map.

Rocky supports elevation grid data in three formats:
* Single-channel TIFF (32-bit float)
* Mapbox-encoded PNG
* Terrarium-encoded PNG

```c++
auto elevation = TMSElevationLayer::create();
elevation->uri = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png";
elevation->encoding = ElevationLayer::Encoding::TerrariumRGB;
map->add(elevation);
```

## Elevation Queries
It is often useful to query the elevation at a given location. There are two ways to do it.

### Intersect the scene graph
This method is fast, but it limited to whatever terrain triangles are currently
in memory. Thus the result will depend on where your camera is and how much data is loaded.
```c++
GeoPoint point(SRS::WGS84, longitude, latitude);

auto clampedPoint = mapNode->terrain->intersect(point);

if (clampedPoint.ok())
    float elevation = clampedPoint->transform(SRS::WGS84).z;
```

### Query the source data
This method is the most accurate, and does not depend on what data is currently
loaded in the scene since it goes straight to the source layer. But it's usually slower.
```c++
ElevationSampler sampler;
sampler.layer = elevationLayerFromMap;

Result<ElevationSample> result = sampler.sample(point, app.io());

if (result.ok())
    Log()->info("Result = {}m", result.value());

// or, return a clamped GeoPoint:
Result<GeoPoint> clampedPoint = sampler.clamp(point, app.io());
```

You can also batch queries to the `ElevationSampler`, which speeds things up when querying a localized area. A batched query session modifies the input data directly. It expects the `SRS` of the points you plan to sample. You also get optional control over the sampling resolution ... lower resolution is often faster.
```c++
ElevationSampler sampler;
sampler.layer = ...;

auto session = sampler.session(app.io());
session.srs = pointsSRS; // required!
session.resolution = Distance(100, Units::METERS);
session.clampRange(points.begin(), points.end());
```
We use this technique in the `Demo_MVTFeatures.h` example to clamp GIS features to the terrain.

<br/>

## Vector Features
Rocky include some facilities for loading GIS Feature data through GDAL. GDAL has many drivers to load different types of feature data.

Open an ESRI Shapefile and iterate over all its features:
```c++
#include <rocky/GDALFeatureSource.h>
...
auto fs = rocky::GDALFeatureSource::create();
fs->uri = "my_vectors.shp";
auto status = fs->open();

if (status.ok())
{
    fs.each(app.io(), [](Feature&& feature)
        {
            // ... each Feature object contains geometry and fields
        });
}
```

`FeatureView` will help turn features into visible geometry:
```c++
FeatureView feature_view;

// add features to our FeatureView helper:
fs.each(app.io(), [&](Feature&& feature)
    {
        feature_view.features.emplace_back(std::move(feature));
    });
       
// Generate primitives (lines and meshes) in the world coordinate system:
auto prims = feature_view.generate(app.mapNode->worldSRS());

// Put our new primitives into the ECS registry:
if (!prims.empty())
{
    app.registry.write([&](entt::registry& registry)
        {
            auto e = prims.createEntity(registry);
            entities.emplace_back(e);
        });
}
```
Worldwide data? Check out the [Mapbox Vector Tiles](src/apps/rocky_demo/Demo_MVT.h) demo.

<br/><br/>

# Spatial Reference Systems

A **Spatial Reference System (SRS)** defines how geographic data is mapped to real-world locations. It specifies the coordinate system, projection, and datum used to interpret spatial data. SRSs are essential for ensuring that geographic features are accurately located and can be combined or compared across different datasets.

In Rocky, the `SRS` class (see `SRS.h`) provides a convenient interface for working with spatial reference systems. It allows you to define, query, and convert between different coordinate systems, such as geographic (latitude/longitude) and projected (e.g., UTM, Web Mercator).

### Built-in SRS Definitions

Rocky provides several commonly used SRS definitions as static constants in `SRS.h`, so you don't need to remember EPSG codes or WKT strings. These include:

- `rocky::SRS::WGS84` &mdash; Geographic coordinates (latitude/longitude, EPSG:4326)
- `rocky::SRS::SPHERICAL_MERCATOR` &mdash; Spherical Mercator (meters, EPSG:3857)
- `rocky::SRS::ECEF` &mdash; Earth Centered Earth Fixed (meters, geocentric)

You can use these directly in your code:
```c++
#include <rocky/SRS.h>

// Use the built-in WGS84 SRS
auto srs_geographic = rocky::SRS::WGS84;
```

### Creating and Using an SRS

You can also create an SRS from an EPSG code, WKT string, or PROJ init string:
```c++
rocky::SRS srs_custom("EPSG:32633"); // UTM zone 33N
```

**Checking SRS Properties:**
```c++
if (srs_geographic.isGeographic()) {
    // This SRS uses latitude/longitude coordinates
}
```

**Transforming Coordinates:**
```c++
rocky::GeoPoint geo_point(srs_geographic, 12.0, 55.0, 0.0); // lon, lat, alt
auto utm_point = geo_point.transform(srs_custom);
```

Spatial reference systems ensure that your map data aligns correctly, support accurate distance and area calculations, and enable visualization in a wide variety of map projections. For more details, refer to the documentation in `SRS.h` and the examples provided in the Rocky source code.

<br/><br/>

# Annotations

Rocky has a set of built-in primitives for displaying objects on the map.

* Line - a string of 2D line segments
* Mesh - a collection of triangles
* Point - a collection of points
* Model - a VSG scene graph representing an object
* Widget - an interactive ImGui panel (for labels, icons, or custom UIs)
* Label - a simple text label (based on Widget)

To create and manage these elements, Rocky uses an [Entity Component System](https://en.wikipedia.org/wiki/Entity_component_system) (ECS) driven by the popular [EnTT](https://github.com/skypjack/entt) SDK. We will not delve into the benefits of an ECS for data management here. Suffice it to say that it is a very popular mechanism used in modern gaming and graphics engine with excellent performance and scalability benefits.

*This subsystem is completely optional. Since Rocky is built with [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph), you can use its API to populate your scene in any way you choose.*

## Creating Entities and Components
Let's look at a simple example.
```c++
#include <rocky/rocky.h>

using namespace rocky;

Application app;
...

void addLabel(const std::string& text)
{
    // Start by acquiring a write-lock on the registry.
    // The lock will release automatically at the end of the current scope.
    auto& [lock, registry] = app.registry.write();

    // create a new entity
    auto entity = registry.create();
    
    // attach a simple label:
    Widget& label = registry.emplace<Widget>(entity);
    label.text = "Hello, world";
    
    // attach a Transform component to position our entity on the map:
    Transform& transform = registry.emplace<Transform>(entity);
    transform.position = GeoPoint(SRS::WGS84, -76, 34, 0);
    transform.dirty();
}
```
As you can see, the ECS works by creating an `entity` and then attaching *components* to that entity. You are not limited to Rocky's built-in components; you can create and attach your own types as well.

## The Registry
Let's briefly talk about the *Entity Registry*. In the ECS, the *registry* is a database that holds all your entities and components. Rocky wraps the EnTT `entt::registry` in a locking mechanism that makes it safer to access your registry from more than one thread should you choose to do so. You just need to follow this usage pattern:
```c++
void function_that_creates_or_destroys_things(Application& app)
{
   auto& [lock, registry] = app.registry.write();
   
   // ALL registry operations are safe here, including:
   auto e = registry.create();                // creating a new entity
   auto& comp = registry.emplace<Line>(e);    // attaching a new component
   registry.remove<Line>(e);                  // removing a component
   registry.destroy(e);                       // destroying an entity (and all its attachments)
}

void function_that_only_reads_or_edits_things(Application& app)
{
   auto& [lock, registry] = app.registry.read();

   // ONLY actions that read data or modify data in-place are safe here, including:
   auto& comp = registry.get<LineGeometry>(e);         // look up an existing component
   line.points.emplace_back(glm::dvec3(-72, 34, 0));   // modify a component in-place
   for(auto&& [line, xform] : registry.view<Line, Transform>().each()) { ... }  // iterate data

   // NOT safe!!
   // create(), emplace(), emplace_or_replace(), remove(), destroy();
}
```

You can also use a lambda function to encapsulate your updates:
```c++
app.registry.write([&](entt::registry& registry)
    {
        auto e = registry.create();
        auto& widget = registry.emplace<Widget>(e);
        widget.text = "WKRP";
    });

app.registry.read([&](entt::registry& registry)
    {
        auto& visibility = registry.get<Visibility>(e);
        visibility.visible[0] = true;
    });
```

While the example here will show you the basics, we recommend you read up on the [EnTT SDK](https://github.com/skypjack/entt) if you want to understand the full breadth of the registry's API!

## Control Components
*Control Components* do not render anything, but rather affect how other components attached to your entity behave.

As we've already seen, you can position an entity using a `Transform` component:
```c++
// Use a 'write' block to emplace the Transform.
// Later you can use a 'read' block to update it.
app.registry.write([&](entt::registry& registry)
    {
        Transform& transform = registry.emplace<Transform>(entity);

        // Set the geospatial position in the SRS of your choice:
        transform.position = GeoPoint(SRS::WGS84, -76, 34, 0);

        // Whether any geometry (like a Line or Model) attached to the same entity
        // will render relative to a topographic tangent plane
        transform.localTangentPlane = true;
    });
```

Use the `Visibility` component to toggle an entity's visibility. (Rocky automatically adds a `Visibility` whenever you create one of the built-in primitive types - you don't have to emplace it yourself.) The component is actually an array so you can control visibility on a per-view basis.
```c++
// Set visibility on a particular view:
app.registry.read([&](entt::registry& r)
    {
        Visibility& vis = r.get<Visibility>(entity);
        vis[0] = true; // index 0 is the default view
    });

// Or set it across all views using the setVisible convenience function:
app.registry.read([&](entt::registry& r)
    {
        auto& vis = r.get<Visibility>(entity);
        rocky::setVisible(r, entity, true);
    });
```


Other control components include:
* `ActiveState` (for the overall active state of an entity)
* `Declutter` (whether, and how, the entity participates in screen decluttering)

In the `rocky_demo` application you will find example code for each component, in the header files `Demo_Icon.h`, `Demo_Line.h`, etc.



<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/96f128d5-9391-4b92-aa00-6b9fde0a2b35">


<br/><br/>

# Rocky and Dear ImGui

[Dear ImGui](https://github.com/ocornut/imgui) is a runtime UI SDK for C++. Rocky integrates with ImGui in two ways.

### ImGui Widgets
Rocky has an ECS component called `Widget` that lets you place an ImGui window anywhere on the Map (using a `Transform`) and treat it just like other components.

```c++
auto [lock, registry] = app.registry.write();

auto entity = registry.create();

Widget& widget = registry.emplace<Widget>(entity);

widget.render = [](WidgetInstance& i)
{
    ImGui::SetCurrentContext(i.context);
    i.renderWindow([&]() {
        ImGui::Text("Hello, world!");
    });
};

auto& transform = registry.emplace<Transform>(entity);
transform.setPosition(GeoPoint(SRS::WGS84, 0, 0));
```

`WidgetInstance` contains various things you can use to customize the position and appearance of your Widget.

`renderWindow()` is just a convenience. If you want more control, roll your own:
```c++
widget.render = [](WidgetInstance& i)
{
    ImGui::SetCurrentContext(i.context);

    // window setup and creation:
    ImGui::SetNextWindowPos(ImVec2(i.center.x - i.size.x / 2, i.center.y - i.size.y / 2));
    ImGui::Begin(i.uid.c_str(), nullptr, i.windowFlags);

    ImGui::Text("Hello, world!");

    // window cleanup:
    i.size = ImGui::GetWindowSize();
    ImGui::End();
};
```

Rendering images or icons with ImGui is easy too. Use the `rocky::ImGuiImage` class to wrap a Rocky `Image` for use with ImGUi:
```c++
rocky::Image image;
...
ImGuiImage imguiImage(image, vsgcontext);
...
// later, to render...
ImGui::Image(imguiImage.handle(), imguiImage.size());
```

**IMPORTANT!!** Anything you do with ImGui-based Widgets is *context-sensitive* so you must activate the current context explicitly when rendering by calling
```c++
ImGui::SetCurrentContext(...);
```

[Demo_Widget.h](src/apps/rocky_demo/Demo_Widget.h) and [Demo_LabelFeatures](src/apps/rocky_demo/Demo_LabelFeatures.h) and [Demo_Simulation](src/apps/rocky_demo/Demo_Simulation.h) have more examples.

### Creating an Application GUI
Rocky has an `ImGuiIntegration` API that makes it easy to render a GUI atop your map display.
```c++
#include <rocky/rocky_imgui.h>
...

struct MyGUI : public vsg::Inherit<ImGuiContextNode, MainGUI>
{
    void render(ImGuiContext* imguiContext) const override
    {
        ImGui::SetCurrentContext(imguiContext); // important!!
        
        if (ImGui::Begin("Main Window")) 
        {
            ImGui::Text("Hello, world!");
        }
        ImGui::End();
    }
};
...

Application app;
...

auto traits = vsg::WindowTraits::create(1920, 1080, "Main Window");
auto main_window = app.displayManager->addWindow(traits);

auto imgui_renderer = RenderImGuiContext::create(main_window);
imgui_renderer->add(MyGUI::create());
app.install(imgui_renderer);
```
That's it. **Don't forget** to call `ImGui::SetCurrentContext` at the top of your `render` function!



<br/><br/>

# Rocky and VulkanSceneGraph

If you need more fine-grained control over the view and the camera in a VulkanSceneGraph (VSG) application, you
can use a `rocky::Application` object and add your windows and views by hand. Here is an example:
```c++
rocky::Application app(argc, argv);

auto imagery = rocky::TMSImageLayer::create();
imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
app.mapNode->map->add(imagery);

double nearFarRatio = 0.00001;
double R = app.mapNode->srs().ellipsoid().semiMajorAxis();

auto traits = vsg::WindowTraits::create(1920, 1080, "window");
auto window = vsg::Window::create(traits);

auto perspective = vsg::Perspective::create(
    30.0,
    (double)(window->extent2D().width) / (double)(window->extent2D().height),
    R * nearFarRatio,
    R * 10.0);

auto lookAt = vsg::LookAt::create(
    vsg::dvec3(R * 10.0, 0.0, 0.0),
    vsg::dvec3(0.0, 0.0, 0.0),
    vsg::dvec3(0.0, 0.0, 1.0));

auto camera = vsg::Camera::create(
    perspective,
    lookAt,
    vsg::ViewportState::create(window->extent2D()));

auto view = vsg::View::create(camera, app.mainScene);

app.display.addWindow(window, view);

app.viewer->addEventHandler(vsg::Trackball::create(camera));

app.renderContinuously = true;

return app.run();
```

You can also skip the `Application` object altogether and run your own frame loop. Just be aware: the ECS-based Annotation system is managed by `Application` so it will not be available (unless you install and manage it yourself.)

Here's the framework:

```c++
#include <rocky/rocky.h>
...

auto viewer = vsg::Viewer::create();

auto context = rocky::VSGContextFactory::create(viewer);

auto mapNode = rocky::MapNode::create(context);

// ... add the MapNode to your scene graph wherever you like
myScene->addChild(mapNode);

while (viewer->advanceToNextFrame())
{        
    viewer->handleEvents();
    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();
}
```
If you want to add Rocky's stock `MapManipulator` to that Viewer to control the map:
```c++
viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, context));
```

Still want the ECS system? Create your own `Registry` and `ECSNode`:
```c++
Registry registry;
...
// true = install all the same systems that Application installs
auto ecs = rocky::ECSNode::create(registry, true);
myScene->addChild(ecs);
```

## Terrain Rendering Profiles

Rocky supports two map-tiling profiles for rendering the terrain.

You can configure the rendering profile in your `MapNode`:
```c++
mapNode->profile = Profile("global-qsc");
```

**global-geodetic** : Loads fast, but sacrifices some quality in the polar regions. This is the default since it closely matches many online data sources.<br/>
<img width="480" height="320" alt="global-geodetic" src="https://github.com/user-attachments/assets/2a07e68c-6881-4d4b-b743-4b5365b324f4" />

**global-qsc** : Accurate worldwide including the polar regions. The trade-off can be slower loading speeds depending on your data. If you work in the polar regions or use polar stereographic data, this will be the better option.<br/>
<img width="480" height="320" alt="global-qsc" src="https://github.com/user-attachments/assets/adb12b63-9013-4fb0-b875-1a7b24b79d53" />

The *Rendering* panel in `rocky_demo` has a drop-down to select the profile so you can see the difference. Toggle on *Show triangles* for a better look.

## VSG Nodes and Layers

Use Rocky's `NodeLayer` to wrap a `vsg::Node` in a Rocky map layer:
```c++
auto layer = NodeLayer::create(my_vsg_node);
...
map->add(layer);
```

Use the `EntityNode` to manage a collection of Rocky ECS Components in a VSG node. The entities' visibility will be automatically controlled by the normal culling of the scene graph:
```c++
auto entityNode = EntityNode::create(registry);
entityNode->entities.emplace_back(...);
```
And of course you can combine the two and put an `EntityNode` inside a `NodeLayer`. Opening and closing the layer will show and hide the entities in the `EntityNode`.

## VSG and Math Functions
Rocky uses the [glm](https://github.com/g-truc/glm) library for math operations, whereas VulkanSceneGraph has its own math objects. Luckily they are practically identical
and it is easy to convert between them:
```c++
vsg::dvec3 vsg_input(...);
glm::dvec3 glm_value = to_glm(vsg_input); // convert from VSG to GLM
vsg::dvec3 vsg_value = to_vsg(glm_value); // convert from GLM to VSG
```

<br/><br/>

# Rocky and Qt

You can embed Rocky in a Qt widget. See the `rocky_demo_qt` example for details.

Both Qt5 and Qt6 are supported, based on the underlying support in [vsgQt](https://github.com/vsg-dev/vsgQt).

<img width="100%" alt="image" src="https://github.com/user-attachments/assets/84cda604-f617-4562-b208-6d049f8b5ee1">

<br/><br/>
# Acknowledgements

Thanks to these excellent open source projects that help make Rocky possible!
* [cpp-httplib](https://github.com/yhirose/cpp-httplib)
* [entt](https://github.com/skypjack/entt)
* [GDAL](https://github.com/OSGeo/gdal) (optional)
* [glm](https://github.com/g-truc/glm)
* [ImGui](https://github.com/ocornut/imgui) (optional)
* [nlohmann-json](https://github.com/nlohmann/json)
* [openssl](https://github.com/openssl/openssl) (optional)
* [proj](https://github.com/OSGeo/PROJ)
* [spdlog](https://github.com/gabime/spdlog)
* [sqlite3](https://github.com/sqlite/sqlite) (optional)
* [vsgXchange](https://github.com/vsg-dev/vsgXchange) (optional)
* [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)
* [weejobs](https://github.com/pelicanmapping/weejobs) (embedded)
