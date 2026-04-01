/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once
#include <rocky/Common.h>
#include <rocky/Math.h>
#include <string>
#include <vector>
#include <cstdint>

namespace ROCKY_NAMESPACE
{
    /**
     * 4-channel Color
     */
    class ROCKY_EXPORT Color : public glm::fvec4
    {
    public:
        enum class Format {
            RGBA,
            ABGR,
            ARGB            
        };

        /** Creates a new default color */
        Color() :
            glm::fvec4(1, 1, 1, 1) {}

        /** Copy constructor */
        Color(const Color& rhs) = default;

        /** Make from vec3 */
        Color(const glm::fvec3& rgb) :
            glm::fvec4(rgb[0], rgb[1], rgb[2], 1.0f) {}

        /** Copy constructor with modified alpha value */
        Color(const Color& rhs, float a) :
            glm::fvec4(rhs[0], rhs[1], rhs[2], a) {}

        /** Component constructor */
        Color(float r, float g, float b, float a = 1.0f) :
            glm::fvec4(r, g, b, a) {}

        /** RGBA/ABGR constructor */
        explicit Color(std::uint32_t value, Format format = Format::RGBA);

        /**
         * Construct a color from a hex string in one of the following formats, (with or
         * without the component order reversed):
         *   RRGGBB or RRGGBBAA
         *   #RRGGBB or #RRGGBBAA (HTML style - preceding hash)
         *   0xRRGGBB or 0xRRGGBBAA (C style - preceding "0x")
         */
        Color(const std::string& html, Format format = Format::RGBA);

        /** Encode the color at an HTML color string (e.g., "#FF004F78") */
        std::string toHTML(Format format = Format::RGBA) const;

        /** Dump out the color as a 32-bit integer */
        std::uint32_t as(Format format) const;

        /** Lighten/darken the color by factor */
        Color brighten(float factor) const;

        //! gets the color as HSL (and alpha)
        glm::fvec4 asHSL() const;

        //! as normalized ubyte4
        glm::u8vec4 asNormalizedRGBA() const;

        //! populates the RGB from HSL
        static Color fromHSL(const glm::fvec4& hsla);

        //! generate a random color ramp
        static std::vector<Color> createRandomColorRamp(unsigned count, int seed = -1);

    private:

        void set(float r, float g, float b, float a);
    };

    namespace StockColor
    {
        // some handy built-in colors
        // http://en.wikipedia.org/wiki/Web_colors#HTML_color_names
        // These are const (and not inline const) so that each TU will have its
        // own copy and there will be no SIOF issues

        const Color White(0xffffffff);
        const Color Silver(0xc0c0c0ff);
        const Color Gray(0x808080ff);
        const Color Black(0x000000ff);
        const Color Red(0xff0000ff);
        const Color Maroon(0x800000ff);
        const Color Yellow(0xffff00ff);
        const Color Olive(0x808000ff);
        const Color Lime(0x00ff00ff);
        const Color Green(0x008000ff);
        const Color Aqua(0x00ffffff);
        const Color Teal(0x008080ff);
        const Color Blue(0x0000ffff);
        const Color Navy(0x000080ff);
        const Color Fuchsia(0xff00ffff);
        const Color Purple(0x800080ff);
        const Color Orange(0xffa500ff);

        // Others
        const Color Cyan(0x00ffffff);
        const Color DarkGray(0x404040ff);
        const Color Magenta(0xc000c0ff);
        const Color Brown(0xaa5500ff);
        const Color Transparent(0x00000000);
    };
}

