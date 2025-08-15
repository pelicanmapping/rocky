/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileLayer.h"
#include "TileKey.h"
#include "json.h"
#include "rtree.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::util;

#define LC "[TileLayer] \"" << name().value() << "\" "

struct ROCKY_NAMESPACE::TileLayer::DataExtentsIndex : public RTree<DataExtent, double, 2>
{
    //nop
};

TileLayer::TileLayer() :
    super()
{
    construct({});
}

TileLayer::TileLayer(std::string_view conf) :
    super(conf)
{
    construct(conf);
}

void
TileLayer::construct(std::string_view conf)
{
    const auto j = parse_json(conf);
    get_to(j, "max_level", maxLevel);
    get_to(j, "max_data_level", maxDataLevel);
    get_to(j, "min_level", minLevel);
    get_to(j, "tile_size", tileSize);
    get_to(j, "profile", _originalProfile);        
}

std::string
TileLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "max_level", maxLevel);
    set(j, "max_data_level", maxDataLevel);
    set(j, "min_level", minLevel);
    set(j, "tile_size", tileSize);
    set(j, "profile", _originalProfile);
    return j.dump();
}

Result<>
TileLayer::openImplementation(const IOOptions& io)
{
    auto result = super::openImplementation(io);
    if (result.ok())
    {
        if (_originalProfile.has_value())
        {
            profile = _originalProfile;
        }
    }
    return result;
}

void
TileLayer::closeImplementation()
{
    profile = {};
    _dataExtents.clear();
    _dataExtentsUnion = {};
    if (_dataExtentsIndex)
    {
        _dataExtentsIndex = nullptr;
    }

    super::closeImplementation();
}

void
TileLayer::setPermanentProfile(const Profile& perm_profile)
{
    _originalProfile = profile;
    profile = perm_profile;
}

bool
TileLayer::isKeyInConfiguredRange(const TileKey& in_key) const
{
    if (!in_key.valid() || !profile.valid())
        return false;

    auto localKeys = in_key.intersectingKeys(profile);
    if (localKeys.empty())
        return false;

    if (minLevel.has_value() && localKeys.front().level < minLevel)
        return false;

    if (maxLevel.has_value() && localKeys.front().level > maxLevel)
        return false;

    return true;
}

const DataExtentList&
TileLayer::dataExtents() const
{
    return _dataExtents;
}

void
TileLayer::setDataExtents(const DataExtentList& dataExtents)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid(), void());

    _dataExtents = dataExtents;

    // rebuild the union:
    _dataExtentsUnion = {};
    if (_dataExtents.size() > 0)
    {
        _dataExtentsUnion = _dataExtents[0];
        for (unsigned int i = 1; i < _dataExtents.size(); i++)
        {
            _dataExtentsUnion.expandToInclude(_dataExtents[i]);

            if (_dataExtents[i].minLevel.has_value())
                _dataExtentsUnion.minLevel = std::min(_dataExtentsUnion.minLevel.value(), _dataExtents[i].minLevel.value());

            if (_dataExtents[i].maxLevel.has_value())
                _dataExtentsUnion.maxLevel = std::max(_dataExtentsUnion.maxLevel.value(), _dataExtents[i].maxLevel.value());
        }
    }

    // rebuild the index:
    double a_min[2], a_max[2];
    _dataExtentsIndex = std::make_shared<DataExtentsIndex>(); // new DataExtentsIndex();

    for (auto de = _dataExtents.begin(); de != _dataExtents.end(); ++de)
    {
        // Build the index in the SRS of this layer
        GeoExtent extentInLayerSRS = de->transform(profile.srs());

        if (extentInLayerSRS.srs().isGeodetic() && extentInLayerSRS.crossesAntimeridian())
        {
            GeoExtent west, east;
            extentInLayerSRS.splitAcrossAntimeridian(west, east);
            if (west.valid())
            {
                DataExtent new_de(west);
                new_de.minLevel = de->minLevel;
                new_de.maxLevel = de->maxLevel;
                a_min[0] = new_de.xmin(), a_min[1] = new_de.ymin();
                a_max[0] = new_de.xmax(), a_max[1] = new_de.ymax();
                _dataExtentsIndex->Insert(a_min, a_max, new_de);
            }
            if (east.valid())
            {
                DataExtent new_de(east);
                new_de.minLevel = de->minLevel;
                new_de.maxLevel = de->maxLevel;
                a_min[0] = new_de.xmin(), a_min[1] = new_de.ymin();
                a_max[0] = new_de.xmax(), a_max[1] = new_de.ymax();
                _dataExtentsIndex->Insert(a_min, a_max, new_de);
            }
        }
        else
        {
            a_min[0] = extentInLayerSRS.xmin(), a_min[1] = extentInLayerSRS.ymin();
            a_max[0] = extentInLayerSRS.xmax(), a_max[1] = extentInLayerSRS.ymax();
            _dataExtentsIndex->Insert(a_min, a_max, *de);
        }
    }
}

