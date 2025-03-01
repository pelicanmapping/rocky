<h1>:mountain: Rocky</h1>

Rocky is a C++ SDK for rendering maps and globes. <img align="right" width="200" alt="Screenshot 2023-02-22 124318" src="https://user-images.githubusercontent.com/326618/220712284-8a17d87a-431f-4966-a425-0f2628b23b40.png">

Rocky will render an accurate 3D or 2D map with real geospatial imagery and elevation data. It supports thousands of map projections and many popular geodata sources including GeoTIFF, TMS, OpenStreetMap, WMTS, WMS, and Azure Maps. Rocky's data model is inspired by the osgEarth SDK, a 3D GIS toolkit created in 2008 and still in wide use today.

This project is in its early stages so expect a lot of API and architectural changes before version 1.0. 

![Windows](https://github.com/pelicanmapping/rocky/actions/workflows/ci-windows-minimal.yml/badge.svg)
![Doxygen](https://github.com/pelicanmapping/rocky/actions/workflows/doxygen-gh-pages.yml/badge.svg)

<!-- TOC start (generated with https://github.com/derlin/bitdowntoc) -->

- [Setup](#getting-started)
   * [Build the SDK](#build-the-sdk)
   * [Run the Demo](#run-the-demo)
- [Hello World](#hello-world)
   * [main.cpp](#maincpp)
   * [CMakeLists.txt](#cmakeliststxt)
- [Maps](#maps)
- [Entities](#entities)
   * [Creating entities and components](#creating-entities-and-components)
   * [The entity registry](#the-entity-registry)
   * [Control components](#control-components)
- [Integrations](#integrations)
   * [Rocky and ImGui](#rocky-and-imgui)
   * [Rocky and VulkanSceneGraph](#rocky-and-vulkanscenegraph)
   * [Rocky and Qt](#rocky-and-qt)
- [Acknowedgements](#acknowledgements)

<!-- TOC end -->

<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/3767d7e8-364c-498f-a09f-2b0e17b45c0a">

# Setup

## Build the SDK
Rocky uses CMake, and we maintain the build on Windows and Linux.
Rocky comes with a handy Windows batch file to automatically configure the project using `vcpkg`:
```bat
bootstrap-vcpkg.bat
```
That will download and build all the dependencies (takes a while) and generate your CMake project and Visual Studio solution file.

If you would rather not use vcpkg, you can build and install the [dependencies](#acknowledgements) yourself, or use your favorite package manager (like `apt` on Linux).

## Run the Demo
Rocky is pretty good at finding its data files, but you might need to set a couple environment variables to help:
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

# Hello World

The easiest way to write a turnkey Rocky app is to use the `rocky::Application` object. It will create a viewer, a default map, and a scene graph to store everything you want to visualize.

## main.cpp
```c++
#include <rocky/vsg/Application.h>
#include <rocky/TMSImageLayer.h>

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
# Maps
Coming soon.
This section will talk about Map data and SRS's.

# Entities
Rocky has a set of built-in primitives for displaying map annotations:

* Icon - a 2D billboarded image
* Label - a text string
* Line - a string of 2D line segments
* Mesh - a collection of triangles
* Model - a VSG scene graph representing an object
* Widget - an interactive ImGui panel

To create and manage these elements, Rocky uses an Entity Component System (ECS) driven by the excellent EnTT SDK.

## Creating Entities and Components
Let's look at a simple example.
```c++
#include <rocky/vsg/Application.h>
#include <rocky/vsg/ecs.h>

using namespace rocky;

Application app;
...

void addLabel(const std::string& text)
{
    // Start by acquiring a write-lock on the entity registry.
    // The lock will release automatically at the end of the current scope,
    // or you can call lock.unlock() to release it manually.
    auto [lock, registry] = app.registry.write();

    // create a new entity
    auto entity = registry.create();
    
    // attach a Label component:
    Label& label = registry.emplace<Label>(entity);
    label.text = "Hello, world";
    
    // attach a Transform component to position our entity on the map:
    Transform& transform = registry.emplace<Transform>(entity);
    transform.position = GeoPoint(SRS::WGS84, -76, 34, 0);
    transform.dirty();
}
```
As you can see, the ECS works by creating an `entity` and then attaching *components* to that entity.And you are not limited to Rocky's built-in components; you can create and attach your own types as well.

## The Entity Registry
Let's briefly talk about the *Entity Registry*. In the ECS, the *registry* is a container that holds all your entities and components. Rocky wraps the EnTT `entt::registry` in a locking mechanism that makes it safer to access your registry from more than one thread. You just need to follow this usage pattern:
```c++
void function_that_creates_or_destroys_things(Application& app)
{
   auto [lock, registry] = app.registry.write();
   
   // ALL registry calls are safe here, including:
   auto e = registry.create();                // creating a new entity
   auto& label = registry.emplace<Label>(e);  // attaching a new component
   registry.remove<Label>(e);                 // removing a component
   registry.destroy(e);                       // destroying an entity (and all its attachments)
}

void function_that_only_reads_or_edits_things(Application& app)
{
   auto [lock, registry] = app.registry.read();

   // ONLY actions that read data or modify data in-place are safe here, including:
   auto& label = registry.get<Label>(e);    // look up an existing component
   label.text = "New text";                 // modify a component in-place
   for(auto&&[label, xform] : registry.view<Label, Transform>().each()) { ... }  // iterate data

   // NOT safe!!
   // create(), emplace(), emplace_or_replace(), remove(), destroy();
}
```
While the example here will show you the basics, we highly recommend you read up on the [EnTT SDK](https://github.com/skypjack/entt) if you want to understand the full breadth of the registry's API!

## Control Components
*Control Components* do not render anything, but rather affect how other components attached to your entity behave.

As we've already seen, you can position an entity using a `Transform` component:
```c++
auto[lock, registry] = app.registry.write();

Transform& transform = registry.emplace<Transform>(entity);

// Set the geospatial position in the SRS of your choice:
transform.position = GeoPoint(SRS::WGS84, -76, 34, 0);

// Whether any geometry (like a Line or Model) attached to the same entity will render relative to a topographic tangent plane (as opposed to absolute coordinates)
transform.localTangentPlane = true;
```
You toggle an entity's visiblity, use the `Visibility` component. (Rocky automatically adds a `Visibility` whenever you create one of the built-in primitive types - you don't have to emplace it yourself.) The component is actually an array so you can control visibility on a per-view basis.
```c++
auto[lock, registry] = app.registry.read();

Visibility& vis = registry.get<Visibility>(entity);
vis[0] = true; // index 0 is the default view
```
Other control components include:
* `Active` (for the overall active state of an entity)
* `Declutter` (whether, and how, the entity participates in screen decluttering)

In the `rocky_demo` application you will find example code for each component, in the header files `Demo_Icon.h`, `Demo_Line.h`, etc.



<hr/>
<img width="100%" alt="image" src="https://github.com/user-attachments/assets/96f128d5-9391-4b92-aa00-6b9fde0a2b35">

<hr/>

# Integrations

## Rocky and Dear ImGui
[Dear ImGui](https://github.com/ocornut/imgui) is a fast, flexible runtime UI SDK for C++. Rocky integrates smoothly with ImGui in two ways.

### Creating a top-level GUI
Rocky has an `ImGuiIntegration` API that makes it easy to render a GUI atop your map display.
```c++
#include <rocky/vsg/imgui/ImGuiIntegration.h>
...

struct MyGUI : public vsg::Inherit<ImGuiNode, MainGUI>
{
    void render(ImGuiContext* imguiContext) const override
    {
        ImGui::SetCurrentContext(imguiContext);
        
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

auto imgui_group = ImGuiIntegration::addContextGroup(app.displayManager, main_window);

imgui_group->add(MyGUI::create(), app);
```
That's basically it. *Don't forget* to call `ImGui::SetCurrentContext` at the top of your render function!

### Using ImGui Widgets
Rocky has an ECS component called `Widget` that lets you place an ImGui window anywhere on the Map and treat it just like other components.
```c++
auto [lock, registry] = app.registry.write();

auto entity = registry.create();

Widget& widget = registry.emplace<Widget>(entity);
widget.render = [](WidgetInstance& i)
    {
        ImGui::SetCurrentContext(i.context);
        ImGui::SetNextWindowPos(i.position);
        if (ImGui::Begin(i.uid.c_str(), nullptr, i.defaultWindowFlags))
        {
            ImGui::Text("Hello, world!");                        
        }
        ImGui::End();
    };

auto& transform = registry.emplace<Transform>(entity);
transform.setPosition(GeoPoint(SRS::WGS84, 0, 0, 0));
```

## Rocky and VulkanSceneGraph
If you're already using VSG in your application and want to add a `MapNode` to a view, do this:
```c++
// Your VSG viewer:
auto viewer = vsg::Viewer::create();

// Make a runtime context for the viewer:
auto context = rocky::VSGContextFactory::create(viewer);

// Make a map node to render your map data:
auto mapNode = rocky::MapNode::create(context);

// optional - add one or more maps to your map:
auto layer = rocky::TMSImageLayer::create();
layer->uri = "https://[abc].tile.openstreetmap.org/{z}/{x}/{y}.png";
layer->setProfile(rocky::Profile::SPHERICAL_MERCATOR);
layer->setAttribution(rocky::Hyperlink{ "\u00a9 OpenStreetMap contributors", "https://openstreetmap.org/copyright" });
mapNode->map->add(layer);
...
scene->addChild(mapNode);
...
// Run your main loop as usual
while (viewer->advanceToNextFrame())
{        
    viewer->handleEvents();
    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();
}
```
You'll probably also want to add the `MapManipulator` to that view to control the map:
```c++
viewer->addEventHandler(rocky::MapManipulator::create(mapNode, window, camera, context));
```

## Rocky and Qt
You can embed Rocky in a Qt widget. See the `rocky_demo_qt` example for details.

<img width="100%" alt="image" src="https://github.com/user-attachments/assets/84cda604-f617-4562-b208-6d049f8b5ee1">

# Acknowedgements

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
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)
* [weejobs](https://github.com/pelicanmapping/weejobs) (embedded)