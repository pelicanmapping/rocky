# Slughorn source provenance

This directory vendors the minimal Slughorn atlas-building core used by Rocky's
experimental Slug overlay path.

- Upstream: https://github.com/AlphaPixel-LLC/slughorn
- Commit: `312ef217aaf6b1c47b05ba7575342b513daa830d`
- Imported: 2026-08-19
- License: MIT; see `LICENSE`

The files under `slughorn/` come from that commit. `serial.hpp` and its
`render.hpp` dependency are included so the diagnostic demo can export the
exact runtime atlas as a `.slug` file; serialization uses Rocky's existing
private nlohmann-json dependency. The files under `rocky/` are Rocky's private
compatibility adapter and are not part of upstream Slughorn.
