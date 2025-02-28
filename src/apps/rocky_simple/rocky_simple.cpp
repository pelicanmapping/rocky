#include <rocky/vsg/Application.h>
#include <rocky/TMSImageLayer.h>
#include <rocky/vsg/ecs.h>

int main(int argc, char** argv)
{
    rocky::Application app(argc, argv);

    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
    app.mapNode->map->add(imagery);

    return app.run();
}
