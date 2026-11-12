/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */

/**
* Standalone Slug vector-overlay demo and benchmark: MVT buildings + roads over one city, with
* nothing else in the scene. Interactive by default; --bench parks the camera, waits for the
* GPU frame time to settle, times a fixed number of frames with GPU timestamps, prints one
* summary line and exits.
*
*   rocky_demo_slughorn --city tokyo
*   rocky_demo_slughorn --city tokyo --bench --label distance
*   rocky_demo_slughorn --city tokyo --bench --strokes baked --label baked
*   rocky_demo_slughorn --city tokyo --bench --raster --label raster
*
* Run with --help for every option.
*/

#include <rocky/rocky.h>
#include <rocky/ElevationSampler.h>
#include <rocky/ecs/Registry.h>
#include <rocky/ecs/Overlay.h>
#include <vsg/commands/ResetQueryPool.h>
#include <vsg/commands/WriteTimestamp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <vector>

using namespace ROCKY_NAMESPACE;

namespace
{
    struct City
    {
        std::string name;
        double lon;
        double lat;
        double rangeKm;
    };

    // Ranges keep a top-down 16:9 view inside a 5x5 grid of z14 tiles around the center.
    const std::vector<City> cities =
    {
        { "tokyo", 139.743, 35.684, 10.0 },
        { "helsinki", 24.919, 60.162, 7.0 },
        { "quito", -78.511, -0.206, 10.0 }
    };

    void usage()
    {
        std::cout << R"USAGE(rocky_demo_slughorn [options]

Scene:
  --city NAME          tokyo | helsinki | quito (default tokyo)
  --range KM           camera range (default per city)
  --size W H           window size (default 1920 1080)
  --tiles DIR          read MVT tiles from DIR/14/X/Y.pbf (XYZ rows) instead of readymap.org
  --imagery            add readymap.org imagery (default: none, bare terrain)
  --elevation          add readymap.org elevation and clamp features to it (default: none)
  --no-features        terrain only (baseline)
  --no-buildings       roads only
  --no-roads           buildings only
  --raster             Overlay mode Raster (the pre-Slug path) instead of Vector (Slug)

Slug experiments (rocky-bands SlugAdapter switches):
  --strokes MODE       distance (default) | baked
  --fill-batch N       split building fills into batches of at most N curves (0 = off)

Benchmark:
  --bench              park the camera, settle, time frames, print one line, exit
  --frames N           frames to time (default 600)
  --settle S           seconds of stable GPU time required before timing (default 3)
  --min-wait S         never settle before S seconds, so an empty scene isn't timed (default 8)
  --max-wait S         give up settling after S seconds and time anyway (default 180)
  --label TEXT         tag for the summary line
)USAGE" << std::endl;
    }

    // Two timestamps bracket the window's command graph. A ring of query slots lets each frame
    // read back the slot written gpuTimerRing frames earlier, which has finished, so reading
    // with VK_QUERY_RESULT_WAIT_BIT never stalls.
    struct GpuTimer
    {
        static constexpr std::uint32_t ring = 8;

        vsg::ref_ptr<vsg::QueryPool> pool;
        vsg::ref_ptr<vsg::ResetQueryPool> reset;
        vsg::ref_ptr<vsg::WriteTimestamp> begin;
        vsg::ref_ptr<vsg::WriteTimestamp> end;
        double nsPerTick = 1.0;
        std::uint64_t framesWritten = 0;

        void attach(vsg::ref_ptr<vsg::CommandGraph> cg, vsg::ref_ptr<vsg::Window> window)
        {
            nsPerTick = window->getOrCreatePhysicalDevice()->getProperties().limits.timestampPeriod;

            pool = vsg::QueryPool::create();
            pool->queryType = VK_QUERY_TYPE_TIMESTAMP;
            pool->queryCount = 2 * ring;

            reset = vsg::ResetQueryPool::create(pool);
            reset->queryCount = 2;
            begin = vsg::WriteTimestamp::create(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, 0);
            end = vsg::WriteTimestamp::create(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, 1);

            cg->children.insert(cg->children.begin(), { reset, begin });
            cg->children.push_back(end);
        }

        //! Call once before each frame; returns the GPU time (ms) of the frame ring frames ago.
        std::optional<double> next()
        {
            std::optional<double> result;
            auto slot = static_cast<std::uint32_t>(framesWritten % ring);

            if (framesWritten >= ring)
            {
                std::vector<std::uint64_t> ticks(2);
                if (pool->getResults(ticks, 2 * slot) == VK_SUCCESS)
                    result = static_cast<double>(ticks[1] - ticks[0]) * nsPerTick * 1e-6;
            }

            reset->firstQuery = 2 * slot;
            begin->query = 2 * slot;
            end->query = 2 * slot + 1;
            framesWritten++;

            return result;
        }
    };

    struct Summary
    {
        double mean = 0.0;
        double median = 0.0;
        double p95 = 0.0;
        double min = 0.0;
    };

    void setEnv(const char* name, const std::string& value)
    {
#ifdef _WIN32
        ::_putenv_s(name, value.c_str());
#else
        ::setenv(name, value.c_str(), 1);
#endif
    }

    Summary summarize(std::vector<double> v)
    {
        Summary s;
        if (v.empty())
            return s;

        std::sort(v.begin(), v.end());
        s.mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        s.median = v[v.size() / 2];
        s.p95 = v[std::min(v.size() - 1, v.size() * 95 / 100)];
        s.min = v.front();
        return s;
    }
}