const DataExtent&
TileLayer::dataExtentsUnion() const
{
    return _dataExtentsUnion;
}

const GeoExtent&
TileLayer::extent() const
{
    if (crop.has_value())
    {
        return crop.value();
    }
    else
    {
        return dataExtentsUnion();
    }
}

TileKey
TileLayer::bestAvailableTileKey(const TileKey& key) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid() && key.valid(), {});

    // trivial reject
    if (!key.valid())
        return {};

    // find the corresponding keys in the local profile:
    auto localKeys = key.intersectingKeys(profile);
    if (localKeys.empty())
        return {};

    auto& firstKey = localKeys.front();
    auto localLevel = firstKey.level;

    auto effectiveMinLevel = std::max(firstKey.level, minLevel.value_or(0)); 
    if (effectiveMinLevel > firstKey.level)
        return {}; // the key is below the minimum level

    auto effectiveMaxLevel = std::min(firstKey.level, maxLevel.value_or(~0));

    // union the local key extents
    GeoExtent localExtent;
    for (auto& localKey : localKeys)
        localExtent.expandToInclude(localKey.extent());

    // coarse intersection check:    
    if (extent().valid() && !extent().intersects(localExtent))
        return {};

    // no extents? Just return the input key
    if (_dataExtents.empty())
        return key;

    bool intersectionFound = false;
    unsigned highestLevelFound = 0u;
    unsigned bestLevel = ~0;

    auto checkDE = [&](const DataExtent& de)
        {
            if (!de.minLevel.has_value() || localLevel >= (int)de.minLevel.value())
            {
                intersectionFound = true;

                // If the maxLevel is not set, there's not enough information
                // so just assume our key might be good.
                if (!de.maxLevel.has_value())
                {
                    bestLevel = std::min(localLevel, effectiveMaxLevel);
                    return RTREE_STOP_SEARCHING; // Stop searching, we've found a key
                }

                // Is our key at a lower or equal LOD than the max key in this extent?
                // If so, our key is good.
                else if (localLevel <= (int)de.maxLevel.value())
                {
                    bestLevel = std::min(localLevel, effectiveMaxLevel);
                    return RTREE_STOP_SEARCHING; // Stop searching, we've found a key
                }

                // otherwise, record the highest encountered LOD that
                // intersects our key.
                else if (de.maxLevel.value() > highestLevelFound)
                {
                    highestLevelFound = de.maxLevel.value();
                }
            }
            return RTREE_KEEP_SEARCHING; // Continue searching
        };

    constexpr double EPS = 1e-10;
    double a_min[2] = { localExtent.xmin(), localExtent.ymin() };
    double a_max[2] = { localExtent.xmax() - EPS, localExtent.ymax() - EPS };
    _dataExtentsIndex->Search(a_min, a_max, checkDE);

    int delta;
    if (bestLevel < ~0)
    {
        delta = firstKey.level - bestLevel;
    }
    else if (intersectionFound)
    {
        // for a normal dataset, dataset max takes priority over MDL.
        unsigned maxAvailableLevel = std::min(highestLevelFound, effectiveMaxLevel);
        delta = firstKey.level - std::min(firstKey.level, maxAvailableLevel);
    }
    else return {};

    return key.createAncestorKey(std::max(0, (int)key.level + delta));
}

bool
TileLayer::intersects(const TileKey& key) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid() && key.valid(), false);

    // find the corresponding keys in the local profile:
    auto localKeys = key.intersectingKeys(profile);
    if (localKeys.empty())
        return false;

    if (minLevel.has_value() && localKeys.front().level < minLevel)
        return false;

    if (maxLevel.has_value() && localKeys.front().level > maxLevel)
        return false;

    // union the local key extents
    GeoExtent localExtent;
    for (auto& localKey : localKeys)
        localExtent.expandToInclude(localKey.extent());

    // account for a crop:
    if (crop.has_value() && !localExtent.intersects(crop.value()))
        return false;

    // and search the spatial index.
    constexpr double EPS = 1e-10;
    double a_min[2] = { localExtent.xmin(), localExtent.ymin() };
    double a_max[2] = { localExtent.xmax() - EPS, localExtent.ymax() - EPS };
    auto hits = _dataExtentsIndex->Search(a_min, a_max, [](const DataExtent& de) {
        return false; }); // just check if we have any extents that intersect

    return hits > 0;
}

bool
TileLayer::mayHaveData(const TileKey& key) const
{
    return (key == bestAvailableTileKey(key));
}
