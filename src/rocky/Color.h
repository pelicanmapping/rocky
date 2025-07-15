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
     * Pre-set colors (convenience class).
     */
    class ROCKY_EXPORT Color : public glm::fvec4
    {
    public:
        enum Format {
            RGBA,
            ABGR
        };

        /** Creates a new default color */
        Color() : 
            glm::fvec4(1, 1, 1, 1) { }

        /** Copy constructor */
        Color(const Color& rhs) : 
            glm::fvec4(rhs) { }

        /** Make from vec4 */
        Color(const glm::fvec4& rgba) : glm::fvec4(rgba) { }

        /** Make from vec3 */
        Color(const glm::fvec3& rgb) :
            glm::fvec4(rgb[0], rgb[1], rgb[2], 1.0f) { }

        /** Copy constructor with modified alpha value */
        Color(const Color& rhs, float a);

        /** Component constructor */
        Color(float r, float g, float b, float a = 1.0f) :
            glm::fvec4(r, g, b, a) { }

        /** Vector constructor */
        explicit Color(float v) :
            glm::fvec4(v, v, v, v) { }

        /** RGBA/ABGR constructor */
        explicit Color(std::uint32_t value, Format format = RGBA);

        /**
         * Construct a color from a hex string in one of the following formats, (with or
         * without the component order reversed):
         *   RRGGBB or RRGGBBAA
         *   #RRGGBB or #RRGGBBAA (HTML style - preceding hash)
         *   0xRRGGBB or 0xRRGGBBAA (C style - preceding "0x")
         */
        Color(const std::string& html, Format format = RGBA);

        /** Encode the color at an HTML color string (e.g., "#FF004F78") */
        std::string toHTML(Format format = RGBA) const;

        /** Dump out the color as a 32-bit integer */
        std::uint32_t as(Format format) const;

        /** Lighten/darken the color by factor */
        Color brightness(float factor) const;

        //! gets the color as HSL (and alpha)
        glm::fvec4 asHSL() const;

        //! populates the RGB from HSL
        void fromHSL(const glm::fvec4& hsla);

        //! as normalized ubyte4
        glm::u8vec4 asNormalizedRGBA() const;

        //! set all four values
        void set(float r, float g, float b, float a);

        // built in colors
        // http://en.wikipedia.org/wiki/Web_colors#HTML_color_names

        static const Color White;
        static const Color Silver;
        static const Color Gray;
        static const Color Black;
        static const Color Red;
        static const Color Maroon;
        static const Color Yellow;
        static const Color Olive;
        static const Color Lime;
        static const Color Green;
        static const Color Aqua;
        static const Color Teal;
        static const Color Blue;
        static const Color Navy;
        static const Color Fuchsia;
        static const Color Purple;
        static const Color Orange;

        // others:
        static const Color Cyan;
        static const Color DarkGray;
        static const Color Magenta;
        static const Color Brown;
        static const Color Transparent;

        //! generate a random color ramp
        static std::vector<Color> createRandomColorRamp(unsigned count, int seed = -1);
    };

}