int main(int argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    if (args.read({ "--help", "-h" }))
    {
        usage();
        return 0;
    }

    std::string cityName = "tokyo";
    args.read("--city", cityName);

    auto cityIter = std::find_if(cities.begin(), cities.end(), [&](const City& c) { return c.name == cityName; });
    if (cityIter == cities.end())
    {
        std::cerr << "Unknown city '" << cityName << "'" << std::endl;
        return 1;
    }
    City city = *cityIter;
    args.read("--range", city.rangeKm);

    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    args.read("--size", width, height);

    std::string tilesDir;
    args.read("--tiles", tilesDir);

    bool imagery = args.read("--imagery");
    bool elevation = args.read("--elevation");
    bool noFeatures = args.read("--no-features");
    bool noBuildings = args.read("--no-buildings");
    bool noRoads = args.read("--no-roads");
    OverlayMode overlayMode = args.read("--raster") ? OverlayMode::Raster : OverlayMode::Vector;

    // The SlugAdapter reads these when it builds each atlas.
    std::string strokes = "distance";
    if (args.read("--strokes", strokes))
        setEnv("ROCKY_SLUG_STROKES", strokes);

    std::string fillBatch = "0";
    if (args.read("--fill-batch", fillBatch))
        setEnv("ROCKY_SLUG_FILL_BATCH", fillBatch);

    bool bench = args.read("--bench");
    int benchFrames = 600;
    double settleSeconds = 3.0;
    double minWaitSeconds = 8.0;
    double maxWaitSeconds = 180.0;
    std::string label = "run";
    args.read("--frames", benchFrames);
    args.read("--settle", settleSeconds);
    args.read("--min-wait", minWaitSeconds);
    args.read("--max-wait", maxWaitSeconds);
    args.read("--label", label);

    rocky::Application app(argc, argv);

    if (app.commandLineStatus.failed())
    {
        Log()->error(app.commandLineStatus.error().string());
        return 1;
    }

    auto map = app.mapNode->map;

    if (imagery)
    {
        auto layer = rocky::TMSImageLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/7/";
        map->add(layer);
    }

    if (elevation)
    {
        auto layer = rocky::TMSElevationLayer::create();
        layer->uri = "https://readymap.org/readymap/tiles/1.0.0/116/";
        map->add(layer);
    }

    auto traits = vsg::WindowTraits::create(10, 10, width, height, "rocky_demo_slughorn");
    if (bench)
        traits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

    auto& window = app.display.addWindow(traits);

    app.scene->addChild(vsg::createHeadlight());

    // Shared styles, as in rocky_demo's "Vector tiles" demo.
    entt::entity styleEntity = entt::null;
    app.registry.write([&](entt::registry& reg)
        {
            styleEntity = reg.create();

            auto& lineStyle = reg.emplace<LineStyle>(styleEntity);
            lineStyle.color = Color(0x050505FF);
            lineStyle.width = 7.0f;
            lineStyle.widthUnits = Units::METERS;
            lineStyle.depthOffset = 10;
            lineStyle.outlineColor = StockColor::White;
            lineStyle.outlineWidth = 0.5f;

            auto& polygonStyle = reg.emplace<PolygonStyle>(styleEntity);
            polygonStyle.color = Color(1, 0.75f, 0.2f, 1);
            polygonStyle.depthOffset = 12;
        });

    ElevationSampler elevationSampler;
    elevationSampler.layer = map->layer<ElevationLayer>();

    if (!noFeatures)
    {
        auto pager = NodePager::create(Profile("spherical-mercator"), app.mapNode->srs());
        pager->minLevel = 14;
        pager->maxLevel = 14;
        pager->refinePolicy = NodePager::RefinePolicy::Replace;
        pager->pixelError = 256;

        pager->calculateBound = [&](const TileKey& key, const IOOptions& io)
            {
                auto ex = key.extent().transform(app.mapNode->srs());
                auto bs = ex.createWorldBoundingSphere(0, 0);

                if (elevationSampler.ok() && key.level > 1)
                {
                    auto resolutionX = elevationSampler.layer->resolution(key.level).first;
                    if (auto p = elevationSampler.clamp(ex.centroid(), resolutionX, io))
                    {
                        auto n = glm::normalize(bs.center);
                        bs.center += n * p.value().transform(ex.srs().geodeticSRS()).z;
                        bs.radius *= 1.01;
                    }
                }

                return bs;
            };

        pager->createPayload = [&](const TileKey& key, const IOOptions& io)
            {
                vsg::ref_ptr<EntityNode> entityNode;

                auto gdal = GDALFeatureSource::create();
                if (tilesDir.empty())
                {
                    gdal->uri = "MVT:https://readymap.org/readymap/mvt/osm/" + key.str() + ".pbf";
                }
                else
                {
                    auto path = std::filesystem::path(tilesDir) / (key.str() + ".pbf");
                    if (!std::filesystem::exists(path))
                        return NodePager::Payload::Ptr{};

                    gdal->uri = "MVT:" + path.string();
                }
                gdal->openOptions.emplace_back("CLIP=NO");

                auto status = gdal->open();
                if (status.failed())
                {
                    Log()->warn(status.error().message);
                    return NodePager::Payload::Ptr{};
                }

                std::vector<Feature> buildings, roads;

                gdal->each(io, [&](Feature&& f)
                    {
                        if (f.hasField("building") && f.geometry.type == Geometry::Type::Polygon)
                        {
                            if (!noBuildings)
                                buildings.emplace_back(std::move(f));
                        }

                        else if (f.field("highway") == "motorway" ||
                            f.field("highway") == "trunk" ||
                            f.field("highway") == "primary" ||
                            f.field("highway") == "secondary" ||
                            f.field("highway") == "tertiary")
                        {
                            if (!noRoads)
                                roads.emplace_back(std::move(f));
                        }
                    });

                FeatureBuilder builder;
                builder.clamper = elevationSampler.session(io);
                builder.clamper.level = key.level;

                if (!buildings.empty())
                {
                    entityNode = EntityNode::create(app.registry);

                    PolygonStyle style = app.registry.read().registry.get<PolygonStyle>(styleEntity);
                    PolygonGeometry geomTemp;
                    builder.buildPolygonGeometry(buildings, style, geomTemp);

                    app.registry.write([&](entt::registry& reg)
                        {
                            auto entity = reg.create();
                            auto& geom = reg.emplace<PolygonGeometry>(entity, std::move(geomTemp));
                            auto& style = reg.get<PolygonStyle>(styleEntity);
                            reg.emplace<rocky::Polygon>(entity, geom, style);
                            reg.emplace<Overlay>(entity).mode = overlayMode;
                            entityNode->entities.emplace_back(entity);
                        });
                }

                if (!roads.empty())
                {
                    if (!entityNode)
                        entityNode = EntityNode::create(app.registry);

                    LineStyle style = app.registry.read().registry.get<LineStyle>(styleEntity);
                    LineGeometry geomTemp;
                    builder.buildLineGeometry(roads, style, geomTemp);

                    app.registry.write([&](entt::registry& reg)
                        {
                            auto entity = reg.create();
                            auto& geom = reg.emplace<LineGeometry>(entity, geomTemp);
                            auto& style = reg.get<LineStyle>(styleEntity);
                            reg.emplace<Line>(entity, geom, style);
                            reg.emplace<Overlay>(entity).mode = overlayMode;
                            entityNode->entities.emplace_back(entity);
                        });
                }

                return NodePager::Payload::create(entityNode);
            };

        pager->initialize(app.vsgcontext);

        auto layer = NodeLayer::create(pager);
        layer->name = "MVT features";
        if (layer->open(app.io()).ok())
            map->add(layer);
    }

    GpuTimer gpuTimer;
    if (bench)
        gpuTimer.attach(window.commandGraph, window.vsgWindow);

    app.realize();

    if (auto manip = MapManipulator::get(window.view(0).vsgView))
    {
        Viewpoint vp;
        vp.name = city.name;
        vp.point = GeoPoint(SRS::WGS84, city.lon, city.lat);
        vp.range = Distance(city.rangeKm, Units::KILOMETERS);
        manip->setViewpoint(vp, std::chrono::duration<float>(0.0f));
    }

    if (!bench)
        return app.run();

    // Benchmark: settle, then time.
    app.renderContinuously = true;

    using clock = std::chrono::steady_clock;
    auto seconds = [](clock::duration d) { return std::chrono::duration<double>(d).count(); };

    auto t0 = clock::now();
    auto windowStart = t0;
    std::vector<double> windowGpu;
    std::vector<double> windowMeans;
    bool settled = false;

    while (!settled)
    {
        auto gpu = gpuTimer.next();
        if (!app.frame())
            return 1;

        if (gpu)
            windowGpu.push_back(*gpu);

        auto now = clock::now();
        if (seconds(now - windowStart) >= 1.0)
        {
            double mean = summarize(windowGpu).mean;
            windowMeans.push_back(mean);
            windowGpu.clear();
            windowStart = now;

            std::cerr << "[slug-bench] settling " << std::fixed << std::setprecision(1) << seconds(now - t0)
                << " s | gpu " << std::setprecision(3) << mean << " ms" << std::endl;

            // Stable = every one of the last `settle` one-second windows within 2% of the newest.
            auto need = static_cast<std::size_t>(std::max(1.0, settleSeconds));
            if (windowMeans.size() > need && mean > 0.0 && seconds(now - t0) >= minWaitSeconds)
            {
                settled = std::all_of(windowMeans.end() - need - 1, windowMeans.end(), [&](double m)
                    {
                        return std::abs(m - mean) <= 0.02 * mean;
                    });
            }

            if (!settled && seconds(now - t0) >= maxWaitSeconds)
            {
                std::cerr << "[slug-bench] WARNING: did not settle within " << maxWaitSeconds << " s" << std::endl;
                break;
            }
        }
    }

    double settleTime = seconds(clock::now() - t0);

    std::vector<double> gpuMs;
    std::vector<double> cpuMs;
    gpuMs.reserve(benchFrames);
    cpuMs.reserve(benchFrames);

    while (static_cast<int>(gpuMs.size()) < benchFrames)
    {
        auto gpu = gpuTimer.next();
        auto start = clock::now();
        if (!app.frame())
            return 1;
        cpuMs.push_back(seconds(clock::now() - start) * 1000.0);

        if (gpu)
            gpuMs.push_back(*gpu);
    }

    auto g = summarize(gpuMs);
    auto c = summarize(cpuMs);

    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << "[slug-bench] " << label
        << " | city " << city.name << " " << width << "x" << height << " range " << city.rangeKm << " km"
        << " | " << (noFeatures ? "no-features" : overlayMode == OverlayMode::Raster ? "raster" : "vector")
        << " strokes " << strokes << " fill-batch " << fillBatch
        << (noBuildings ? " no-buildings" : "") << (noRoads ? " no-roads" : "")
        << " | gpu ms mean " << g.mean << " median " << g.median << " p95 " << g.p95 << " min " << g.min
        << " | frame ms mean " << c.mean << " median " << c.median
        << " | frames " << gpuMs.size() << " settle " << std::setprecision(1) << settleTime << " s"
        << (settled ? "" : " UNSETTLED");

    std::cout << out.str() << std::endl;
    return 0;
}
