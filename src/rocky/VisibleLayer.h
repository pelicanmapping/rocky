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
        virtual void setVisible(bool value);
        bool getVisible() const;

        //! Opacity with which to draw this layer
        virtual void setOpacity(float value);
        float getOpacity() const;

        //! Minimum camera range at which this image layer is visible (if supported)
        float getMinVisibleRange() const;
        void setMinVisibleRange( float minVisibleRange );

        //! Maximum camera range at which this image layer is visible (if supported)
        float getMaxVisibleRange() const;
        void setMaxVisibleRange( float maxVisibleRange );

        //! Distance (m) over which to ramp min and max range blending
        float getAttenuationRange() const;
        void setAttenuationRange( float value );

        //! Blending mode to apply when rendering this layer
        void setColorBlending(ColorBlending value);
        ColorBlending getColorBlending() const;

        //! Enables/disables a debug view for this layer, if available.
        void setEnableDebugView(bool value);
        bool getEnableDebugView() const;

    public: // Layer

        virtual Status openImplementation(const IOOptions&) override;

        virtual Config getConfig() const override;

    protected: // Layer

        VisibleLayer();

        VisibleLayer(const Config&);

    private:

        void construct(const Config&);

        optional<bool> _visible = true;
        optional<float> _opacity = 1.0f;
        optional<unsigned> _mask = 0xffffffff;
        optional<float> _maxVisibleRange = FLT_MAX;
        optional<float> _minVisibleRange = 0.0f;
        optional<float> _attenuationRange = 0.0f;
        optional<ColorBlending> _blend = BLEND_INTERPOLATE;
        optional<bool> _debugView = false;
    };

} // namespace VisibleLayer
