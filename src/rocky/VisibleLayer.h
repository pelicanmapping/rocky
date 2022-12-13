/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Layer.h>

namespace rocky
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

        optional<bool> _visible;
        optional<float> _opacity;
        optional<unsigned> _mask;
        optional<float> _maxVisibleRange;
        optional<float> _minVisibleRange;
        optional<float> _attenuationRange;
        optional<ColorBlending> _blend;
        optional<bool> _debugView;
    };

} // namespace VisibleLayer
