/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "Image.h"
#include <cstring>

using namespace ROCKY_NAMESPACE;

namespace
{
    using uchar = unsigned char;
    using ushort = unsigned short;

    constexpr float norm_8 = 255.0f;
    constexpr float denorm_8 = 1.0f / norm_8;

    template<typename T>
    struct NORM8 {
        static Image::Pixel read(unsigned char* ptr, int n) {
            return Image::Pixel(
                n > 0 ? (float)(ptr[0]) * denorm_8 : 1.0f,
                n > 1 ? (float)(ptr[1]) * denorm_8 : 1.0f,
                n > 2 ? (float)(ptr[2]) * denorm_8 : 1.0f,
                n > 3 ? (float)(ptr[3]) * denorm_8 : 1.0f);
        }
        static void write(const Image::Pixel& pixel, unsigned char* ptr, int n) {
            for (int i = 0; i < n; ++i)
                *ptr++ = (T)(pixel[i] * norm_8);
        }
    };

    template<typename T>
    struct SRGB8 {
        // RGB are encoded, alpha is direct
        static Image::Pixel read(unsigned char* ptr, int n) {
            Image::Pixel pixel;
            for (int i = 0; i < std::min(n, 3); ++i)
                pixel[i] = util::sRGB_to_linear((float)(*ptr++) * denorm_8);
            for (int i = std::min(n, 3); i < n; ++i)
                pixel[i] = (float)(*ptr++) * denorm_8;
            for (int i = n; i < 4; ++i)
                pixel[i] = 1.0f;
            return pixel;
        }
        static void write(const Image::Pixel& pixel, unsigned char* ptr, int n) {
            for (int i = 0; i < std::min(n, 3); ++i)
                *ptr++ = (T)(util::linear_to_sRGB(pixel[i]) * norm_8);
            for (int i = std::min(n, 3); i < n; ++i)
                *ptr++ = (T)(pixel[i] * norm_8);
        }
    };

    constexpr float norm_16 = 65535.0f;
    constexpr float denorm_16 = 1.0f / norm_16;

    template<typename T>
    struct NORM16 {
        static Image::Pixel read(unsigned char* ptr, int n) {
            T* sptr = (T*)ptr;
            return Image::Pixel(
                n > 0 ? (float)(sptr[0]) * denorm_16 : 1.0f,
                n > 1 ? (float)(sptr[1]) * denorm_16 : 1.0f,
                n > 2 ? (float)(sptr[2]) * denorm_16 : 1.0f,
                n > 3 ? (float)(sptr[3]) * denorm_16 : 1.0f);
        }
        static void write(const Image::Pixel& pixel, unsigned char* ptr, int n) {
            T* sptr = (T*)ptr;
            for (int i = 0; i < n; ++i)
                *sptr++ = (T)(pixel[i] * norm_16);
        }
    };

    template<typename T>
    struct FLOAT {
        static Image::Pixel read(unsigned char* ptr, int n) {
            T* sptr = (T*)ptr;
            return Image::Pixel(
                n > 0 ? (float)(sptr[0]) : 1.0f,
                n > 1 ? (float)(sptr[1]) : 1.0f,
                n > 2 ? (float)(sptr[2]) : 1.0f,
                n > 3 ? (float)(sptr[3]) : 1.0f);
        }
        static void write(const Image::Pixel& pixel, unsigned char* ptr, int n) {
            T* sptr = (T*)ptr;
            for (int i = 0; i < n; ++i)
                *sptr++ = (T)pixel[i];
        }
    };
}

// static member
Image::Layout Image::_layouts[11] =
{
    { &NORM8<uchar>::read, &NORM8<uchar>::write, 1, 1, R8_UNORM },
    { &SRGB8<uchar>::read, &SRGB8<uchar>::write, 1, 1, R8_SRGB },
    { &NORM8<uchar>::read, &NORM8<uchar>::write, 2, 2, R8G8_UNORM },
    { &SRGB8<uchar>::read, &SRGB8<uchar>::write, 2, 2, R8G8_SRGB },
    { &NORM8<uchar>::read, &NORM8<uchar>::write, 3, 3, R8G8B8_UNORM },
    { &SRGB8<uchar>::read, &SRGB8<uchar>::write, 3, 3, R8G8B8_SRGB },
    { &NORM8<uchar>::read, &NORM8<uchar>::write, 4, 4, R8G8B8A8_UNORM },
    { &SRGB8<uchar>::read, &SRGB8<uchar>::write, 4, 4, R8G8B8A8_SRGB },
    { &NORM16<ushort>::read, &NORM16<ushort>::write, 1, 2, R16_UNORM },
    { &FLOAT<float>::read, &FLOAT<float>::write, 1, 4, R32_SFLOAT },
    { &FLOAT<double>::read, &FLOAT<double>::write, 1, 8, R64_SFLOAT }
};

Image::Image(PixelFormat format, unsigned cols, unsigned rows, unsigned depth) :    
    super(),
    _width(0), _height(0), _depth(0),
    _pixelFormat(R8G8B8A8_UNORM),
    _data(nullptr)
{
    allocate(format, cols, rows, depth);
}

Image::Image(const Image& rhs) :
    super(rhs)
{
    if (rhs.sizeInBytes() > 0 && rhs._data != nullptr)
    {
        allocate(rhs.pixelFormat(), rhs.width(), rhs.height(), rhs.depth());
        memcpy(_data, rhs._data, sizeInBytes());
    }
}

