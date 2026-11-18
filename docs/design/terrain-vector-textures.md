# Terrain vector textures

`VectorTileLayer` supplies ordered surface artwork to terrain tiles. Each terrain
tile binds its imagery/elevation resources and an independent analytic vector
resource. The fragment shader evaluates vector coverage using terrain UVs before
lighting. There are no projectors, ECS entities, decal slots, or screen-space
decal lists for this channel.

`MapboxVectorTileLayer` supplies MVT data using the Mapbox/MapLibre style subset
described below. Solid round strokes use GPU analytic path coverage where the
projection permits it; fills, gap strokes and distorted strokes use analytic
filled coverage.

## Data and rendering

The producer receives a destination `TileKey` and returns a `VectorTile`: ordered
fills (exterior plus holes) and lines. TileUV coordinates run from southwest
`(0,0)` to northeast `(1,1)`; Mercator shapes use an isotropic local frame instead.
Geometry may extend beyond
the tile; terrain geometry bounds the draw. Providers must reproject, retile and
preserve neighboring geometry themselves. In particular, cutting lines exactly
at tile edges will introduce caps and seams.

Slughorn compiles the artwork into curve/band textures. The terrain resource also
holds painter-ordered layer records and a 16x16 spatial index. The index has no
per-cell truncation; broad fragment footprints fall back to the complete layer
list. Resource limits reject oversized input/atlases instead of silently dropping
shapes. Solid round lines retain centerlines and evaluate stroke width on the GPU;
gap lines retain baked ring coverage. Style pixel widths are evaluated at the
current style zoom, rather than maintained exactly in screen space across an
oblique view. Dash patterns, symbols and extrusions are not implemented.

All vector layers compose above the terrain's raster imagery, in map-layer order.
Interleaving individual raster and vector style layers is not supported. Layer
opacity multiplies each paint operation's alpha. Colors follow Rocky's existing
color conventions. Fill holes are normalized to opposite winding.

## Paging and ownership

Vector work is driven by terrain record traversal: only surfaces drawn in main
views request new vectors. Culled siblings and resident parent tiles retain
existing resources without competing for vector preparation. Vector jobs share
the terrain loader pool, its configured concurrency and distance/LOD priority.
The last recorded demand survives idle updates in render-on-demand applications;
a new record traversal replaces it, and paging transitions request another frame.
Imagery/elevation and vectors load alongside one another; vector completion does
not gate progressive terrain refinement. Children inherit the parent's resource
and UV scale/bias. maxDataLevel selects the vector source ancestor independently
of geometry/imagery detail, and simultaneous requests for that ancestor and
style/revision snapshot share one future/resource.

The update thread snapshots source revisions, opacity and view style. Atlas and
Vulkan descriptor preparation happen on terrain workers. Publication uses Rocky's
existing metered update queue, shared with imagery merges. A vector candidate is
installed only if its base render model is still current; otherwise it is rebased
on newer imagery/elevation. Dropping demand or changing the style/source snapshot
cancels preparation and queued publication. Abandoned compiled candidates use
VSG's deferred disposal. The request-sharing table retains only current demand;
resident vector resources follow terrain tile lifetime and normal page-out.

Callback producers are immutable while open; captured mutable data must be
synchronized. Change layer settings on the update thread and call dirty() when
source artwork changes. Closing a layer waits for active producers to release
their read locks.

A successful empty tile replaces inherited coverage. A failed request retains
the current resource and logs once while its request remains cached; `dirty()` or
reopening the layer retries it. There is no timed network retry policy yet.
Dropping unused futures cancels work. Scene binds retain descriptors until VSG's
deferred disposal releases them. CPU atlas arrays are currently retained, so a
production source will need explicit memory budgets and cache policies.

## Demo and validation

Load the bundled example through the ordinary map-file path:

```
rocky_demo --map data/openfreemap.map.json
```

The map uses a registered `MapboxVectorTile` layer; no application-specific layer
creation or update callback is required. The existing `--openfreemap` shortcut
remains available for comparison with earlier runs.

### Map-file serialization

```json
{
    "map": {
        "layers": [{
            "type": "MapboxVectorTile",
            "name": "OpenFreeMap",
            "style": "openfreemap-terrain.style.json",
            "maxDataLevel": 14,
            "maximumSourceZoom": 14
        }]
    },
    "terrain": { "maxLevel": 18 },
    "services": { "contentCache": { "directory": "rocky-cache", "memoryEntries": 256 } }
}
```

