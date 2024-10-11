/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "TileLayer.h"
#include "TileKey.h"
#include "Map.h"
#include "rtree.h"
#include "json.h"

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
    construct(JSON());
}

TileLayer::TileLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
TileLayer::construct(const JSON& conf)
{
    const auto j = parse_json(conf);
    get_to(j, "max_level", _maxLevel);
    get_to(j, "max_resolution", _maxResolution);
    get_to(j, "max_data_level", _maxDataLevel);
    get_to(j, "min_level", _minLevel);
    get_to(j, "tile_size", _tileSize);
    get_to(j, "profile", _originalProfile);        
}

JSON
TileLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "max_level", _maxLevel);
    set(j, "max_resolution", _maxResolution);
    set(j, "max_data_level", _maxDataLevel);
    set(j, "min_level", _minLevel);
    set(j, "tile_size", _tileSize);
    set(j, "profile", _originalProfile);
    return j.dump();
}

void TileLayer::setMinLevel(unsigned value) {
    _minLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::minLevel() const {
    return _minLevel;
}
void TileLayer::setMaxLevel(unsigned value) {
    _maxLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::maxLevel() const {
    return _maxLevel;
}
void TileLayer::setMinResolution(double value) {
    _minResolution = value, _reopenRequired = true;
}
const optional<double>& TileLayer::minResolution() const {
    return _minResolution;
}
void TileLayer::setMaxResolution(double value) {
    _maxResolution = value, _reopenRequired = true;
}
const optional<double>& TileLayer::maxResolution() const {
    return _maxResolution;
}
void TileLayer::setMaxDataLevel(unsigned value) {
    _maxDataLevel = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::maxDataLevel() const {
    return _maxDataLevel;
}
void TileLayer::setTileSize(unsigned value) {
    _tileSize = value, _reopenRequired = true;
}
const optional<unsigned>& TileLayer::tileSize() const {
    return _tileSize;
}

Status
TileLayer::openImplementation(const IOOptions& io)
{
    auto result = super::openImplementation(io);
    if (result.ok())
    {
        if (_originalProfile.has_value())
            setProfile(_originalProfile);
    }
    return result;
}

void
TileLayer::closeImplementation()
{
    _runtimeProfile = _originalProfile;

    _dataExtents.clear();
    _dataExtentsUnion = {};
    if (_dataExtentsIndex)
    {
        _dataExtentsIndex = nullptr;
    }

    super::closeImplementation();
}

const Profile&
TileLayer::profile() const
{
    return _runtimeProfile;
}

void
TileLayer::setPermanentProfile(const Profile& profile)
{
    _originalProfile = profile;
    setProfile(profile);
}

void
TileLayer::setProfile(const Profile& profile)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(!isOpen(), void(), "ILLEGAL: cannot set profile after layer is open");

    _runtimeProfile = profile;

    Log()->debug("Layer \"{}\" profile set to {}", name(), _runtimeProfile->toReadableString());
}

bool
TileLayer::isKeyInLegalRange(const TileKey& key) const
{
    if ( !key.valid() )
    {
        return false;
    }

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();


    // First check the key against the min/max level limits, it they are set.
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return false;
    }

    // Next check the maxDataLevel if that is set.
    if (_maxDataLevel.has_value() && localLOD > _maxDataLevel)
    {
        return false;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (profile().valid())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.extent().width() / (double)tileSize();
            double resLayer = SRS::transformUnits(resKey, key.profile().srs(), profile().srs(), Angle());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            {
                return false;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            {
                return false;
            }
        }
    }

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
    _dataExtents = dataExtents;

    // rebuild the union:
    _dataExtentsUnion = {};
    if (_dataExtents.size() > 0)
    {
        _dataExtentsUnion = _dataExtents[0];
        for (unsigned int i = 1; i < _dataExtents.size(); i++)
        {
            _dataExtentsUnion.expandToInclude(_dataExtents[i]);

            if (_dataExtents[i].minLevel().has_value())
                _dataExtentsUnion.minLevel() = std::min(_dataExtentsUnion.minLevel().value(), _dataExtents[i].minLevel().value());

            if (_dataExtents[i].maxLevel().has_value())
                _dataExtentsUnion.maxLevel() = std::max(_dataExtentsUnion.maxLevel().value(), _dataExtents[i].maxLevel().value());
        }
    }

    // rebuild the index:
    double a_min[2], a_max[2];
    _dataExtentsIndex = std::make_shared<DataExtentsIndex>(); // new DataExtentsIndex();

    for (auto de = _dataExtents.begin(); de != _dataExtents.end(); ++de)
    {
        // Build the index in the SRS of this layer
        GeoExtent extentInLayerSRS = profile().clampAndTransformExtent(*de);

        if (extentInLayerSRS.srs().isGeodetic() && extentInLayerSRS.crossesAntimeridian())
        {
            GeoExtent west, east;
            extentInLayerSRS.splitAcrossAntimeridian(west, east);
            if (west.valid())
            {
                DataExtent new_de(west);
                new_de.minLevel() = de->minLevel();
                new_de.maxLevel() = de->maxLevel();
                a_min[0] = new_de.xmin(), a_min[1] = new_de.ymin();
                a_max[0] = new_de.xmax(), a_max[1] = new_de.ymax();
                _dataExtentsIndex->Insert(a_min, a_max, new_de);
            }
            if (east.valid())
            {
                DataExtent new_de(east);
                new_de.minLevel() = de->minLevel();
                new_de.maxLevel() = de->maxLevel();
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
    if (_crop.has_value())
    {
        return _crop.value();
    }
    else
    {
        return dataExtentsUnion();
    }
}

TileKey
TileLayer::bestAvailableTileKey(const TileKey& key) const
{
    // trivial reject
    if (!key.valid())
    {
        return TileKey::INVALID;
    }

    unsigned MDL = _maxDataLevel;

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();

    // Check against level extrema:
    if ((_maxLevel.has_value() && localLOD > _maxLevel) ||
        (_minLevel.has_value() && localLOD < _minLevel))
    {
        return TileKey::INVALID;
    }

    // Next, check against resolution limits (based on the source tile size).
    if (_minResolution.has_value() || _maxResolution.has_value())
    {
        if (profile().valid())
        {
            // calculate the resolution in the layer's profile, which can
            // be different that the key's profile.
            double resKey = key.extent().width() / (double)tileSize();
            double resLayer = SRS::transformUnits(resKey, key.profile().srs(), profile().srs(), Angle());

            if (_maxResolution.has_value() && _maxResolution > resLayer)
            {
                return TileKey::INVALID;
            }

            if (_minResolution.has_value() && _minResolution < resLayer)
            {
                return TileKey::INVALID;
            }
        }
    }

    // If we have no data extents available, just return the MDL-limited input key.
    if (_dataExtents.size() == 0)
    {
        return localLOD > MDL ? key.createAncestorKey(MDL) : key;
    }

    // Reject if the extents don't overlap at all.
    // (Note: this does not consider min/max levels, only spatial extents)
    if (!dataExtentsUnion().intersects(key.extent()))
    {
        return TileKey::INVALID;
    }

    // Consider a user-crop:
    if (_crop.has_value() && !_crop->intersects(key.extent()))
    {
        return TileKey::INVALID;
    }

    bool intersects = false;
    unsigned highestLOD = 0u;
    double a_min[2], a_max[2];

    // Transform the key extent to the SRS of this layer to do the index search
    GeoExtent keyExtentInLayerSRS = profile().clampAndTransformExtent(key.extent());

    a_min[0] = keyExtentInLayerSRS.xmin(); a_min[1] = keyExtentInLayerSRS.ymin();
    a_max[0] = keyExtentInLayerSRS.xmax(); a_max[1] = keyExtentInLayerSRS.ymax();

    TileKey bestKey;
    _dataExtentsIndex->Search(a_min, a_max, [&](const DataExtent& de)
        {
            // check that the extent isn't higher-resolution than our key:
            if (!de.minLevel().has_value() || localLOD >= (int)de.minLevel().value())
            {
                // Got an intersetion; now test the LODs:
                intersects = true;

                // If the maxLevel is not set, there's not enough information
                // so just assume our key might be good.
                if (!de.maxLevel().has_value())
                {
                    bestKey = localLOD > MDL ? key.createAncestorKey(MDL) : key;
                    return false; //Stop searching, we've found a key
                }

                // Is our key at a lower or equal LOD than the max key in this extent?
                // If so, our key is good.
                else if (localLOD <= (int)de.maxLevel().value())
                {
                    bestKey = localLOD > MDL ? key.createAncestorKey(MDL) : key;
                    return false; //Stop searching, we've found a key
                }

                // otherwise, record the highest encountered LOD that
                // intersects our key.
                else if (de.maxLevel().value() > highestLOD)
                {
                    highestLOD = de.maxLevel().value();
                }
            }
            return true; // Continue searching
        });

    if (bestKey.valid())
    {
        return bestKey;
    }

    if ( intersects )
    {
        // for a normal dataset, dataset max takes priority over MDL.
        unsigned maxAvailableLOD = std::min(highestLOD, MDL);
        return key.createAncestorKey(std::min(key.levelOfDetail(), maxAvailableLOD));
    }

    return TileKey::INVALID;
}

bool
TileLayer::intersects(const TileKey& key) const
{
    double a_min[2], a_max[2];

    // We must use the equivalent lod b/c the input key can be in any profile.
    unsigned localLOD = profile().valid() ?
        profile().getEquivalentLOD(key.profile(), key.levelOfDetail()) :
        key.levelOfDetail();

    // Transform the key extent to the SRS of this layer to do the index search
    GeoExtent keyExtentInLayerSRS = profile().clampAndTransformExtent(key.extent());

    a_min[0] = keyExtentInLayerSRS.xmin(); a_min[1] = keyExtentInLayerSRS.ymin();
    a_max[0] = keyExtentInLayerSRS.xmax(); a_max[1] = keyExtentInLayerSRS.ymax();

    return _dataExtentsIndex->Intersects(a_min, a_max);
}

bool
TileLayer::mayHaveData(const TileKey& key) const
{
    return (key == bestAvailableTileKey(key));
}
