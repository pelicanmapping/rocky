/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Map.h"
#include "VisibleLayer.h"
#include "json.h"
#include <tuple>

using namespace ROCKY_NAMESPACE;

#define LC "[Map] "

Map::Map(const Instance& instance) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct({}, _instance.ioOptions());
}

Map::Map(const Instance& instance, const IOOptions& io) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct({}, io);
}

Map::Map(const Instance& instance, const JSON& conf) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct(conf, _instance.ioOptions());
}

Map::Map(const Instance& instance, const JSON& conf, const IOOptions& io) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct(conf, io);
}

void
Map::construct(const JSON& conf, const IOOptions& io)
{
    // reset the revision:
    _dataModelRevision = 0;

    // Generate a UID.
    _uid = rocky::createUID();

#if 0
    // elevation sampling
    _elevationPool = ElevationPool::create(_instance);
    _elevationPool->setMap( this );
#endif

    auto j = parse_json(conf);
    get_to(j, "name", _name);
    get_to(j, "profile", _profile);
    get_to(j, "profile_layer", _profileLayer);
    if (j.contains("layers")) {
        auto j_layers = j.at("layers");
        if (j_layers.is_array()) {
            for (auto& j_layer : j_layers) {
                std::string type;
                get_to(j_layer, "type", type);
                auto new_layer = Instance::createObject<Layer>(type, j_layer.dump());
                if (new_layer) {
                    layers().add(new_layer);
                }
            }
        }
    }

    // set a default profile if neccesary.
    if (!profile().valid())
    {
        setProfile(Profile::GLOBAL_GEODETIC);
    }
}

JSON
Map::to_json() const
{
    auto j = json::object();
    set(j, "name", _name);
    set(j, "profile", profile());
    set(j, "profile_layer", _profileLayer);

    auto layers_json = nlohmann::json::array();
    for (auto& layer : layers().all())
    {
        auto layer_json = parse_json(layer->to_json());
        layers_json.push_back(layer_json);
    }

    if (layers_json.size() > 0)
    {
        j["layers"] = layers_json;
    }

    return j.dump();
}


#if 0
ElevationPool*
Map::getElevationPool() const
{
    return _elevationPool.get();
}
#endif

Revision
Map::revision() const
{
    return _dataModelRevision;
}

void
Map::setProfile(const Profile& value)
{
    bool notifyLayers = (!_profile.valid());

    if (value.valid())
    {
        _profile = value;
    }

#if 0
    // If we just set the profile, tell all our layers they are now added
    // to a valid map.
    if (_profile.valid() && notifyLayers)
    {
        for(auto& layer : _layers)
        {
            if (layer->isOpen())
            {
                layer->addedToMap(this);
            }
        }
    }
#endif
}

const Profile&
Map::profile() const
{
    return _profile;
}

std::set<std::string>
Map::attributions() const
{
    std::set<std::string> result;

    std::shared_lock lock(_mapDataMutex);

    for(auto& layer : _layers)
    {
        if (layer->isOpen())
        {
            auto visibleLayer = VisibleLayer::cast(layer);

            if (!visibleLayer || visibleLayer->visible())
            {
                std::string attribution = layer->attribution();
                if (!attribution.empty())
                {
                    result.insert(attribution);
                }
            }
        }
    }
    
    return std::move(result);
}

const SRS&
Map::srs() const
{
    static SRS emptySRS;
    return _profile.valid() ? _profile.srs() : emptySRS;
}

void
Map::removeCallback(UID uid)
{
    onLayerAdded.functions.erase(uid);
    onLayerRemoved.functions.erase(uid);
    onLayerMoved.functions.erase(uid);
}