The style URI is relative to the map file, including maps loaded over HTTP(S).
It accepts Rocky's URI string or an object with `href` and `headers`. Source
TileJSON URIs resolve relative to the style; PBF templates resolve relative to
TileJSON (or the style when supplied directly). Serialization preserves the
authored style path and headers; reading the saved map supplies its new URI context.

| JSON property | Default | Meaning |
| --- | --- | --- |
| style | Required on open | URI of a supported Mapbox/MapLibre style document |
| decodedCacheBytes | 134217728 | Decoded source tile cache budget in bytes |
| maximumSourceZoom | 14 | Highest source MVT zoom, in the range 0–22 |
| automaticStyleZoom | true | Derive style zoom from Rocky's rendering camera |
| styleZoom | 12 | Manual/direct-call zoom; clamped to 0–22 and rounded to quarter levels |
| forceBufferedStrokes | false | Diagnostic CPU stroke-buffering override |

Common layer settings (name, open, attribution, opacity, profile and terrain
level limits) round-trip through their existing base serializers. Runtime atlas
resources, cached entries, counters and view snapshots are not serialized. Deserialization
performs no source I/O; ordinary layer opening resolves the style and sources.
Invalid provider JSON options fail on open before source access. The callback
`VectorTileLayer` remains programmatic: its common options can be restored, but
C++ producer callbacks are not a map-file source driver.

The serialization checks passed with Slughorn (12 terrain/style cases, 308 assertions)
and without it (six cases, 131 assertions). Coverage includes factory construction,
map round trips, defaults, disabled layers, invalid options, URI headers, relocation,
and local relative map/style/TileJSON/PBF paths. The ordinary rocky_demo also loaded
the bundled map from a different working directory and visibly rendered its vector
globe and attribution, without --openfreemap. Both builds used D:\vcpkg and cached
dependencies. The shared URI reader was corrected to read its own serialized header pairs.

### Shared content caching

Raw URI responses use IOOptions::services().contentCache. The default VSG context
retains its existing 256-entry in-memory LRU. For persistence, install a
FileContentCache before opening layers:

    #include <rocky/FileContentCache.h>
    app.io().services().contentCache =
        std::make_shared<FileContentCache>("rocky-cache", 256);

The equivalent map-file setting is services.contentCache.directory, with optional
memoryEntries (default 256; zero selects disk-only storage). It is a top-level
peer of map, profile and terrain, and round-trips with MapNode::to_json().
Relative directories use the process working directory. Settings affect all
users of that shared Services instance and must be configured before I/O workers
start. Maps without this setting preserve the application cache, including
custom ContentCache implementations. Custom implementations are not serialized.

Application accepts --cache <directory> before map loading; the explicit command
line overrides the map directory. The former --vector-cache spelling remains an
alias. Both now apply to every URI reader using those services, including styles,
TileJSON and PBFs. The layer-specific cacheDirectory property was removed; old
map files receive a configuration error explaining the replacement.

FileContentCache stores content bytes, MIME type and timestamps in a private
rocky-content-v1 subdirectory. Records contain the original URI key plus an
integrity checksum; malformed records are cache misses. Writes publish complete
temporary files, disk failures retain the memory-cache entry and log once, and
clear() removes only this cache implementation's records. The older vector-specific
.pbf entries are left untouched and are not automatically imported.

The memory tier is entry-bounded; disk storage currently has no eviction or HTTP
freshness/revalidation policy. Cached content is reused until cleared. Individual
disk records are limited to 256 MiB of content. These limitations belong to this
cache implementation, not the vector provider; applications may substitute their
own ContentCache.

The vector layer separately retains its decoded geometry/attribute cache under
decodedCacheBytes. Its reads counter now counts PBF URI reads that missed the
shared content cache; hits includes decoded-feature and shared-content hits.

### Offline verification

By default, `rocky_demo_vector_tiles` is offline: checker imagery, a polygon with a
hole, a translucent overlap, and road casing/core geometry crossing tile edges.
Vector data stops at LOD 3; terrain can refine to LOD 7.

```
rocky_demo_vector_tiles
rocky_demo_vector_tiles --verify --debug
rocky_demo_vector_tiles --delay 100 --verify --debug
```

