/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileLayer.h"
#include "TileKey.h"
#include "json.h"
#include "rtree.h"
#include "GeoImage.h"

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
    get_to(j, "maxLevel", maxLevel);
    get_to(j, "maxDataLevel", maxDataLevel);
    get_to(j, "minLevel", minLevel);
    get_to(j, "tileSize", tileSize);
    get_to(j, "profile", _originalProfile);        
}

std::string
TileLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "maxLevel", maxLevel);
    set(j, "maxDataLevel", maxDataLevel);
    set(j, "minLevel", minLevel);
    set(j, "tileSize", tileSize);
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
    _dataExtentsIndex = std::make_shared<DataExtentsIndex>();

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

    // find the corresponding keys in the local profile:
    auto localKeys = key.intersectingKeys(profile);
    if (localKeys.empty())
        return {};

    auto localLevel = localKeys.front().level;

    if (minLevel.has_value() && localLevel < minLevel)
        return {};

    if (maxLevel.has_value() && localLevel > maxLevel)
        return {};

    // union the local key extents
    GeoExtent localExtent;
    for (auto& localKey : localKeys)
        localExtent.expandToInclude(localKey.extent());

    // account for a crop:
    if (crop.has_value() && !localExtent.intersects(crop.value()))
        return {};

    // no extents? Just return the input key
    if (_dataExtents.empty())
        return key;

    // now we will search the spatial index to find the BEST local level available
    // for the extents containing the intersecting keys.
    unsigned bestLocalLevel = 0;

    auto findBestLocalLevel = [&](const DataExtent& de)
        {
            auto localMinLevel = de.minLevel.has_value() ? de.minLevel.value() : 0;
            auto localMaxLevel = de.maxLevel.has_value() ? std::min(de.maxLevel.value(), localLevel) : localLevel;

            if (localLevel >= localMinLevel)
                bestLocalLevel = std::max(bestLocalLevel, localMaxLevel);

            return bestLocalLevel < localLevel ? RTREE_KEEP_SEARCHING : RTREE_STOP_SEARCHING;
        };

    constexpr double EPS = 1e-12;
    double a_min[2] = { localExtent.xmin(), localExtent.ymin() };
    double a_max[2] = { localExtent.xmax() - EPS, localExtent.ymax() - EPS };
    int intersections = _dataExtentsIndex->Search(a_min, a_max, findBestLocalLevel);

    // no intersections in the spatial index == no data.
    if (intersections == 0)
        return {};

    // Calculate the delta between our best local level, and the profile of the input key.
    // It might be negative.
    int delta = (int)localLevel - (int)key.level;
    int bestLevel = std::max((int)bestLocalLevel - delta, 0);

    return key.createAncestorKey((unsigned)bestLevel);
}

bool
TileLayer::intersects(const TileKey& key) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(profile.valid() && key.valid(), false);

    // find the corresponding keys in the local profile:
    auto localKeys = key.intersectingKeys(profile);
    if (localKeys.empty())
        return false;

    auto localLevel = localKeys.front().level;

    if (minLevel.has_value() && localLevel < minLevel)
        return false;

    if (maxLevel.has_value() && localLevel > maxLevel)
        return false;

    // union the local key extents
    GeoExtent localExtent;
    for (auto& localKey : localKeys)
        localExtent.expandToInclude(localKey.extent());

    // first check the union of our data extents, if we have any
    if (_dataExtentsUnion.valid() && !_dataExtentsUnion.intersects(localExtent))
        return {};

    // account for a crop:
    if (crop.has_value() && !localExtent.intersects(crop.value()))
        return false;

    // no data extents? We did the best we could:
    if (_dataExtents.empty())
        return true;

    // and search the spatial index.
    constexpr double EPS = 1e-10;
    double a_min[2] = { localExtent.xmin(), localExtent.ymin() };
    double a_max[2] = { localExtent.xmax() - EPS, localExtent.ymax() - EPS };
    bool hit = false;
    _dataExtentsIndex->Search(a_min, a_max, [&](const DataExtent& de) {
        if (de.minLevel.has_value() && localLevel < de.minLevel.value()) return RTREE_KEEP_SEARCHING; // skip this extent
        if (de.maxLevel.has_value() && localLevel > de.maxLevel.value()) return RTREE_KEEP_SEARCHING; // skip this extent
        hit = true; // we have at least one extent that intersects
        return RTREE_STOP_SEARCHING; }); // just check if we have any extents that intersect

    return hit;
}

bool
TileLayer::mayHaveData(const TileKey& key) const
{
    return (key == bestAvailableTileKey(key));
}

Result<GeoImage>
TileLayer::getOrCreateTile(const TileKey& key, const IOOptions& io, std::function<Result<GeoImage>()>&& create) const
{
    if (io.services().residentImageCache)
    {
        auto cacheKey = key.str() + '-' + std::to_string(key.profile.hash()) + '-' + std::to_string(uid()) + "-" + std::to_string(revision());

        auto cached = io.services().residentImageCache->get(cacheKey);

        // note: "first" = Image, "second" = GeoExtent
        if (cached.has_value() && cached.value().first && cached.value().second)
        {
            return GeoImage(cached.value().first, cached.value().second);
        }

        auto r = create();

        if (r.ok() && r.value().image())
        {
            io.services().residentImageCache->put(cacheKey, r.value().image(), r.value().extent());
        }

        return r;
    }
    else
    {
        return create();
    }
}