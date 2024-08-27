/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Layer.h>

namespace ROCKY_NAMESPACE
{
    enum ColorBlending
    {
        BLEND_INTERPOLATE,
        BLEND_MODULATE
    };

    //! Base class for a layer supporting visibility and opacity controls.
    class ROCKY_EXPORT VisibleLayer : public Inherit<Layer, VisibleLayer>
    {
    public:
        //! Whether to draw this layer.
        inline void setVisible(bool value);
        inline const optional<bool>& visible() const;

        //! Opacity with which to draw this layer
        inline void setOpacity(float value);
        inline const optional<float>& opacity() const;

        //! Serialize
        std::string to_json() const override;

    protected: // Layer

        VisibleLayer();

        VisibleLayer(const JSON&);

    private:

        void construct(const JSON&);

        optional<bool> _visible = true;
        optional<float> _opacity = 1.0f;
        optional<bool> _debugView = false;
    };


    // inlines
    void VisibleLayer::setVisible(bool value) {
        _visible = value;
    }
    const optional<bool>& VisibleLayer::visible() const {
        return _visible;
    }

    //! Opacity with which to draw this layer
    void VisibleLayer::setOpacity(float value) {
        _opacity = value;
    }

    const optional<float>& VisibleLayer::opacity() const {
        return _opacity;
    }

} // namespace VisibleLayer