`--churn --delay 1000 --verify` checks cancellation during repeated camera moves.
`--image-delay 1000 --delay 100 --verify` checks that vectors can appear before imagery.
`--verify` checks visible-surface publication, shared resources during overzoom, revision
replacement, and valid empty coverage. `--delay` adds producer latency in
milliseconds. `--frames N` runs a bounded interactive smoke test. Unit tests use
the `[terrain-vectors]` tag.

For the current Windows worktree, PowerShell can launch with the correct cached
DLL and PROJ/GDAL data paths:

```powershell
$env:BUILD_DIR = 'D:\workspace\build-rocky-vector-textures'
& D:\workspace\rocky-vector-textures\build\run-vector-tiles.bat
```

The helper reads the dependency directory/triplet from that build's CMake cache.
It accepts the demo options above, including `--verify`.

The implementation requires Slughorn. Builds without it retain ordinary terrain
rendering and return a service-unavailable error when opening a vector layer.
The terrain layout and node structure change the ABI (65 to 66).

### Engine paging integration validation (2026-11-18)

Built and installed with Slughorn enabled and disabled, using D:\vcpkg and the
existing x64-windows-release dependencies. The focused Slughorn suite passed
46 cases / 311,744 assertions; the applicable no-Slughorn subset passed four
cases / 38 assertions. Added checks cover camera-derived style scale and
immutable request zoom snapshots without dirtying the shared layer.

The ordinary rocky_demo rendered the Washington, DC OpenFreeMap view using its
normal render-on-demand application loop, with no per-frame demo styling or
loading callback. Verification also runs on demand. Idle updates preserve the
last recorded vector demand; terrain paging transitions explicitly request a
frame so asynchronous completion can advance the terrain.

Both offline stress checks passed with Vulkan validation enabled:

- Repeated camera changes canceled 63 obsolete vector requests, and the final
  view converged. Revision replacement removed all visible populated coverage.
- With imagery delayed 1,000 ms and vectors delayed 100 ms, vectors appeared
  before imagery; final resources and subsequent empty coverage converged.
- Both runs checked source-ancestor sharing and overzoom. The known device-layer
  setup and debug-messenger teardown validation errors remain. The delayed-image
  run also logged a compile warning during shutdown after verification passed.

The OpenFreeMap navigation/cache check also passed in render-on-demand mode:
40 visible surfaces converged at style zoom 13.5, then 40 at 14.5 after pan/zoom.
It built 53 destination resources, reused cached source data (222 hits, no PBF
fetches), and emitted 3,821 GPU stroke pieces with zero buffered batches. The
ordinary rocky_demo remains open with the layer enabled.

These checks establish engine scheduling and lifecycle behavior, not a frame-time
or GPU-memory budget guarantee. Expensive fill preparation and incremental
restyling remain work items. Visible-surface counts in the new checks exclude
resident ancestors/siblings, so they are not directly comparable to older
whole-traversal convergence counts below.

### Validation before engine paging integration

- MSVC 2022, cached dependencies from `D:\vcpkg` / the existing Rocky installed
  dependency tree, RelWithDebInfo with `/Z7` (the environment's shared PDB server
  reported C1902 with `/Zi`).
- 43 focused regression cases passed (311,665 assertions), covering terrain
  vectors, style expressions/MVT, Slug, textures, terrain anchors, shaders and
  tile keys. The style/terrain-vector subset also passed without Slughorn
  (29 assertions in three cases).
- RTX 5080 runtime verification passed with ordinary and delayed producers:
  130 terrain tiles, 74 shared resources, 56 overzoomed tiles. Revision replacement
  reduced populated tiles from 70 to zero. Visual inspection confirmed the hole,
  alpha overlap and continuous road boundaries.
- Builds and installation with Slughorn enabled and disabled both succeeded.
- The full suite is not green: the TMS/vertical-datum failures and subsequent
  heap-corruption termination also reproduce with the existing main-build tests.
  Vulkan debug mode reports device-layer setup and debug-messenger teardown
  issues in existing infrastructure; the vector run reports no descriptor,
  shader, synchronization or vector-resource lifetime validation errors.

## Styled MVT implementation

