/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TerrainTileModel.h"
#include "Layer.h"
#include "Image.h"
#include "Heightfield.h"

using namespace ROCKY_NAMESPACE;

#undef  LC
#define LC "[TerrainTileModel] "

#if 0
GeoImage*
TerrainTileModel::getColorLayerImage(UID layerUID) const
{
    for (auto& colorLayer : colorLayers)
        if (colorLayer.layer && colorLayer.layer->getUID() == layerUID)
            return colorLayer.image;

    return nullptr;
}

const fmat4&
TerrainTileModel::getColorLayerMatrix(UID layerUID) const
{
    static fmat4 s_identity(1.0f);

    for (auto& colorLayer : colorLayers)
        if (colorLayer.layer && colorLayer.layer->getUID() == layerUID)
            return colorLayer.matrix;

    return s_identity;
}
#endif