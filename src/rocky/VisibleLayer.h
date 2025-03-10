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
        //! Whether to draw this layer.
        option<bool> visible = true;

        //! Opacity with which to draw this layer
        option<float> opacity = 1.0f;

        //! Serialize
        std::string to_json() const override;

    protected: // Layer

        VisibleLayer();

        VisibleLayer(const JSON&);

    private:

        void construct(const JSON&);

        option<bool> _debugView = false;
    };

} // namespace VisibleLayer
