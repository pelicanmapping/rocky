/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>
#include <cmath>
#include <limits>

namespace ROCKY_NAMESPACE
{
    class IOOptions;

    /**
     * A raster grid. The image can hold any data including colors, height samples,
     * coverage values, etc. as long as it's storable in one of the supported
     * pixel formats.
     */
    class ROCKY_EXPORT Image : public Inherit<Object, Image>
    {
    public:
        using Ptr = std::shared_ptr<Image>;

        //! Pixel formats.
        enum PixelFormat {
            R8_UNORM,
            R8_SRGB,
            R8G8_UNORM,
            R8G8_SRGB,
            R8G8B8_UNORM,
            R8G8B8_SRGB,
            R8G8B8A8_UNORM,
            R8G8B8A8_SRGB,
            R16_UNORM,
            R32_SFLOAT,
            R64_SFLOAT,
            NUM_PIXEL_FORMATS,
            UNDEFINED
        };

        //! Pixel value. For color data, each channel is assumed to be in linear color space.
        using Pixel = glm::fvec4;

    public:
        //! number of colums
        unsigned width() const { return _width; }

        //! Number of rows
        unsigned height() const { return _height; }

        //! Number of layers
        unsigned depth() const { return _depth; }

        //! Pixel format (see PixelFormat enum)
        PixelFormat pixelFormat() const { return _pixelFormat; }

        //! No-data value (for elevation)
        float noDataValue() const { return _noDataValue; }

        //! Whether there's an alpha channel
        bool hasAlphaChannel() const;

    public:
        //! Construct an empty (invalid) image
        Image() = default;

        //! Construct an image an allocate memory for it,
        //! unless data is non-null, in which case use that memory
        Image(PixelFormat format, unsigned s, unsigned t, unsigned r = 1);

        //! Copy constructor
        Image(const Image& rhs);

        //! Move constructor
        Image(Image&& rhs) noexcept;

        //! Destruct and release the data unless it's not owned
        virtual ~Image();

        //! Iterator for visiting each pixel in the image.        
        class iterator
        {
        public:
            iterator(const Image* image) : _image(image) { }

            inline unsigned r() const { return _r; }
            inline unsigned s() const { return _s; }
            inline unsigned t() const { return _t; }
            inline double u() const { return _u; }
            inline double v() const { return _v; }

            //! Call the user callable for each pixel in the image.
            //! CALLABLE must have the signature void(const Iterator& i).
            template<typename CALLABLE> inline void each(CALLABLE&& func);

        private:
            const Image* _image = nullptr;
            unsigned _r = 0, _s = 0, _t = 0;
            double _u = 0.0, _v = 0.0;
        };

        //! Whether this object contains valid data
        inline bool valid() const;

        //! Pointer to the data array (type T).
        template<class T> T* data() {
            return reinterpret_cast<T*>(_data);
        }

        //! Const pointer to the data array (type T).
        template<class T> const T* data() const {
            return reinterpret_cast<T*>(_data);
        }

        //! Raw pointer to the data starting at s, t, layer.
        template<class T> T* data(unsigned s, unsigned t, unsigned layer = 0) const {
            return &reinterpret_cast<T*>(_data)[layer * width() * height() + t * width() + s];
        }

        //! Value (type T) at s, t, layer.
        template<class T> T value(unsigned s, unsigned t, unsigned layer = 0) const {
            return reinterpret_cast<T*>(_data)[layer * width() * height() + t * width() + s];
        }

        //! Mutable reference to the value (type T) at s, t, layer.
        template<class T> T& value(unsigned s, unsigned t, unsigned layer = 0) {
            return reinterpret_cast<T*>(_data)[layer * width() * height() + t * width() + s];
        }

        //! Value at the i'th position in the data array
        template<class T> T value(unsigned offset) const {
            return reinterpret_cast<T*>(_data)[offset];
        }

        //! Read the pixel at a column, row, and layer.
        //! \param pixel Output value (in linear color space, if applicable)
        inline Pixel read(unsigned s, unsigned t, unsigned layer = 0) const;

        //! Read the pixel at the location in an iterator
        //! \param pixel Output value (in linear color space, if applicable)        
        inline Pixel read(const iterator& i) const {
            return read(i.s(), i.t(), i.r());
        }

        //! Read the pixel at UV coordinates with bilinear interpolation
        //! \param pixel Output value (in linear color space, if applicable)
        inline Pixel read_bilinear(float u, float v, unsigned layer = 0) const;

