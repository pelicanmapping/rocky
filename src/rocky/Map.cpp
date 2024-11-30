/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Map.h"
#include "ImageLayer.h"
#include "ElevationLayer.h"
#include "json.h"
#include <tuple>

using namespace ROCKY_NAMESPACE;

#define LC "[Map] "

Map::Map() :
    _imageLayers(this),
    _elevationLayers(this)
{
    //_profile.set_default(Profile::GLOBAL_GEODETIC);
}

Status
Map::from_json(const std::string& input, const IOOptions& io)
{
    auto j = parse_json(input);
    if (j.status.failed())
        return j.status;

    get_to(j, "name", _name);
    //get_to(j, "profile", _profile);
    //get_to(j, "profile_layer", _profileLayer);
    if (j.contains("layers")) {
        auto j_layers = j.at("layers");
        if (j_layers.is_array()) {
            for (auto& j_layer : j_layers) {
                std::string type;
                get_to(j_layer, "type", type);
                auto new_layer = ContextImpl::createObject<Layer>(type, j_layer.dump(), io);
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
    //set(j, "profile", _profile);
    //set(j, "profile_layer", _profileLayer);

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

//void
//Map::setProfile(const Profile& value)
//{
//    _profile = value;
//}
//
//const Profile&
//Map::profile() const
//{
//    return _profile;
//}
//
//const SRS&
//Map::srs() const
//{
//    static SRS emptySRS;
//    return _profile.has_value() && _profile->valid() ? _profile->srs() : emptySRS;
//}

void
Map::removeCallback(UID uid)
{
    onLayerAdded.remove(uid);
    onLayerRemoved.remove(uid);
    onLayerMoved.remove(uid);
}

void
Map::add(std::shared_ptr<Layer> layer)
{
    if (auto imageLayer = ImageLayer::cast(layer))
    {
        _imageLayers.add(imageLayer);
    }
    else if (auto elevLayer = ElevationLayer::cast(layer))
    {
        _elevationLayers.add(elevLayer);
    }
}

Status
Map::openAllLayers(const IOOptions& io)
{
    Status status;
    for (auto& layer : layers().all())
    {
        if (layer->openAutomatically() && !layer->isOpen())
        {
            auto layer_status = layer->open(io);
            if (layer_status.failed())
            {
                status = Status_GeneralError;
            }
        }
    }
    return status;
}
