/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#include <rocky/rocky.h>
#include <rocky/VectorTileLayer.h>
#include <rocky/MapboxVectorTileLayer.h>
#include <rocky/vsg/terrain/TerrainVectorTexture.h>
#include <atomic>
#include <set>
#include <thread>
#include <iostream>

using namespace ROCKY_NAMESPACE;

namespace
{
    //! Offline checker imagery demonstrates independent raster and vector tile transforms.
    class CheckerLayer : public Inherit<ImageLayer, CheckerLayer>
    {
    public:
        unsigned delay = 0;
        CheckerLayer() { profile = Profile("global-geodetic"); }
        Result<GeoImage> createTileImplementation(const TileKey& key, const IOOptions& io) const override
        {
            for (unsigned i = 0; i < delay && !io.canceled(); i += 10)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (io.canceled()) return Failure_OperationCanceled;
            auto image = Image::create(Image::R8G8B8A8_UNORM, 32, 32);
            const auto e = key.extent();
            for (unsigned y = 0; y < 32; ++y)
                for (unsigned x = 0; x < 32; ++x)
                {
                    const auto lon = e.xmin() + (x + 0.5) * e.width() / 32;
                    const auto lat = e.ymin() + (y + 0.5) * e.height() / 32;
                    const auto shade = (int(std::floor(lon / 5)) + int(std::floor(lat / 5))) & 1;
                    image->write(shade ? Color("#b9bfba") : Color("#d4d8d2"), x, y);
                }
            return GeoImage(image, e);
        }
    };

    //! Author continuous world-space artwork, then map it to each destination key without clipping edges.
    VectorTile artwork(const TileKey& key)
    {
        VectorTile tile;
        VectorTile::Shape land;
        land.color = Color("#72b681");
        land.contours = {
            { {-12, 5}, {12, 5}, {12, 35}, {-12, 35} },
            { {-6, 12}, {6, 12}, {6, 24}, {-6, 24} } };
        tile.shapes.push_back(land);
        land.color = Color(0.15f, 0.40f, 0.90f, 0.5f);
        land.contours = { { {4, 12}, {18, 12}, {18, 30}, {4, 30} } };
        tile.shapes.push_back(land);
        VectorTile::Shape road;
        road.type = VectorTile::Shape::Type::Line;
        road.contours = { { {-30, -5}, {0, 20}, {30, 40} }, { {-25, 22.5}, {25, 22.5} } };
        road.color = Color("#483b37");
        road.width = 0.6f;
        tile.shapes.push_back(road);
        road.color = Color("#ffdc83");
        road.width = 0.32f;
        tile.shapes.push_back(road);
        const auto e = key.extent();
        if (e.xmax() < -31 || e.xmin() > 31 || e.ymax() < -6 || e.ymin() > 41)
            return {};
        for (auto& shape : tile.shapes)
        {
            shape.width /= static_cast<float>(e.width());
            for (auto& contour : shape.contours)
                for (auto& p : contour)
                    p = { (p.x - e.xmin()) / e.width(), (p.y - e.ymin()) / e.height() };
        }
        return tile;
    }

    //! Inspect published terrain resources without making any projector/overlay entities.
    struct Inspect : vsg::Visitor
    {
        unsigned tiles = 0, active = 0, populated = 0, overzoomed = 0, pending = 0, detailed = 0;
        std::set<const TerrainVectorTexture*> resources;
        std::vector<std::string> pendingKeys;
        const Map& map;
        const std::uint64_t frame;
        std::optional<double> viewZoom;
        bool vectorsBeforeImagery = false;
        explicit Inspect(const Map& in, std::uint64_t currentFrame, std::optional<double> zoom) :
            map(in), frame(currentFrame), viewZoom(zoom) { }
        void apply(vsg::Object& object) override
        {
            if (auto tile = dynamic_cast<TerrainTileNode*>(&object))
            {
                ++tiles;
                // Cached subtrees outside the traversed view intentionally retain old revisions
                // until visited again. Only current traversal participants must converge now.
                const bool traversed = tile->lastSurfaceFrame.load() != 0 && tile->lastSurfaceFrame.load()+1u >= frame;
                if (traversed) ++active;
                const auto& resource = tile->renderModel.vectors;
                if (traversed && resource && resource->layerCount && !tile->renderModel.color.image)
                    vectorsBeforeImagery = true;
                const auto request = terrainVectorRequest(map, tile->key, viewZoom);
                if (traversed && (!resource || resource->signature != request.signature || resource->key != request.key))
                {
                    ++pending;
                    pendingKeys.push_back(tile->key.str());
                }
                if (resource)
                {
                    if (traversed && resource->layerCount > 1 && resource->signature == request.signature) ++detailed;
                    resources.insert(resource.get());
                    if (traversed && resource->layerCount) ++populated;
                    if (resource->key.valid() && tile->key.level > resource->key.level) ++overzoomed;
                }
            }
            object.traverse(*this);
        }
    };
}