`MapboxVectorTileLayer` now supplies this terrain carrier with real XYZ MVT.
`MapboxStyle` is a separate immutable style plan with no renderer or network
dependency. It consumes a documented subset of version 8 Mapbox/MapLibre JSON.
GDAL decodes MVT into typed attributes and immutable WKB. Slughorn remains the
analytic coverage renderer; no decals, projected overlays, or rasterized vector
color textures are involved.

```cpp
auto vectors = MapboxVectorTileLayer::create();
vectors->styleURI = URI("openfreemap-terrain.style.json");
vectors->maxDataLevel = 14u;
app.mapNode->map->add(vectors);
```

Rocky derives style scale from the rendering camera, viewport and view-center
ellipsoid/plane intersection. It supports geocentric and projected maps,
perspective and orthographic cameras, with no MapManipulator dependency. Projected
views convert the local map scale to Mercator for style-zoom evaluation. The
highest zoom among main views sharing a TerrainNode is used for its shared tile
resources; shadow cameras do not affect styling. Distinct terrain nodes capture
independent zoom snapshots even when sharing a layer. Invalid/off-map camera
samples preserve the last valid scale. The evaluator accepts continuous zoom;
artwork requests quantize to quarter levels. automaticStyleZoom=false opts into
the explicitly assigned setStyleZoom() value. Direct createTile calls without a
style snapshot also use that explicit value. Ordinary engine camera changes do
not dirty the layer or mutate its global style state.

| Quantity | Meaning |
| --- | --- |
| Terrain LOD | Destination geometry/imagery refinement; independent of source detail |
| Source zoom | XYZ Mercator detail, bounded by destination footprint, floor(style zoom), TileJSON availability, and `maximumSourceZoom` |
| Style zoom | Visibility, filters, colors and pixel widths; quarter-level artwork invalidation |
| `maxDataLevel` | Highest destination artwork level; descendants share the ancestor's vector resource |

TileJSON and inline `tiles` sources are supported, including relative URLs and
attribution. A 128 MiB estimated decoded-data LRU shares source tiles across
destination requests and style changes. Raw PBF responses use the shared URI
content cache described above. Active work may retain decoded tiles
beyond the LRU budget; terrain atlas/GPU resources have no independent byte budget.

The source provider gathers overlapping Mercator tiles with a 128-style-pixel
guard. It clips fragments to their source tile's ownership rectangle, dissolves
contiguous equal-paint polygon batches, and adaptively reprojects fill contours
into destination UV. Solid round lines retain centerlines in a local, isotropic
Mercator frame and use Slughorn GPU distance stroking. Separate road casing/core
passes both use this path. Gap strokes retain buffered polygon coverage.
Polygon holes and multipolygons remain grouped. Invalid quantized polygons are
repaired once during decoding. Dense coverage is recursively partitioned into
small spatial pieces before atlas construction. Pieces share a paint group:
the terrain shader sums their coverage, clamps it, then applies alpha once.
This avoids split-edge seams while retaining Slughorn's band-capacity/performance
limits and giving the tile-local bins smaller bounds. WGS84 and spherical-Mercator
terrain grids retain their specialized paths. Other destination profiles use
Rocky SRS operations to reproject into tile UVs, including QSC cube faces and
plate carrée. Longitude wrap is handled with shifted world copies; MVT coverage
stops at the Mercator latitude limit.

Pixels correspond to a 512-pixel Mercator world at zoom zero. Evaluating strokes
in Mercator coordinates prevents longitude/latitude aspect distortion. This is a
focal-point approximation, not a screen-space constant-width guarantee across
an oblique globe view. Source geometry can also change at source-LOD boundaries.
Exact view-dependent widths, coordinated LOD transitions and fully incremental
source/style evaluation remain future work.

### Destination profile reprojection

The generic path computes a geographic footprint for each destination tile and
gathers overlapping Mercator source tiles. Before forward projection, it clips
coverage to the footprint with a small fringe so wide source guards do not cross
the destination projection domain. Projected edges subdivide adaptively to a
1/8192 tile-UV tolerance, checking quarter points and the midpoint. Polygon holes
remain attached to their exteriors. Geometry is clipped and spatially partitioned
in destination UVs before the normal analytic atlas preparation.

