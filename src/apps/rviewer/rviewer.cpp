#include <rocky/Instance.h>
#include <rocky/Notify.h>
#include <rocky/GDALLayers.h>
#include <rocky_vsg/MapNode.h>
#include <vsg/all.h>

using namespace rocky;

int usage(const char* msg)
{
    std::cout << msg << std::endl;
    return -1;
}

int main(int argc, char** argv)
{
    Instance instance;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (arguments.read({ "--help", "-h", "-?" }))
    {
        return usage(argv[0]);
    }

    // options that get passed to VSG reader/writer modules?
    auto options = vsg::Options::create();
    arguments.read(options);

    // window configuration
    auto traits = vsg::WindowTraits::create();
    traits->windowTitle = "Viewer";
    traits->debugLayer = arguments.read({ "--debug" });
    traits->apiDumpLayer = arguments.read({ "--api" });
    auto window = vsg::Window::create(traits);

    // main viewer
    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);

    // the scene graph
    auto vsg_scene = vsg::Group::create();

    // TODO: read this from an earth file
    auto mapNode = MapNode::create(instance);
    //auto layer = GDALImageLayer::create();
    //layer->setURI("D:/data/imagery/world.tif");
    //mapNode->getMap()->addLayer(layer);

    vsg_scene->addChild(mapNode);



    //auto terrainNode = mapNode->getTerrainNode();

    //auto terrainEngine = TerrainEngineVOE::create();

    //options->add(terrainEngine->tileReader);
    // add vsgXchange's support for reading and writing 3rd party file formats
    //options->add(vsgXchange::all::create());


#if 0
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "voe_globe";
    windowTraits->debugLayer = arguments.read({ "--debug", "-d" });
    windowTraits->apiDumpLayer = arguments.read({ "--api", "-a" });
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read({ "--fullscreen", "--fs" })) windowTraits->fullscreen = true;
    if (arguments.read({ "--window", "-w" }, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    arguments.read("--samples", windowTraits->samples);
    auto numFrames = arguments.value(-1, "-f");
    auto loadLevels = arguments.value(0, "--load-levels");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");
    bool useEllipsoidPerspective = !arguments.read({ "--disble-EllipsoidPerspective", "--dep" });
    arguments.read("--file-cache", options->fileCache);
    bool osgEarthStyleMouseButtons = arguments.read({ "--osgearth","-e" });

    uint32_t numOperationThreads = 0;
    if (arguments.read("--ot", numOperationThreads)) options->operationThreads = vsg::OperationThreads::create(numOperationThreads);

    const double invalid_value = std::numeric_limits<double>::max();
    double poi_latitude = invalid_value;
    double poi_longitude = invalid_value;
    double poi_distance = invalid_value;
    while (arguments.read("--poi", poi_latitude, poi_longitude)) {};
    while (arguments.read("--distance", poi_distance)) {};

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);
    terrainEngine->init(options, arguments, windowTraits);

    // create the viewer and assign window(s) to it
    bool multisampling = traits->samples != VK_SAMPLE_COUNT_1_BIT;

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(traits);
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return 1;
    }

    if (multisampling)
    {
        // Initializing the device causes VSG to get the supported sample counts from Vulkan and
        // use the largest one that supports our request.
        window->getOrCreateDevice();
        terrainEngine->samples = window->framebufferSamples();
    }

    // load the root tile.
    auto vsg_scene = terrainEngine->createScene(options);
    if (!vsg_scene) return 1;

    viewer->addWindow(window);
#endif

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    double nearFarRatio = 0.0005;

    // set up the camera
    vsg::ref_ptr<vsg::LookAt> lookAt;
    vsg::ref_ptr<vsg::ProjectionMatrix> perspective;

#if 0
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel"));
    if (ellipsoidModel)
    {
        if (poi_latitude != invalid_value && poi_longitude != invalid_value)
        {
            double height = (poi_distance != invalid_value) ? poi_distance : radius * 3.5;
            auto ecef = ellipsoidModel->convertLatLongAltitudeToECEF({ poi_latitude, poi_longitude, 0.0 });
            auto ecef_normal = vsg::normalize(ecef);

            centre = ecef;
            vsg::dvec3 eye = centre + ecef_normal * height;
            vsg::dvec3 up = vsg::normalize(vsg::cross(ecef_normal, vsg::cross(vsg::dvec3(0.0, 0.0, 1.0), ecef_normal)));

            // set up the camera
            lookAt = vsg::LookAt::create(eye, centre, up);
        }
        else
        {
            lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        }

        if (useEllipsoidPerspective)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        }
    }
    else
#endif
    {
        perspective = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5);
        lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    }

    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    // add close handler to respond the close window button and pressing escape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    //viewer->addEventHandler(terrainEngine->createWireframeHandler());

#if 0
    if (ellipsoidModel)
    {
        auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
        trackball->addKeyViewpoint(vsg::KeySymbol('1'), 51.50151088842245, -0.14181489107549874, 2000.0, 2.0); // Grenwish Observatory
        trackball->addKeyViewpoint(vsg::KeySymbol('2'), 55.948642740309324, -3.199226855522667, 2000.0, 2.0);  // Edinburgh Castle
        trackball->addKeyViewpoint(vsg::KeySymbol('3'), 48.858264952330764, 2.2945039609604665, 2000.0, 2.0);  // Eiffel Town, Paris
        trackball->addKeyViewpoint(vsg::KeySymbol('4'), 52.5162603714634, 13.377684902745642, 2000.0, 2.0);    // Brandenburg Gate, Berlin
        trackball->addKeyViewpoint(vsg::KeySymbol('5'), 30.047448591298807, 31.236319571791213, 10000.0, 2.0); // Cairo
        trackball->addKeyViewpoint(vsg::KeySymbol('6'), 35.653099536061156, 139.74704060056993, 10000.0, 2.0); // Tokyo
        trackball->addKeyViewpoint(vsg::KeySymbol('7'), 37.38701052699002, -122.08555895549424, 10000.0, 2.0); // Mountain View, California
        trackball->addKeyViewpoint(vsg::KeySymbol('8'), 40.689618207006355, -74.04465595488215, 10000.0, 2.0); // Empire State Building
        trackball->addKeyViewpoint(vsg::KeySymbol('9'), 25.997055873649554, -97.15543476551771, 1000.0, 2.0);  // Boca Chica, Taxas

        if (osgEarthStyleMouseButtons)
        {
            trackball->panButtonMask = vsg::BUTTON_MASK_1;
            trackball->rotateButtonMask = vsg::BUTTON_MASK_2;
            trackball->zoomButtonMask = vsg::BUTTON_MASK_3;
        }

        viewer->addEventHandler(trackball);
    }
    else
#endif
    {
        viewer->addEventHandler(vsg::Trackball::create(camera));
    }

#if 0
    // if required pre load specific number of PagedLOD levels.
    if (loadLevels > 0)
    {
        vsg::LoadPagedLOD loadPagedLOD(camera, loadLevels);

        auto startTime = std::chrono::steady_clock::now();

        vsg_scene->accept(loadPagedLOD);

        auto time = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "No. of tiles loaed " << loadPagedLOD.numTiles << " in " << time << "ms." << std::endl;
    }
#endif

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene, VK_SUBPASS_CONTENTS_INLINE, false);
    viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });
    viewer->compile();

    // rendering main loop
    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        //TODO: MapNode updates here.
        //terrainEngine->update(viewer, camera);
        viewer->recordAndSubmit();
        viewer->present();
    }

    return 0;
}

