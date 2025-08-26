/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    //! Global "no data" value for heightfields.
    constexpr float NO_DATA_VALUE = -std::numeric_limits<float>::max();

    //! Default image format for heightfields.
    constexpr Image::PixelFormat HEIGHTFIELD_FORMAT = Image::R32_SFLOAT;

    /**
    * Wrapper around an Image that provides heightfield functions.
    */
    class Heightfield
    {
    public:
        //! Construct a new Image wrapped in a Heightfield API.
        static Heightfield create(unsigned cols, unsigned rows) {
            return Heightfield(cols, rows);
        }

        //! Wrap a Heightfield API around an existing image.
        Heightfield(Image::Ptr in_image) : image(in_image)
        {
            ROCKY_SOFT_ASSERT(image);
            ROCKY_SOFT_ASSERT(image->pixelFormat() == HEIGHTFIELD_FORMAT);
        }
        
        //! Construct a new heightfield with given dimensions.
        Heightfield(unsigned cols, unsigned rows)
        {
            image = Image::create(HEIGHTFIELD_FORMAT, cols, rows, 1);
        }

        //! Underlying image.
        Image::Ptr image = nullptr;

        //! Mutable reference to the height value at col, row
        inline float& heightAt(unsigned col, unsigned row);

        //! Height value at col, row
        inline float heightAt(unsigned col, unsigned row) const;

        //! Height value at normalized UV coordinates (0..1, 0..1)
        inline float heightAtUV(float u, float v) const;

        //! Visits each height in the field with a user-provided function
        //! that takes "float" or "float&" as an argument.
        template<typename CALLABLE>
        inline void forEachHeight(CALLABLE&& func);

        //! Visits each height in the field with a user-provided function
        //! that takes "float" as an argument.
        template<typename CALLABLE>
        inline void forEachHeight(CALLABLE&& func) const;

        //! Fill with a single height value
        inline void fill(float value);

        //! Width of the underlying image (in pixels)
        inline unsigned width() const { return image->width(); }

        //! Height of the underlying image (in pixels)
        inline unsigned height() const { return image->height(); }

        //! No-data value of the underlying image
        inline float noDataValue() const { return image->noDataValue(); }
    };

    // inline functions

    inline float& Heightfield::heightAt(unsigned c, unsigned r)
    {
        return image->value<float>(c, r);
    }

    inline float Heightfield::heightAt(unsigned c, unsigned r) const
    {
        return image->value<float>(c, r);
    }

    inline float Heightfield::heightAtUV(float u, float v) const
    {
        return image->read_bilinear(u, v, 0).r;
    }

    template<typename CALLABLE>
    inline void Heightfield::forEachHeight(CALLABLE&& func)
    {
        float* ptr = image->data<float>();
        for (auto i = 0u; i < image->sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }

    template<typename CALLABLE>
    inline void Heightfield::forEachHeight(CALLABLE&& func) const
    {
        static_assert(std::is_invocable_v<CALLABLE, float>, "Callable must be invocable with a float argument");
        const float* ptr = image->data<float>();
        for (auto i = 0u; i < image->sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }
    
    inline void Heightfield::fill(float value)
    {
        float* ptr = image->data<float>();
        for (unsigned i = 0; i < image->sizeInPixels(); ++i)
            *ptr++ = value;
    }
}