//! Terrain surface demo; --openfreemap selects real MVT, and --verify exercises resource replacement and exits.
int main(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);
    bool verify = args.read("--verify");
    const bool keepOpen = args.read("--keep-open");
    const bool openfreemap = args.read("--openfreemap");
    const bool bufferedStrokes = args.read("--buffered-strokes");
    const bool churn = args.read("--churn");
    const bool profileCycle = args.read("--profile-cycle");
    std::string style = "openfreemap-terrain.style.json";
    double lon = -77.0365, lat = 38.90, range = 8.0;
    args.read("--style", style);
    args.read("--lon", lon); args.read("--lat", lat); args.read("--range", range);
    unsigned frames = 0, delay = 0, imageDelay = 0;
    args.read("--frames", frames);
    args.read("--delay", delay);
    args.read("--image-delay", imageDelay);
    Application app(argc, argv);
    app.mapNode->terrainSettings().lighting = false;
    app.mapNode->terrainSettings().maxLevel = openfreemap ? 18u : 7u;
    if (!openfreemap)
    {
        auto imagery = CheckerLayer::create();
        imagery->delay = imageDelay;
        app.mapNode->map->add(imagery);
    }
    auto canceledRequests = std::make_shared<std::atomic_uint>(0);
    auto empty = std::make_shared<std::atomic_bool>(false);
    VectorTileLayer::Ptr vectors = VectorTileLayer::create();
    vectors->name = "Terrain vector texture";
    vectors->maxDataLevel = 3u;
    vectors->createTileFunction = [empty, delay, canceledRequests](const TileKey& key, const IOOptions& io) -> Result<VectorTile>
    {
        for (unsigned i = 0; i < delay && !io.canceled(); i += 10)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (io.canceled()) { ++*canceledRequests; return Failure_OperationCanceled; }
        return empty->load() ? VectorTile{} : artwork(key);
    };
    MapboxVectorTileLayer::Ptr styled;
    if (openfreemap)
    {
        styled = MapboxVectorTileLayer::create();
        auto found = vsg::findFile(style,app.vsgcontext->searchPaths);
        styled->styleURI = URI(found.empty() ? style : found.string());
        styled->forceBufferedStrokes = bufferedStrokes;
        styled->maxDataLevel = 14u;
        auto opened = styled->open(app.io());
        if (opened.failed()) { std::cerr << opened.error().message << std::endl; return 5; }
        std::cout << "Attribution: " << styled->attributionText() << std::endl;
        vectors = styled;
    }
    app.mapNode->map->add(vectors);
    auto traits = vsg::WindowTraits::create(100, 100, 1100, 800, openfreemap ?
        "Rocky | OpenFreeMap | OpenMapTiles | (c) OpenStreetMap contributors" : "Rocky terrain vector textures");
    auto& window = app.display.addWindow(traits);
    if (auto manip = MapManipulator::get(window.view(0).vsgView))
    {
        Viewpoint vp;
        vp.point = GeoPoint(SRS::WGS84, openfreemap ? lon : 0.0, openfreemap ? lat : 20.0);
        vp.range = Distance(openfreemap ? range : 8500, Units::KILOMETERS);
        manip->setViewpoint(vp, std::chrono::duration<float>(0));
    }
    const auto start = std::chrono::steady_clock::now();
    auto phaseStart = start;
    unsigned stable = 0, rendered = 0, lastReport = 0;
    bool cleared = false, sawVectorsBeforeImagery = false;
    unsigned profileStep = 0;
    const std::vector<std::string> profileSequence =
        { "plate-carree", "global-qsc", "spherical-mercator", "global-geodetic" };
    while (app.frame())
    {
        ++rendered;
        // Verification only: invalidate in-flight requests by moving the ordinary camera rapidly.
        if (churn && rendered <= 120u && rendered % 30u == 0u)
        {
            Viewpoint vp;
            vp.point = GeoPoint(SRS::WGS84, rendered == 120u ? 0.0 : (rendered % 60u ? 90.0 : -90.0), 20.0);
            vp.range = Distance(8500, Units::KILOMETERS);
            MapManipulator::get(window.view(0).vsgView)->setViewpoint(vp, std::chrono::duration<float>(0));
        }
        if (!verify && frames > 0 && rendered >= frames) return 0;
        if (!verify) continue;
        Inspect inspect(*app.mapNode->map, app.vsgcontext->viewer()->getFrameStamp()->frameCount,
            app.mapNode->terrainNode->vectorStyleZoom());
        app.mapNode->terrainNode->accept(inspect);
        sawVectorsBeforeImagery |= inspect.vectorsBeforeImagery;
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if (styled && elapsed >= lastReport+10)
        {
            lastReport = static_cast<unsigned>(elapsed);
            std::cout << "Loading: tiles=" << inspect.tiles << " active=" << inspect.active << " pending=" << inspect.pending
                << " detailed=" << inspect.detailed << " elapsedSeconds=" << elapsed << std::endl;
        }
        if ((!churn || rendered > 120u) && inspect.tiles > 4 && inspect.pending == 0 &&
            (!styled || inspect.detailed >= 8)) ++stable;
        else stable = 0;
        if (stable >= 30)
        {
            std::cout << "tiles=" << inspect.tiles << " active=" << inspect.active << " resources=" << inspect.resources.size()
                << " populated=" << inspect.populated << " overzoomed=" << inspect.overzoomed << std::endl;
            if (styled)
            {
                auto s = styled->stats();
                std::cout << "styleZoom=" << styled->viewStyleZoom(inspect.viewZoom.value_or(12.0)).value() << " reads=" << s.reads << " hits=" << s.hits
                    << " cacheMiB=" << s.cacheBytes/(1024.0*1024.0) << " built=" << s.tilesBuilt
                    << " features=" << s.features << " detailed=" << inspect.detailed
                    << " gpuStrokePieces=" << s.gpuStrokePieces << " bufferedBatches=" << s.bufferedStrokeBatches
                    << " elapsedSeconds=" << std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()
                    << " frameMs=" << app.stats.frame.count()/1000.0 << std::endl;
                if (profileCycle && profileStep < profileSequence.size())
                {
                    // Exercise the same map-profile/camera transition as Demo_Views, retaining the source layer.
                    const auto profile = profileSequence[profileStep++];
                    std::cout << "Switching profile to " << profile << std::endl;
                    app.mapNode->profile = Profile(profile);
                    auto view = window.view(0).vsgView;
                    if (app.mapNode->profile.srs().isProjected())
                        view->camera->projectionMatrix = vsg::Orthographic::create(-1, 1, -1, 1, 1, 1e8);
                    else
                        view->camera->projectionMatrix = vsg::Perspective::create(45.0,
                            double(view->camera->getViewport().width)/view->camera->getViewport().height, 1, 1e8);
                    auto manip = MapManipulator::get(view);
                    manip->home();
                    Viewpoint vp;
                    vp.point = GeoPoint(SRS::WGS84, lon, lat);
                    vp.range = Distance(range, Units::KILOMETERS);
                    manip->setViewpoint(vp, std::chrono::duration<float>(0));
                    app.vsgcontext->requestFrame();
                    stable = 0;
                    phaseStart = std::chrono::steady_clock::now();
                    continue;
                }
                if (!cleared)
                {
                    auto manip = MapManipulator::get(window.view(0).vsgView);
                    Viewpoint vp = manip->viewpoint(); vp.point.x += 0.015;
                    vp.range = Distance(manip->distance()*0.5, Units::METERS);
                    manip->setViewpoint(vp); stable = 0; cleared = true;
                    phaseStart = std::chrono::steady_clock::now();
                    continue;
                }
                if (s.features == 0 || s.hits == 0) return 6;
                std::cout << "PASS: styled OpenFreeMap terrain, navigation, zoom replacement, source cache" << std::endl;
                if (keepOpen) { verify = false; continue; }
                return 0;
            }
            if (!cleared)
            {
                if (inspect.populated == 0 || inspect.overzoomed == 0 || inspect.resources.size() >= inspect.tiles)
                    return 2;
                empty->store(true);
                vectors->dirty();
                cleared = true;
                stable = 0;
            }
            else
            {
                if (inspect.populated != 0) return 3;
                std::cout << "canceledVectorRequests=" << canceledRequests->load()
                    << " vectorsBeforeImagery=" << sawVectorsBeforeImagery << std::endl;
                if (churn && canceledRequests->load() == 0) return 7;
                if (imageDelay > delay && !sawVectorsBeforeImagery) return 8;
                std::cout << "PASS: terrain vectors, shared overzoom, revision replacement, empty coverage" << std::endl;
                return 0;
            }
        }
        if (std::chrono::steady_clock::now() - phaseStart > std::chrono::seconds(styled ? 180 : 60))
        {
            std::cerr << "Timed out: tiles=" << inspect.tiles << " pending=" << inspect.pending << std::endl;
            for (const auto& key : inspect.pendingKeys) std::cerr << "Pending " << key << std::endl;
            return 4;
        }
    }
    return 1;
}