        //! Read the pixel at UV coordinates with bilinear interpolation at an iterator location
        //! \param pixel Output value (in linear color space, if applicable)
        inline Pixel read_bilinear(const iterator& i) const {
            return read_bilinear(i.u(), i.v(), i.r());
        }

        //! Write the pixel at a column, row, and layer
        //! \param pixel Value to store (in linear color space, if applicable)
        inline void write(const Pixel& pixel, unsigned s, unsigned t, unsigned layer = 0);

        //! Write the pixel at the location in an iterator
        //! \param pixel Value to store (in linear color space, if applicable)
        inline void write(const Pixel& pixel, const iterator& i) {
            write(pixel, i.s(), i.t(), i.r());
        }

        //! Size of this image in bytes
        inline unsigned sizeInBytes() const;

        //! Size of this image in pixels
        inline unsigned sizeInPixels() const;

        //! Size of a row in bytes
        inline unsigned rowSizeInBytes() const;

        //! Size of each pixel component in bytes
        inline unsigned componentSizeInBytes() const;

        //! Creates a deep copy of this image
        virtual std::shared_ptr<Image> clone() const;

        //! Creates a sharpened clone of this image.
        //! @param strength sharpening kernel strength, 1-5 is typically a reasonable range
        std::shared_ptr<Image> sharpen(float strength = 2.5f) const;

        //! Creates a convolved clone of this image.
        //! @param kernel convolution kernel (9 floats that add up to 1.0f)
        std::shared_ptr<Image> convolve(const float* kernel) const;

        //! Inverts the pixels in the T dimension
        void flipVerticalInPlace();

        //! Nmmber of components in this image's pixel format
        inline unsigned numComponents() const;

        //! Iterate each pixel
        template<typename CALLABLE>
        void eachPixel(CALLABLE&& func) const;

        //! Fills the entire image with a single value
        //! \param pixel Value to store (in linear color space, if applicable)
        void fill(const Pixel& pixel);

        //! Releases this image's data without deleting it. 
        //! Use this to transfer ownership of the raw data to someone else.
        //! The inheritor is responsible to deleting the data.
        //! This object becomes invalid unless you call allocate() on it again.
        unsigned char* releaseData();

        //! Reinterprets this image as having a different pixel format.
        //! Use this with caution. Only use this as a temporary object.
        //! Data ownership is shared between the original and the view,
        //! and NO data conversion is performed.
        //! \param format Pixel format to reinterpret data as
        //! \return A new image that shares the data with this one
        Image viewAs(Image::PixelFormat format) const;

    protected:
        unsigned _width = 0, _height = 0, _depth = 0;
        PixelFormat _pixelFormat = R8G8B8A8_UNORM;
        unsigned char* _data = nullptr;
        float _noDataValue = -std::numeric_limits<float>::max(); // default no-data value
        bool _ownsData = true;

        void allocate(PixelFormat format, unsigned s, unsigned t, unsigned r);

        struct Layout {
            Pixel(*read)(unsigned char*, int);
            void(*write)(const Pixel&, unsigned char*, int);
            int num_components;
            int bytes_per_pixel;
            PixelFormat format;
        };
        static Layout _layouts[11];

        inline unsigned sizeof_miplevel(unsigned level) const;
        inline unsigned char* data_at_miplevel(unsigned level);
    };


    // inline functions

    inline bool Image::valid() const
    {
        return width() > 0 && height() > 0 && depth() > 0 && _data;
    }

    inline Image::Pixel Image::read(unsigned s, unsigned t, unsigned layer) const
    {
        return _layouts[pixelFormat()].read(
            _data + (width()*height()*layer + width()*t + s)*_layouts[pixelFormat()].bytes_per_pixel,
            _layouts[pixelFormat()].num_components);
    }

    inline Image::Pixel Image::read_bilinear(float u, float v, unsigned layer) const
    {
        u = util::clamp(u, 0.0f, 1.0f);
        v = util::clamp(v, 0.0f, 1.0f);

        float sizeS = (float)(width() - 1);
        float s = u * sizeS;
        float s0 = std::max(std::floor(s), 0.0f);
        float s1 = std::min(s0 + 1.0f, sizeS);
        float smix = s0 < s1 ? (s - s0) / (s1 - s0) : 0.0f;

        float sizeT = (float)(height() - 1);
        float t = v * sizeT;
        float t0 = std::max(std::floor(t), 0.0f);
        float t1 = std::min(t0 + 1.0f, sizeT);
        float tmix = t0 < t1 ? (t - t0) / (t1 - t0) : 0.0f;

        auto UL = read((unsigned)s0, (unsigned)t0, layer);
        auto UR = read((unsigned)s1, (unsigned)t0, layer);
        auto LL = read((unsigned)s0, (unsigned)t1, layer);
        auto LR = read((unsigned)s1, (unsigned)t1, layer);

        Pixel TOP = UL.r == _noDataValue ? UR : UR.r == _noDataValue ? UL : UL * (1.0f - smix) + UR * smix;
        Pixel BOT = LL.r == _noDataValue ? LR : LR.r == _noDataValue ? LL : LL * (1.0f - smix) + LR * smix;

        if (TOP.r == _noDataValue && BOT.r == _noDataValue)
            return Pixel(_noDataValue);

        return
            TOP.r == _noDataValue ? BOT :
            BOT.r == _noDataValue ? TOP :
            TOP * (1.0f - tmix) + BOT * tmix;
    }

