/**
 * rocky c++
 * Copyright 2024 Pelican Mapping
 * MIT License
 */
#include "EntityTransform.h"
#include "engine/TerrainEngine.h"
#include "engine/Utils.h"
#include <rocky/Horizon.h>

using namespace ROCKY_NAMESPACE;

EntityTransform::EntityTransform()
{
    // force creation of the first viewlocal data structure
    _viewlocal[0].dirty = true;
}

void
EntityTransform::setPosition(const EntityPosition& position_)
{
    if (position != position_)
    {
        position = position_;
        dirty();
    }
}

void
EntityTransform::dirty()
{
    for (auto& view : _viewlocal)
        view.dirty = true;
}

void
EntityTransform::accept(vsg::RecordTraversal& record) const
{
    // traverse the transform
    if (push(record, vsg::dmat4(1.0)))
    {
        vsg::Group::accept(record);
        pop(record);
    }
}

bool
EntityTransform::push(vsg::RecordTraversal& record, const vsg::dmat4& local_matrix) const
{
    auto state = record.getState();

    // update the view-local data if necessary:
    auto& view = _viewlocal[record.getState()->_commandBuffer->viewID];
    if (view.dirty || local_matrix != view.local_matrix)
    {
        GeoPoint wgs84Point;
        position.basePosition.transform(SRS::WGS84, wgs84Point);
        wgs84Point.z = position.altitude;
        TerrainEngine* terrainEngine;
        if (record.getValue("terrainengine", terrainEngine))
        {
            const auto& tilePager = terrainEngine->tiles;
            unsigned bestLod = 0;
            vsg::ref_ptr<TerrainTileNode> tileNode;
            for (const auto& [key, entry] : tilePager._tiles)
            {
                if (key.extent().contains(position.basePosition) && bestLod < key.levelOfDetail() && entry._tile->dataLoader.available())
                {
                    bestLod = key.levelOfDetail();
                    tileNode = entry._tile;
                }
            }
            if (tileNode)
            {
                GeoPoint heightfieldPoint;
                // pre-transform position in case Z matters e.g. ECEF
                if (position.basePosition.transform(tileNode->dataLoader.value().elevation.heightfield.srs(), heightfieldPoint))
                {
                    auto height = tileNode->dataLoader.value().elevation.heightfield.heightAtLocation(heightfieldPoint.x, heightfieldPoint.y);
                    Log()->info("Read height {:f}", height);
                    // todo: apply heighfield range
                    wgs84Point.z += height;
                }
            }
        }
        SRS worldSRS;
        if (record.getValue("worldsrs", worldSRS))
        {
            if (wgs84Point.transform(worldSRS, view.worldPos))
            {
                view.matrix =
                    to_vsg(worldSRS.localToWorldMatrix(glm::dvec3(view.worldPos.x, view.worldPos.y, view.worldPos.z))) *
                    local_matrix;
            }
        }

        view.local_matrix = local_matrix;
        view.dirty = false;
    }

    // horizon cull, if active:
    if (horizonCulling)
    {
        std::shared_ptr<Horizon> horizon;
        if (state->getValue("horizon", horizon))
        {
            if (!horizon->isVisible(view.matrix[3][0], view.matrix[3][1], view.matrix[3][2], bound.radius))
                return false;
        }
    }

    // replicates RecordTraversal::accept(MatrixTransform&):
    state->modelviewMatrixStack.push(state->modelviewMatrixStack.top() * view.matrix);
    state->dirty = true;
    state->pushFrustum();

    return true;
}

void
EntityTransform::pop(vsg::RecordTraversal& record) const
{
    auto state = record.getState();
    state->popFrustum();
    state->modelviewMatrixStack.pop();
    state->dirty = true;
}
