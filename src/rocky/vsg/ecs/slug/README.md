# Rocky's private Slughorn adapter

These files belong to Rocky, not the external Slughorn SDK. `SlugAdapter.h` is
a C++17-compatible boundary consumed by SlugSystem. `SlugAdapter.cpp` alone
includes SDK headers and is compiled as C++20 in a private static library.
The SDK is resolved through `cmake/ResolveSlughorn.cmake`; its shared source-build
recipe and pinned revision live in `vcpkg-ports/slughorn/`. See
`docs/slughorn.md` for dependency setup and update instructions.

Serialization is enabled in the external SDK so the diagnostic demo can export
the exact runtime atlas as a `.slug` file, using nlohmann-json.

Rocky retains RGBA32F curve precision;
bands now use upstream's native RG16UI format. Rocky's analytic coverage shader
supports the shared curve endpoints and row-spanning curve/band addresses in
the pinned revision. Existing Rocky canvas rounding fixes are present upstream.

Polygon fills retain exterior/hole associations in the private adapter input.
Before committing shapes, the adapter counts the pinned SDK's band references
and checks both 16-bit header fields. Oversized groups are spatially partitioned
into multiple shapes in the same atlas, without failed-build retries. Holes and
overlapping polygon bounds stay together; an indivisible group that still exceeds
the format limit is reported as an error. Tight batch metrics preserve the original
curve coordinates and enable the existing shader's bounds rejection. Strokes,
points, and unstructured mesh contours are not partitioned by this path.
