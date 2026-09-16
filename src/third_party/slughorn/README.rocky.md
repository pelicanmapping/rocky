# Slughorn source provenance

This directory vendors the minimal Slughorn atlas-building core used by Rocky's
experimental Slug overlay path.

- Upstream: https://github.com/AlphaPixel/slughorn
- Commit: `1b203c191bd152c20685f8a896d22dcff974cec4` (latest `main` at import)
- Imported: 2026-09-15
- License: MIT; see `LICENSE`

The files under `slughorn/` come from that commit. `serial.hpp` and its
`render.hpp` dependency are included so the diagnostic demo can export the
exact runtime atlas as a `.slug` file; serialization uses Rocky's existing
private nlohmann-json dependency. The files under `rocky/` are Rocky's private
compatibility adapter and are not part of upstream Slughorn.

The imported core files are unmodified. Rocky retains RGBA32F curve precision;
bands now use upstream's native RG16UI format. Rocky's analytic coverage shader
supports the shared curve endpoints and row-spanning curve/band addresses in
this revision. Existing Rocky canvas rounding fixes are present upstream.