Solid round strokes retain GPU centerlines when nine samples across the tile
show nearly uniform, orthogonal scale (one-percent checks). Otherwise, source
Mercator buffering supplies the stroke outline before adaptive reprojection; this
preserves width under anisotropic distortion. This fallback is vector geometry,
not a rasterized texture. Gap strokes and the diagnostic forceBufferedStrokes
option continue to use buffered outlines.

Reprojection runs in the existing terrain workers and shares the decoded/PBF
caches across profiles. Changing the map profile rebuilds terrain demand without
reopening the vector layer. Invalid projection domains or exceeded subdivision
limits fail the request with a diagnostic under the existing coverage-retention
policy. This does not promise support outside a projection's valid domain.

### GPU stroke path

The terrain shader maps carrier UV and its derivatives into each stroke's local
Mercator frame, then evaluates distance to centerlines using Slughorn's curve and
band textures. Geodetic tiles use a local Mercator difference formula to avoid
subtracting large world coordinates at fine LODs. Mercator terrain uses an affine
mapping. Tile-UV procedural lines also use GPU strokes. Round caps and joins arise
from distance coverage; non-round style semantics retain the existing explicit
approximation diagnostics. Unsupported dash/pattern/offset layers remain omitted.

The Slughorn adapter uses the centerline bounds padded by the stroke envelope
for both band-capacity preflight and atlas construction. This replaces fixed unit
metrics, which clipped recentered paths at zero and could lose boundary caps.

Centerline batches contain at most 257 points, with shared endpoints at splits.
Equal-paint line chunks combine by maximum coverage before alpha blending, so
translucent intersections and chunk boundaries do not darken. Disjoint polygon
partitions continue to sum coverage. Source ownership clipping and the guard
preserve continuity across terrain and MVT tile boundaries.

Paint records are 96-byte std430 records containing color, actual stroke radius,
coordinate mapping, coverage group and atlas metadata. Atlas geometry is weakly
cached by exact serialized input geometry. Colors and actual GPU stroke widths
are independent of the atlas: widths in the same power-of-two envelope reuse
curve/band descriptors, with conservative bins and bands covering that envelope.
Crossing an envelope or changing centerlines rebuilds the atlas. Resource snapshots
are immutable; the weak cache does not prolong atlas lifetime. Source guards stay
fixed within each integer zoom interval, supporting reuse between quarter-zoom
paint updates. Tile production still evaluates styles and clips source geometry.

Use `--buffered-strokes` to force the original CPU buffering path for comparison.
Verification output reports GPU stroke pieces and buffered stroke batches.

### Supported style subset

- Ordered `background`, `fill` and `line` layers; `source-layer`, `filter`,
  `minzoom`/`maxzoom`, visibility, colors, opacity, line width and gap width.
- Typed properties, IDs and geometry types; legacy comparison/membership filters.
- Expressions: `get`, `has`, `id`, `geometry-type`, `zoom`, `literal`, comparisons,
  `in`, `all`, `any`, `!`, `match`, `case`, `coalesce`, `step`, linear/exponential
  `interpolate`, numeric conversion/assertions, basic arithmetic and min/max.
- CSS hex, RGB/RGBA and HSL/HSLA colors, plus black/white/transparent. Color ramps
  interpolate encoded RGB, then convert to linear RGB for terrain composition.

Unsupported paint properties or expression operators omit the affected layer
with a diagnostic. Unsupported layer types are diagnosed. Caps and joins are
approximated as round and reported; explicit fill outlines/antialias controls are
reported as ignored. Symbols, labels, sprites, patterns, dashes, offsets,
extrusions, legacy stop-function objects and the full CSS named-color vocabulary
are not implemented. There is no automatic raster fallback. Runtime type or
geometry failures preserve existing terrain coverage and report the error.

Equal-paint batches use union coverage, including overlapping features; this
avoids duplicate source-edge alpha but does not reproduce every per-feature
transparency interaction of a full MapLibre renderer. The first release is a
terrain-surface style subset, not a drop-in MapLibre implementation.

### OpenFreeMap demo

The bundled `data/openfreemap-terrain.style.json` is standard style JSON covering
land cover, parks, water, buildings and road casing/core passes. It intentionally
contains only supported operations. The default view is Washington, DC.

```powershell
$env:BUILD_DIR = 'D:\workspace\build-rocky-vector-textures'
& D:\workspace\rocky-vector-textures\build\run-vector-tiles.bat --openfreemap --cache D:\workspace\openfreemap-cache
```

