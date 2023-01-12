/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    constexpr float NO_DATA_VALUE = -FLT_MAX;

    /**
     * A grid of height values (32-bit floats).
     */
    class ROCKY_EXPORT Heightfield :
        public Inherit<Image, Heightfield>
    {
    public:
        //! Construct an empty (and invalid) heightfield
        Heightfield();

        //! Construct a heightfield with the given dimensions
        Heightfield(
            unsigned cols,
            unsigned rows);

        //! Access the height value at col, row
        inline float& heightAt(unsigned col, unsigned row);
        inline float heightAt(unsigned col, unsigned row) const;

        //! Visits each height in the field with a user-provided function
        //! that takes "float" or "float&" as an argument.
        template<typename FUNC>
        void forEachHeight(FUNC func);

        template<typename FUNC>
        void forEachHeight(FUNC func) const;

        //! Interpolated height at a normalized (u,v) location
        float heightAtUV(
            double u, double v,
            Interpolation interp = BILINEAR) const;

        //! Interpolated height at a floating point col/row location
        float heightAtPixel(
            double col, double row,
            Interpolation interp = BILINEAR) const;

        //! Fill with a single height value
        void fill(float value);
    };


    // inline functions

    float& Heightfield::heightAt(unsigned c, unsigned r)
    {
        return data<float>(c, r);
    }

    float Heightfield::heightAt(unsigned c, unsigned r) const
    {
        return data<float>(c, r);
    }    

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func)
    {
        float* ptr = data<float>();
        for (auto i = 0u; i < sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func) const
    {
        float* ptr = data<float>();
        for (auto i = 0u; i < sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }
}
