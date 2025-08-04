/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Map.h"
#include "json.h"
#include "Context.h"

using namespace ROCKY_NAMESPACE;

#define LC "[Map] "

Result<>
Map::from_json(std::string_view input, const IOOptions& io)
{
    auto j = parse_json(input);
    if (j.status.failed())
        return j.status.error();

    get_to(j, "name", name);
    if (j.contains("layers")) {
        auto j_layers = j.at("layers");
        if (j_layers.is_array()) {
            _layers.reserve(j_layers.size());
            for (auto& j_layer : j_layers) {
                std::string type;
                get_to(j_layer, "type", type);
                auto new_layer = ContextImpl::createObject<Layer>(type, j_layer.dump(), io);
                if (new_layer) {
                    _layers.emplace_back(new_layer);
                }
            }
        }
    }

    return ResultVoidOK;
}

std::string
Map::to_json() const
{
    auto j = json::object();

    set(j, "name", name);

    auto layers_json = nlohmann::json::array();
    for (auto& layer :  _layers)
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

void
Map::setLayers(const Layers& layers)
{
    {
        std::unique_lock lock(_mutex);
        _layers = layers;
        ++_revision;
    }
    onLayersChanged.fire(this);
}

void
Map::setLayers(Layers&& layers) noexcept
{
    {
        std::unique_lock lock(_mutex);
        _layers = std::move(layers);
        ++_revision;
    }
    onLayersChanged.fire(this);
}

Revision
Map::revision() const
{
    return _revision;
}

void
Map::add(Layer::Ptr layer)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(layer, void());
    {
        std::unique_lock lock(_mutex);
        _layers.emplace_back(layer);
        ++_revision;
    }
    onLayersChanged.fire(this);
}

Result<>
Map::openAllLayers(const IOOptions& io)
{
    std::shared_lock lock(_mutex);

    Status status;
    for (auto& layer : _layers)
    {
        if (layer->openAutomatically && !layer->isOpen())
        {
            auto r = layer->open(io);
            if (r.failed())
            {
                status = r.error();
            }
        }
    }
    if (status.ok())
        return ResultVoidOK;
    else
        return status.error();
}