Additional options:

```
--style <local-file-or-URL>   # different standard style, with omission diagnostics
--lon <degrees> --lat <degrees> --range <kilometers>
--verify                    # wait for traversed resources, pan/zoom, verify replacement and cache reuse
--profile-cycle             # with --verify: plate carrée, QSC, Mercator, then geodetic
--keep-open                 # keep the styled map open after successful verification
--frames <count>             # bounded interactive run
```

`--verify` requires populated detailed resources, current revisions on every
currently drawn surface, and source cache reuse after navigation. Cached subtrees
outside the traversed view may retain old revisions until visited again. The title credits
OpenFreeMap, OpenMapTiles and OpenStreetMap; the console prints provider
attribution. Applications must display their own appropriate attribution.

The `[mapbox-style]` tests cover expression/filter semantics and a self-contained
binary MVT fixture, including typed attributes, holes, GPU and buffered lines,
reprojection, style zoom buckets and cache reuse. No network is needed for tests.

### Styled-map validation (2026-11-18)

The default OpenFreeMap demo passed `--verify --debug` on the RTX 5080:

| Checkpoint | Retained tiles | Traversed tiles with current coverage | Overzoomed tiles |
| --- | ---: | ---: | ---: |
| Initial view, style zoom 13.5 | 118 | 118 | 0 |
| Pan/zoom, style zoom 14.5 | 166 | 134 | 48 |

The final run used warm PBF caches: 987 cache hits, zero source-file fetches,
204 destination tiles built and 126.75 MiB retained in the decoded LRU.
Feature counters count styling visits, not unique features. Source TileJSON was
read over HTTPS; earlier runs also exercised direct HTTPS PBF downloads.
Visual inspection confirmed complete land/water, buildings, roads and parks.
No vector-resource, shader or synchronization validation errors appeared;
the existing device-layer setup and debug-messenger teardown errors remain.
The offline imagery-plus-vectors verification also passed after the coverage-group change.

Full convergence is expensive: 113 seconds initially and another 148 seconds
after navigation in this debug run, even with warm data. Reported frame intervals
were about 16 ms at the checkpoints; these are presentation-paced intervals,
not isolated GPU benchmarks. Visibility-aware scheduling and incremental
restyling should precede treating this as a production basemap implementation.

An unmodified OpenFreeMap Liberty style also passed a load/startup smoke check:
65 paint layers accepted, with explicit diagnostics for omitted/approximated
features. This was not a full Liberty rendering-conformance test.

