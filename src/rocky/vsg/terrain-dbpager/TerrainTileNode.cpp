/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileNode.h"
#include "TerrainEngine.h"
#include "GeometryPool.h"

#include <rocky/Map.h>
#include <rocky/TerrainTileModelFactory.h>

#include <vsg/nodes/QuadGroup.h>
#include <vsg/state/ViewDependentState.h>

using namespace ROCKY_NAMESPACE;

namespace
{
    /**
    * Node that applies custom culling logic to a TerrainTileNode.
    * 
    * - screen-space size (pixel size) culling
    * - bounding box culling
    * - horizon culling
    */
    class TerrainTileCuller : public vsg::Inherit<vsg::Node, TerrainTileCuller>
    {
    public:
        std::shared_ptr<TerrainEngine> engine;
        vsg::ref_ptr<TerrainTileNode> tile;

        void traverse(vsg::Visitor& visitor) override { tile->accept(visitor); }

        void traverse(vsg::ConstVisitor& visitor) const override { tile->accept(visitor); }

        void traverse(vsg::RecordTraversal& visitor) const override
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(tile && engine, void());

            if (tile->surface->isVisible(visitor))
            {
                if (!tile->filename.empty())
                {
                    auto& state = *visitor.getState();
                    auto& viewport = state._commandBuffer->viewDependentState->viewportData->at(0);
                    auto& settings = engine->settings;
                    tile->children[0].minimumScreenHeightRatio = (settings.tilePixelSize + settings.screenSpaceError) / viewport[3];
                }
                tile->accept(visitor);
            }
        }
    };


    struct TileCancelable : public Cancelable
    {
        vsg::ref_ptr<TerrainTileNode> tile;
        unsigned& frameCount;
        mutable bool already_canceled = false;

        TileCancelable(vsg::ref_ptr<TerrainTileNode> tile_, unsigned& frameCount_) :
            tile(tile_), frameCount(frameCount_) {}

        bool canceled() const override
        {
            if (!already_canceled)
                already_canceled = (tile && !tile->highResActive(frameCount));
            return already_canceled;
        }
    };
}

#if 0
void
TerrainTileNode::inheritFrom(vsg::ref_ptr<TerrainTileNode> parent)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(parent, void());

    renderModel = parent->renderModel; // copy render model
    renderModel.applyScaleBias(key.scaleBiasMatrix()); // and scale/bias

    // udpate proxy mesh and refresh bounds
    surface->setElevation(renderModel.elevation.image, renderModel.elevation.matrix);
    bound = surface->worldBoundingSphere;

    stategroup = parent->stategroup;
}
#endif

vsg::ref_ptr<vsg::Object>
TerrainTileNodeQuadReader::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    std::filesystem::path filename_path = filename.string();
    if (filename_path.extension() != ".rocky_terrain_tile_parent")
        return {};

    ROCKY_SOFT_ASSERT_AND_RETURN(options, {});

    auto tilekey_string = filename_path.stem().string();
    std::vector<std::string> tokens;
    util::StringTokenizer tokenizer(tilekey_string, tokens, ",");
    ROCKY_SOFT_ASSERT_AND_RETURN(tokens.size() == 3, {});

    unsigned z = std::stoi(tokens[0]);
    unsigned x = std::stoi(tokens[1]);
    unsigned y = std::stoi(tokens[2]);
    TileKey parent_key(z, x, y, Profile::GLOBAL_GEODETIC);    
    ROCKY_SOFT_ASSERT_AND_RETURN(parent_key.valid(), {});

    // pull the engine
    std::shared_ptr<TerrainEngine> engine;
    options->getValue("rocky.terrain_engine", engine);
    ROCKY_SOFT_ASSERT_AND_RETURN(engine != nullptr, {});

    // pull the parent tile
    auto parent_tile = engine->getCachedTile(parent_key);
    ROCKY_SOFT_ASSERT_AND_RETURN(parent_tile, {});

    Log()->info("read {}", parent_key.str());

    TileCancelable cancelable(parent_tile, frameCount);

    std::array<vsg::ref_ptr<TerrainTileNode>, 4> tiles;
    unsigned num_tiles_with_new_data = 0;

    // Attempt to create a quad of valid tiles.
    for (unsigned i = 0; i < 4 && !cancelable.canceled(); ++i)
    {
        auto child_key = parent_key.createChildKey(i);

#if 1
        tiles[i] = engine->createTile(child_key, cancelable);
        if (!tiles[i])
        {
            tiles[i] = engine->inheritTile(child_key, parent_tile, cancelable);
        }
        else
        {
            num_tiles_with_new_data++;
        }
#else
        tiles[i] = engine->inheritTile(child_key, parent_tile, cancelable);
        ROCKY_SOFT_ASSERT_AND_RETURN(tiles[i], {});

        bool got_data = engine->updateTile(tiles[i], cancelable);
        if (got_data)
        {
            ++num_tiles_with_new_data;
        }
#endif
    }
    
    if (!cancelable.canceled())
    {
        if (num_tiles_with_new_data == 0)
        {
            // no data was loaded, so disable the parent LOD mechanism to prevent
            // the pager from trying to load again.
            parent_tile->filename.clear();
            parent_tile->children[0].minimumScreenHeightRatio = FLT_MAX; // never show child
            parent_tile->children[1].minimumScreenHeightRatio = 0.0f; // always show child

            return vsg::Node::create();
        }

        else
        {
            // at least one subtile loaded, so let's build a quad
            auto quad = vsg::QuadGroup::create();

            for (unsigned i = 0; i < 4; ++i)
            {
                // add our custom culling logic:
                auto culler = TerrainTileCuller::create();
                culler->engine = engine;
                culler->tile = tiles[i];
                quad->children[i] = culler;
            }

            if (!cancelable.canceled())
            {
                return quad;
            }
        }
    }

    // tile was canceled, so return an error code
    Log()->info("Parent tile {} canceled", parent_key.str());
    return vsg::ReadError::create("[rocky.ignore] Tile canceled");
}
