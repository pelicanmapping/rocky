/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Map.h>

 //! optional property macro for referencing another layer
#define ROCKY_OPTION_LAYER(TYPE, NAME) \
    private: \
        LayerReference< TYPE > _layerRef_ ## NAME ; \
    public: \
        optional< TYPE ::Options >& NAME ## EmbeddedOptions () { return NAME ().embeddedOptions(); } \
        const optional< TYPE ::Options >& NAME ## EmbeddedOptions () const { return NAME ().embeddedOptions(); } \
        optional< std::string >& NAME ## LayerName () { return NAME ().externalLayerName() ; } \
        const optional< std::string >& NAME ## LayerName () const { return NAME ().externalLayerName() ; } \
        LayerReference< TYPE >& NAME () { return _layerRef_ ## NAME ; } \
        const LayerReference< TYPE >& NAME () const { return _layerRef_ ## NAME ; }

namespace rocky
{
    /**
     * Helper class for Layers that reference other layers.
     */
    template<typename T>
    class LayerReference
    {
    public:
        typedef typename T::Options TypedOptions;

        LayerReference() { }

        //! User can call this to set the layer by hand (instead of finding it
        //! in the map or in an embedded options structure)
        void setLayer(shared_ptr<T> layer) 
        {
            _layer = layer;
        }

        //! Contained layer object
        shared_ptr<T> getLayer() const 
        {
            return _layer;
        }

        //! Whether the user called setLayer to establish the reference
        //! (as opposed to finding it in an embedded options or in the map)
        bool isSetByUser() const 
        {
            return
                _layer &&
                !_embeddedOptions.has_value()
                && !_externalLayerName.has_value();
        }

        //! open the layer pointed to in the reference and return a status code
        Status open(const IOOptions* io)
        {
            if (_embeddedOptions.has_value())
            {
                shared_ptr<Layer> layer = Layer::create(_embeddedOptions.get());
                shared_ptr<T> typedLayer = std::dynamic_pointer_cast<T>(layer);
                if (typedLayer)
                {
                    const Status& layerStatus = typedLayer->open(io);
                    if (layerStatus.failed())
                    {
                        return layerStatus;
                    }
                    _layer = typedLayer;
                }
            }
            else if (_layer && !_layer->isOpen())
            {
                const Status& layerStatus = _layer->open(io);
                if (layerStatus.failed())
                {
                    return layerStatus;
                }
            }
            return STATUS_OK;
        }

        void close()
        {
            _layer = nullptr;
        }

        //! Find a layer in the map and set this reference to point at it 
        void addedToMap(const Map* map, const IOOptions* io)
        {
            if (!getLayer() && _externalLayerName.has_value())
            {
                shared_ptr<Layer> = map->getLayerByName<T>(_externalLayerName.get());
                if (layer)
                {
                    _layer = layer;

                    if (!layer->isOpen())
                    {
                        layer->open(io);
                    }
                }
            }
            else if (getLayer() && _embeddedOptions.has_value())
            {
                _layer->addedToMap(map, io);
            }
        }

        //! If this reference was set by findInMap, release it.
        void removedFromMap(shared_ptr<Map> map)
        {
            if (map && _layer)
            {
                if (_embeddedOptions.has_value())
                {
                    _layer->removedFromMap(map);
                }

                // Do not set _layer to nullptr. It may still be in use
                // and this is none of the Map's business.
            }
        }

        //! Get the layer ref from either a name or embedded option
        void get(const Config& conf, const std::string& tag)
        {
            // first try to store the name of another layer:
            conf.get(tag, _externalLayerName);

            if (!_externalLayerName.has_value())
            {
                // next try to find a child called (tag) and try to make the layer
                // from it's children:
                if (conf.hasChild(tag) && conf.child(tag).children().size() >= 1)
                {
                    const Config& tag_content = *conf.child(tag).children().begin();
                    {
                        osg::ref_ptr<Layer> layer = Layer::create(tag_content);
                        if (layer && std::dynamic_pointer_cast<T>(layer))
                        {
                            _embeddedOptions = TypedOptions(tag_content);
                        }
                    }
                }

                // failing that, try each child of the config.
                if (!_embeddedOptions.has_value())
                {
                    for(auto& conf : conf.children())
                    {
                        shared_ptr<Layer> layer = Layer::create(conf);
                        if (layer && std::dynamic_pointer_cast<T>(layer))
                        {
                            _embeddedOptions = TypedOptions(conf);
                            break;
                        }
                    }
                }
            }
        }

        //! Set the layer ref options in the config
        void set(Config& conf, const std::string& tag) const
        {
            if (_externalLayerName.has_value())
            {
                conf.set(tag, _externalLayerName);
            }
            else if (_embeddedOptions.has_value())
            {
                conf.set(_embeddedOptions->getConfig());
            }
            else if (isSetByUser()) // should be true
            {
                conf.add(_layer->getConfig());
            }
        }

        optional<TypedOptions>& embeddedOptions() { return _embeddedOptions; }
        const optional<TypedOptions>& embeddedOptions() const { return _embeddedOptions; }

        optional<std::string>& externalLayerName() { return _externalLayerName; }
        const optional<std::string>& externalLayerName() const { return _externalLayerName; }


    private:
        shared_ptr<Layer> _layer;
        optional<TypedOptions> _embeddedOptions;
        optional<std::string> _externalLayerName;
    };
}