Specification references: [MapLibre style](https://maplibre.org/maplibre-style-spec/),
[GDAL MVT driver](https://gdal.org/en/stable/drivers/vector/mvt.html), and
[OpenFreeMap quick start](https://openfreemap.org/quick_start/).

### GPU-stroke validation and comparison (2026-11-18)

Built with `D:\vcpkg` and the existing `x64-windows-release` dependency cache.
The Slughorn build passed 45 focused cases / 311,727 assertions, including
Mercator coordinate records, GPU stroke dispatch, padded bands outside the unit
square, translucent paint grouping, width-envelope/color reuse, envelope
replacement, and the explicit buffered/gap fallbacks. The build without Slughorn
also passed its applicable three style/input cases (29 assertions).

Both paths passed the default OpenFreeMap `--verify --debug` navigation sequence
on the RTX 5080, using warm PBF caches and the same source footprints and styles:

| Path | Initial convergence | Additional pan/zoom convergence | Total |
| --- | ---: | ---: | ---: |
| CPU buffering (`--buffered-strokes`) | 145.865 s | 115.223 s | 261.088 s |
| GPU strokes | 30.210 s | 61.619 s | 91.830 s |

Both built 204 destination tiles with 295,563 styling visits, 1,007 cache hits
and zero PBF fetches. The GPU run emitted 15,281 stroke chunks and zero buffered
stroke batches; the reference emitted 6,278 buffered batches. Visual inspection
confirmed continuous roads, casings, roundabouts and park paths after fixing the
adapter's unit-square band clipping. The final view retained 166 tiles, with
134 traversed tiles current and 48 overzoomed tiles sharing ancestor resources.

These are single-run end-to-end convergence measurements, not isolated CPU or
GPU timings. Checkpoint frame intervals were approximately 16–17 ms for GPU
strokes and 15–17 ms for buffering, with presentation pacing. The final GPU run
reported only the existing device-layer setup validation error; the reference
also reported the already-known debug-messenger teardown error. The interactive
GPU demo was left open after successful verification.

The earlier 28/68-second development run used incorrect fixed stroke bounds and
is superseded by this final result. Fill union/partitioning, source evaluation
and clipping still contribute substantially to zoom-update cost; GPU stroking
does not eliminate that work or provide fully incremental styling.

### Terrain stroke antialiasing (2026-11-18)

The dominant visible stair-stepping came from the tile-local Mercator conversion:
sin(delta/2) lost relative precision at tiny angles on the tested GPU, and the
large tile normalization amplified the error into visible coordinate steps.
For |delta| < 0.01 radians, a fifth-order small-angle polynomial now preserves
relative precision; larger angles retain sin(). A 100,000-point float32 sweep
against double-precision sin gave a maximum relative error below 1.2e-7. The
same close-up at range 1.4 km visibly changed from stepped road casings and park
paths to smooth edges with GPU strokes still enabled. This projection fix also
benefits fills; it does not change the Windows DPI settings.

Terrain polyline strokes now use the shared Slug path-pass coverage function,
with analytic screen rates for each segment's body and round ends. The previous
terrain path used a scalar distance ramp with an average of the two coordinate
footprints, losing edge orientation and anisotropy. Band traversal now expands
by the actual footprint along the band axis, including its antialiasing fringe.
Derivatives still originate outside divergent atlas traversal; no CPU stroke
buffering or raster texture is introduced. This evaluator is for the terrain
provider's straight polyline segments; quadratic strokes would need an extension.

Rebuilt and installed with D:\vcpkg and the existing dependency cache. All 45
focused cases / 311,727 assertions passed. A transient GPU comparison exercised
identical shallow diagonals, subpixel lines, round caps and anisotropic mappings
through the old and new coverage functions. The OpenFreeMap navigation/cache
verification (before the final projection precision change) passed with 15,281 GPU stroke pieces and zero buffered batches;
initial convergence was 28.027 seconds, total 100.712 seconds. The comparison
window ran concurrently for part of this check, so these are not performance
comparison measurements. Windows DPI behavior is unchanged.

### Profile validation (2026-11-18)

Built and installed with Slughorn enabled and disabled using D:\vcpkg and the
existing dependency cache. The terrain/style suite passed 14 cases / 505
assertions; the no-Slughorn subset passed six cases / 133 assertions. New checks
cover all six cube faces, plate carrée, projected camera style scale, GPU-stroke
eligibility, and exact fill/hole placement near both poles and both date-line
sides using local binary MVT fixtures.

The live OpenFreeMap verification passed the complete geodetic -> plate carrée
-> QSC -> Mercator -> geodetic cycle and subsequent pan/zoom. Visible resources
converged after every change. It built 113 destination resources with 809 cache
hits and zero PBF fetches, emitting 4,683 GPU stroke pieces and 2,394 buffered
batches. This run did not enable Vulkan validation and is not a performance
benchmark. Ordinary rocky_demo also visibly rendered vectors after switching
to QSC, plate carrée and back to geodetic through Demo_Views.

### Shared-cache validation (2026-11-18)

Built and installed with Slughorn enabled and disabled using D:\vcpkg and the
existing dependencies. All 19 focused cases / 576 assertions passed with Slughorn;
the no-Slughorn subset passed 10 cases / 192 assertions. Application checks also
verify --cache and its --vector-cache alias take precedence over map settings.

The persistence tests cover binary payloads, MIME types, timestamps, memory eviction,
record corruption/replacement, concurrent URI reads, and shared map-file configuration.
A fresh vector layer with its decoded cache disabled can reopen its style and
TileJSON and rebuild MVT coverage after all three source files are removed, using
only a newly constructed shared disk cache. No network is involved in these tests.

### Remaining work

Visibility-aware scheduling is now part of terrain paging. Next prioritize bounded GPU atlas memory, then
continuous screen-space stroke widths and additional paint operations. Labels
need a separate view-dependent placement/collision pass; extrusions need geometry.
This keeps the terrain carrier useful for MVT, local features and generated
artwork without coupling these sources to the decal pipeline.
