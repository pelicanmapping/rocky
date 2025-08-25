/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    constexpr float NO_DATA_VALUE = -std::numeric_limits<float>::max();

    /**
    * Wrapper around an Image that provides heightfield functions.
    */
    class /*ROCKY_EXPORT*/ Heightfield
    {
    public:
        static const Image::PixelFormat FORMAT = Image::R32_SFLOAT;

        static Heightfield create(unsigned cols, unsigned rows) {
            return Heightfield(cols, rows);
        }

        Heightfield(Image::Ptr in_image) : image(in_image)
        {
            ROCKY_SOFT_ASSERT(image);
            ROCKY_SOFT_ASSERT(image->pixelFormat() == FORMAT);
        }
        
        Heightfield(unsigned cols, unsigned rows)
        {
            image = Image::create(FORMAT, cols, rows, 1);
        }

        Image::Ptr image = nullptr;

        //! Access the height value at col, row
        inline float& heightAt(unsigned col, unsigned row);
        inline float heightAt(unsigned col, unsigned row) const;

        inline float heightAtUV(float u, float v) const;

        //! Visits each height in the field with a user-provided function
        //! that takes "float" or "float&" as an argument.
        template<typename CALLABLE>
        inline void forEachHeight(CALLABLE&& func);

        template<typename CALLABLE>
        inline void forEachHeight(CALLABLE&& func) const;

        //! Fill with a single height value
        inline void fill(float value);

        inline unsigned width() const { return image->width(); }
        inline unsigned height() const { return image->height(); }
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
