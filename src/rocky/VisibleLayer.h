/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Layer.h>

namespace ROCKY_NAMESPACE
{
    //! Base class for a layer supporting visibility and opacity controls.
    class ROCKY_EXPORT VisibleLayer : public Inherit<Layer, VisibleLayer>
    {
    public:
        //! Opacity with which to draw this layer
        option<float> opacity = 1.0f;

        //! Serialize
        std::string to_json() const override;

    protected: // Layer

        //! Construct a layer (from a subclass)
        VisibleLayer();

        //! Deserialize a layer (from a subclass)
        VisibleLayer(const JSON&);

    private:

        void construct(const JSON&);
    };

} // namespace VisibleLayer