Image::Image(Image&& rhs) noexcept :
    super(rhs)
{
    if (this != &rhs)
    {
        _width = rhs._width;
        _height = rhs._height;
        _depth = rhs._depth;
        _pixelFormat = rhs._pixelFormat;
        _data = rhs.releaseData();
    }
}

Image::~Image()
{
    if (_data && _ownsData)
    {
        delete[] _data;
    }
}

bool
Image::hasAlphaChannel() const
{
    return pixelFormat() == R8G8B8A8_UNORM || pixelFormat() == R8G8B8A8_SRGB;
}

std::shared_ptr<Image>
Image::clone() const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(_data, nullptr);

    auto clone = Image::create(pixelFormat(), width(), height(), depth());
    memcpy(clone->data<unsigned char*>(), _data, sizeInBytes());

    return clone;
}

void
Image::allocate(
    PixelFormat pixelFormat_,
    unsigned width_,
    unsigned height_,
    unsigned depth_)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(
        width_ > 0 && height_ > 0 && depth_ > 0 &&
        (unsigned)pixelFormat_ >= 0 && pixelFormat_ < NUM_PIXEL_FORMATS,
        void());
    
    _width = width_;
    _height = height_;
    _depth = depth_;
    _pixelFormat = pixelFormat_;

    auto layout = _layouts[pixelFormat()];

    if (_data && _ownsData)
        delete[] _data;

    _data = new unsigned char[sizeInBytes()];
    _ownsData = true;

    // simple init for one-byte images
    if (sizeInBytes() > 0)
        write(glm::fvec4(0, 0, 0, 0), 0, 0);
}

unsigned char*
Image::releaseData()
{
    auto released = _data;
    _data = nullptr;
    _width = 0;
    _height = 0;
    _depth = 0;
    return released;
}

void
Image::flipVerticalInPlace()
{
    ROCKY_TODO("handle compressed pixel formats");

    auto layerBytes = sizeInBytes() / depth();
    auto rowBytes = rowSizeInBytes();
    auto halfRows = height() / 2;

    for (unsigned d = 0; d < depth(); ++d)
    {
        unsigned layerOffset = d * layerBytes;
        for (unsigned row = 0; row < halfRows; ++row)
        {
            auto antirow = height() - 1 - row;
            auto row1 = data<uchar>() + layerOffset + row*rowBytes;
            auto row2 = data<uchar>() + layerOffset + antirow*rowBytes;
            for (unsigned b = 0; b < rowBytes; ++b, ++row1, ++row2)
                std::swap(*row1, *row2);
        }
    }
}

void
Image::fill(const Image::Pixel& value)
{
    for (unsigned r = 0; r < depth(); ++r)
        for (unsigned t = 0; t < height(); ++t)
            for (unsigned s = 0; s < width(); ++s)
                write(value, s, t, r);
}


std::shared_ptr<Image>
Image::convolve(const float* kernel) const
{
    glm::fvec4 samples[9];

    auto output = clone();

    for (unsigned r = 0; r < depth(); ++r)
    {
        for (unsigned t = 0; t < height(); ++t)
        {
            for (unsigned s = 0; s < width(); ++s)
            {
                unsigned t_minus_1 = t > 0 ? t - 1 : t;
                unsigned t_plus_1 = t < height() - 1 ? t + 1 : t;
                unsigned s_minus_1 = s > 0 ? s - 1 : s;
                unsigned s_plus_1 = s < width() - 1 ? s + 1 : s;

                samples[0] = read(s_minus_1, t_minus_1, r);
                samples[1] = read(s, t_minus_1, r);
                samples[2] = read(s_plus_1, t_minus_1, r);

                samples[3] = read(s_minus_1, t, r);
                samples[4] = read(s, t, r);
                samples[5] = read(s_plus_1, t, r);

                samples[6] = read(s_minus_1, t_plus_1, r);
                samples[7] = read(s, t_plus_1, r);
                samples[8] = read(s_plus_1, t_plus_1, r);

                glm::fvec4 pixel(0, 0, 0, 0);
                for (int i = 0; i < 9; ++i)
                {
                    pixel += (samples[i] * kernel[i]);
                }
                output->write(clamp(pixel, 0.0f, 1.0f), s, t, r);
            }
        }
    }

    return output;
}

std::shared_ptr<Image>
Image::sharpen(float k) const
{
    ROCKY_SOFT_ASSERT_AND_RETURN(k > 0.0f, {});

#if 1
    // box kernel (looks better than gaussian for OSM)
    float a = -k / 9.0f;
    float b = 1.0f - (8.0f * a);
    const float kernel[9] = {
        a, a, a,
        a, b, a,
        a, a, a };
#else
    // guassian kernel
    float a = -k / 16.0f;
    float b = 1.0f - (4.0f * a);
    const float kernel[9] = {
        0, a, 0,
        a, b, a,
        0, a, 0 };
#endif

    return convolve(kernel);
}

Image
Image::viewAs(Image::PixelFormat format) const
{
    Image view;
    view._pixelFormat = format;
    view._width = _width;
    view._height = _height;
    view._depth = _depth;
    view._data = _data; // share the data
    view._ownsData = false;
    return view;
}
