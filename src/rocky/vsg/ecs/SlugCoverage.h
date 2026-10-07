/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include <memory>

namespace rocky::detail
{
    //! Immutable decoded atlas shape shared by layers and projected instances; contains no SDK types.
    class SlugCoverage
    {
    public:
        //! Releases the privately owned decoded curves and bands.
        virtual ~SlugCoverage() = default;

        //! Thread-safe shape-local coverage, matching Rocky's outer-band rejection. Returns zero for
        //! nonfinite coordinates or nonpositive pixel footprints; widths are em units per screen pixel.
        virtual float sample(float x, float y, float width, float height) const = 0;
    };
}
