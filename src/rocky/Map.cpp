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
    construct({}, _instance.io());
}

Map::Map(const Instance& instance, const IOOptions& io) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct({}, io);
}

Map::Map(const Instance& instance, const std::string& JSON) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct(JSON, _instance.io());
}

Map::Map(const Instance& instance, const std::string& JSON, const IOOptions& io) :
    _instance(instance),
    _imageLayers(this),
    _elevationLayers(this)
{
    construct(JSON, io);
}

void
Map::construct(const std::string& JSON, const IOOptions& io)
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

    from_json(JSON, io);

    // set a default profile if neccesary.
    if (!profile().valid())
    {
        // call set_default so it doesn't serialize.
        _profile.set_default(Profile::GLOBAL_GEODETIC);
    }
}

Status
Map::from_json(const std::string& input, const IOOptions& io)
{
    auto j = parse_json(input);
    if (j.status.failed())
        return j.status;

    get_to(j, "name", _name);
    get_to(j, "profile", _profile);
    get_to(j, "profile_layer", _profileLayer);
    if (j.contains("layers")) {
        auto j_layers = j.at("layers");
        if (j_layers.is_array()) {
            for (auto& j_layer : j_layers) {
                std::string type;
                get_to(j_layer, "type", type);
                auto new_layer = Instance::createObject<Layer>(type, j_layer.dump(), io);
                if (new_layer) {
                    layers().add(new_layer);
                }
            }
        }
    }

    return Status_OK;
}

std::string
Map::to_json() const
{
    auto j = json::object();

    set(j, "name", _name);
    set(j, "profile", _profile);
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

Revision
Map::revision() const
{
    return _dataModelRevision;
}

void
Map::setProfile(const Profile& value)
{
    _profile = value;
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
    
    return result;
}

const SRS&
Map::srs() const
{
    static SRS emptySRS;
    return _profile.has_value() && _profile->valid() ? _profile->srs() : emptySRS;
}

void
Map::removeCallback(UID uid)
{
    onLayerAdded.remove(uid);
    onLayerRemoved.remove(uid);
    onLayerMoved.remove(uid);
}