    void Image::write(const Pixel& pixel, unsigned s, unsigned t, unsigned layer)
    {
        _layouts[pixelFormat()].write(
            pixel,
            _data + (width() * height() * layer + height() * t + s) * _layouts[pixelFormat()].bytes_per_pixel,
            _layouts[pixelFormat()].num_components);
    }

    unsigned Image::sizeInBytes() const
    {
        return sizeInPixels() * _layouts[pixelFormat()].bytes_per_pixel;
    }

    unsigned Image::sizeInPixels() const
    {
        return width() * height() * depth();
    }

    unsigned Image::rowSizeInBytes() const
    {
        return width() * _layouts[pixelFormat()].bytes_per_pixel;
    }

    unsigned char* Image::data_at_miplevel(unsigned m)
    {
        auto d = _data;
        for (int i = 0; i < (int)m; ++i)
            d += sizeof_miplevel(i);
        return d;
    }

    unsigned Image::sizeof_miplevel(unsigned level) const
    {
        return (width() * height() * depth()) >> level;
    }

    unsigned Image::numComponents() const
    {
        return _layouts[pixelFormat()].num_components;
    }

    unsigned Image::componentSizeInBytes() const
    {
        return _layouts[pixelFormat()].bytes_per_pixel /
            _layouts[pixelFormat()].num_components;
    }

    template<typename CALLABLE>
    inline void Image::iterator::each(CALLABLE&& func)
    {
        static_assert(std::is_invocable_r_v<void, CALLABLE, const iterator&>);

        for (_r = 0; _r < _image->depth(); ++_r)
        {
            for (_t = 0; _t < _image->height(); ++_t)
            {
                _v = (double)_t / (double)(_image->height() - 1);

                for (_s = 0; _s < _image->width(); ++_s)
                {
                    _u = (double)_s / (double)(_image->width() - 1);
                    func(*this);
                }
            }
        }
    }

    template<typename CALLABLE>
    inline void Image::eachPixel(CALLABLE&& func) const {
        iterator(this).each(std::forward<CALLABLE>(func));
    }


    namespace util
    {
        inline constexpr float linear_to_sRGB(float c)
        {
            constexpr float cutoff = 0.04045f / 12.92f;
            constexpr float linearFactor = 12.92f;
            constexpr float nonlinearFactor = 1.055f;
            constexpr float exponent = 1.0f / 2.4f;
            if (c <= cutoff)
                return c * linearFactor;
            else
                return std::pow(c, exponent) * nonlinearFactor - 0.055f;
        }

        inline constexpr float sRGB_to_linear(float c)
        {
            constexpr float cutoff = 0.04045f / 12.92f;
            constexpr float linearFactor = 12.92f;
            constexpr float nonlinearFactor = 1.055f;
            constexpr float exponent = 2.4f;
            if (c <= linearFactor * cutoff)
                return c / linearFactor;
            else
                return std::pow((c + 0.055f) / nonlinearFactor, exponent);
        }
    }



    /**
    * Image subclass that keeps a list of the images used to assemble it.
    * This in combination with the resident image cache speeds up assemble operations.
    */
    class Mosaic : public Inherit<Image, Mosaic>
    {
    public:
        //! Construct a new mosaic
        Mosaic(Image::PixelFormat format, unsigned s, unsigned t, unsigned r = 1) :
            super(format, s, t, r) {
        }

        //! Copy constructor
        Mosaic(const Mosaic& rhs) = default;

        //! Clone this object
        std::shared_ptr<Image> clone() const override
        {
            return std::make_shared<Mosaic>(*this);
        }

        //! Collection of images used to create this mosaic
        std::vector<std::shared_ptr<Image>> dependencies;
    };
}
