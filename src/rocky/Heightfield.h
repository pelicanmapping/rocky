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
    constexpr Image::PixelFormat HF_WRITABLE_FORMAT = Image::R32_SFLOAT;
    constexpr Image::PixelFormat HF_ENCODED_FORMAT = Image::R16_UNORM;

    /**
    * Wrapper around an Image that provides heightfield functions.
    */
    class Heightfield
    {
    public:
        using EncodedDataType = std::uint16_t;


        //! Construct a new Image wrapped in a Heightfield API.
        static Heightfield create(unsigned cols, unsigned rows) {
            return Heightfield(cols, rows);
        }

        //! Wrap a Heightfield API around an existing image.
        Heightfield(Image::Ptr in_image) : image(in_image)
        {
            ROCKY_SOFT_ASSERT(image);
            ROCKY_SOFT_ASSERT(image->pixelFormat() == HF_WRITABLE_FORMAT || image->pixelFormat() == HF_ENCODED_FORMAT);
            _writable = image->pixelFormat() == HF_WRITABLE_FORMAT;
        }
        
        //! Construct a new heightfield with given dimensions.
        Heightfield(unsigned cols, unsigned rows)
        {
            image = Image::create(HF_WRITABLE_FORMAT, cols, rows, 1);
            _writable = true;
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

        //! Minimum height value (if known)
        inline float minHeight() const { return image->_minValue; }

        //! Maximum height value (if known)
        inline float maxHeight() const { return image->_maxValue; }

        //! Convert a writable heightfield into an encoded heightfield suitable for the GPU
        inline Heightfield encode() const;

        //! Has this heightfield been encoded?
        inline bool encoded() const;

        //! Recompute the min and max heights
        inline void computeAndSetMinMax();

        //! Compute the min and max heights and return them without storing them
        inline std::pair<float, float> computeMinMax() const;

    private:
        bool _writable = true;
        template<typename T> inline float decode(T v) const;
    };

    // inline functions

    template<typename T>
    inline float Heightfield::decode(T v) const
    {
        // decodes the [0..1] value from an encoded normalized image into a real height.
        return static_cast<float>(v) * (image->_maxValue - image->_minValue) + image->_minValue;
    }

    inline float& Heightfield::heightAt(unsigned c, unsigned r)
    {
        static float empty = 0;
        ROCKY_SOFT_ASSERT_AND_RETURN(_writable, empty);
        return image->value<float>(c, r);
    }

    inline float Heightfield::heightAt(unsigned c, unsigned r) const
    {
        return _writable ?
            image->read(c, r).r :
            decode(image->read(c, r).r);
    }

    inline float Heightfield::heightAtUV(float u, float v) const
    {
        return _writable ?
            image->read_bilinear(u, v).r :
            decode(image->read_bilinear(u, v).r);
    }

    template<typename CALLABLE>
    inline void Heightfield::forEachHeight(CALLABLE&& func)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(_writable, void());

        auto* ptr = image->data<float>();
            for (auto i = 0u; i < image->sizeInPixels(); ++i, ++ptr)
                func(*ptr);
    }

    template<typename CALLABLE>
    inline void Heightfield::forEachHeight(CALLABLE&& func) const
    {
        static_assert(std::is_invocable_v<CALLABLE, float>, "Callable must be invocable with a float argument");

        image->eachPixel([&](const Image::iterator& i)
            {
                func(_writable ? image->read(i).r : decode(image->read(i).r));
            });
    }
    
    inline void Heightfield::fill(float value)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(_writable, void());
        float* ptr = image->data<float>();
        for (unsigned i = 0; i < image->sizeInPixels(); ++i)
            *ptr++ = value;
    }

    inline void Heightfield::computeAndSetMinMax()
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(_writable, void());
        float& minH = image->_minValue;
        minH = std::numeric_limits<float>::max();
        float& maxH = image->_maxValue;
        maxH = -std::numeric_limits<float>::max();
        forEachHeight([&](float h)
            {
                if (h != NO_DATA_VALUE) {
                    if (h < minH) minH = h;
                    if (h > maxH) maxH = h;
                }
            });
    }

    inline std::pair<float, float> Heightfield::computeMinMax() const
    {
        float minH = std::numeric_limits<float>::max();
        float maxH = -std::numeric_limits<float>::max();
        forEachHeight([&](float h)
            {
                if (h != NO_DATA_VALUE) {
                    if (h < minH) minH = h;
                    if (h > maxH) maxH = h;
                }
            });
        return std::make_pair(minH, maxH);
    }

    inline Heightfield Heightfield::encode() const
    {
        ROCKY_HARD_ASSERT(_writable, "Image must be writable");
        ROCKY_HARD_ASSERT(image->_maxValue > image->_minValue, "Must call computeAndSetMinMax() before encoding");

        auto outImage = Image::create(HF_ENCODED_FORMAT, image->width(), image->height(), 1);
        outImage->_minValue = image->_minValue;
        outImage->_maxValue = image->_maxValue;

        auto* ptr = outImage->data<EncodedDataType>();

        forEachHeight([&](float h)
            {
                if (h == NO_DATA_VALUE) h = 0.0f;
                *ptr++ = static_cast<EncodedDataType>(((h - image->_minValue) / (image->_maxValue - image->_minValue)) * 65535.0f);;
            });

        return Heightfield(outImage);
    }

    inline bool Heightfield::encoded() const
    {
        return !_writable;
    }
}
