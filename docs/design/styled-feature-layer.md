# Styled feature layers for terrain

Status: implemented, 2026-11-19.

## Implemented scope and use

`StyledFeatureLayer` and the `StyledFeature` factory now accept URI or inline styles and the
`sourceBindings` configurations below. This is the single public styled layer class and factory;
map files use `"type": "StyledFeature"`. ABI is incremented to 67.

GDAL dataset bindings accept local datasets, including shapefile, GeoPackage, standalone FlatGeobuf,
and GeoJSON. Spatial queries preserve field names, typed scalar values, nulls, multipart geometry,
and polygon holes. Each query owns a GDAL cursor. Datasets without fast spatial filtering use a
one-time bounded import with an X-ordered envelope index; default import budget is 32 MiB. Native
dataset indexes are preferred. Opening does not create indexes or alter the dataset.

TiledFeatures supports FlatGeobuf and MVT, XYZ/TMS addressing, named non-composite Rocky source
profiles, and whole-feature deduplication through `idField`. MVT decoding requires Mercator tiles.
Tile footprint CRS and feature CRS are independent. Whole-feature IDs must uniquely identify features
within the source collection. Data is immutable while open; close/reopen to reload it.

Default query limits are 100,000 features, one million vertices, 128 MiB of decoded results, and
64 source tiles. Raw tile responses are limited to 32 MiB. Oversized requests fail explicitly rather
than returning incomplete artwork. The decoded cache budget is divided among bound tiled sources;
unbound MVT retains its existing layer cache. GDAL dataset import budgets are separate.

Run the offline example in the normal application:

```
rocky_demo --map data/styled-features.map.json
```

It reads `styled-features.geojson` through GDAL, using an inline style. Change the binding's URI to
a shapefile/FGB/GeoPackage; select `layer` if the dataset contains multiple layers. The fixture is
synthetic geographic artwork around the eastern United States, not a basemap.

The architecture below remains the design rationale. Follow-up work includes automatic overview
generation, topology-safe simplification, remote standalone datasets/range I/O, configurable sparse
tile conventions, source bounds, composite source addressing, and dynamic style/source editing.
These are not silently accepted as source-binding options. No disk-cache folder was added to a source.
Use the existing shared services content cache for downloaded styles, TileJSON, and tile files.

Normal map-file paths are relative to the referring map; external style paths retain style-relative
resolution. Single-collection GeoJSON declarations require a binding in this first implementation;
the empty FeatureCollection is a standard style placeholder, not inline dataset import support.

Paint support remains background/fill/round line; labels, symbols, extrusions, and feature Z rendering
are not added. Very broad coarse tiles still have bounded detail; prepare large datasets for low zoom.
Geometry crossing the date line must use a consistent GIS representation; addressing wraps do not
repair an ambiguous ring that traverses 358 degrees of longitude.

### Review validation

Both RelWithDebInfo builds/installations passed using D:/vcpkg and the existing dependency cache.
Focused tests: 27 cases / 785 assertions with Slughorn, and 13 cases / 293 assertions without it.
The fixtures exercise shapefile, GeoPackage, indexed/unindexed FGB, typed/case-sensitive fields,
concurrent queries, budgets, inline serialization, XYZ/TMS addressing, deduplication, holes,
non-Mercator inputs, and polar output, alongside the existing MVT/cache/terrain tests.

A temporary verification hook in ordinary rocky_demo checked current published terrain resources
after geodetic -> plate-carree -> QSC -> Mercator -> geodetic transitions and a zoom change.
All stages converged: 721 vector tiles built, 676 styled feature contributions, 187 GPU stroke
chunks, and 135 buffered batches. This was an engine/publication check, not a visual inspection or
a representative large-dataset benchmark. The hook was removed and normal demo binaries rebuilt.

The ordinary rocky_demo was also inspected with Slughorn on 2026-11-19. The offline GDAL map
rendered its polygon with a hole and its styled line from the inline map JSON.

## Recommendation

Introduce `StyledFeatureLayer : VectorTileLayer`, configured by a Mapbox/MapLibre v8 style and named
feature-source bindings. MVT, tiled FlatGeobuf, and GDAL datasets share this layer and factory.

Use two source strategies: tiled feature collections and spatially queried datasets. Encoding is a
property of the source, not of the terrain renderer or public styled layer. Initial inputs are XYZ MVT,
XYZ FlatGeobuf, and a local shapefile; the dataset strategy also permits standalone FGB and GeoPackage.

The output remains the existing ordered `VectorTile` artwork. Terrain paging, Slughorn analytic
coverage, GPU strokes, texture composition, and terrain ownership remain shared. This is a surface
layer; feature Z values do not create elevated geometry. Symbols, labels, and extrusions are separate
rendering capabilities and are not added by this refactor.

## Fit with the current code

`FeatureSource` already exists in Feature.h, with `iterate(io)` and `featureCount()`. GDALFeatureSource
opens a separate dataset for ordinary iterators and now exposes spatial queries. Both paths preserve
attribute spelling and scalar types for case-sensitive style evaluation. The MVT decoder preserves typed attributes and
uses immutable WKB geometry records.

Reuse and extend these facilities instead of introducing an unrelated second feature-source system:

| Component | Proposed responsibility |
| --- | --- |
| StyledFeatureLayer | Resolve style/source bindings, snapshot revisions, orchestrate artwork generation |
| FeatureSource query capability | Return bounded, cancellable feature batches with coverage and identity |
| TiledFeatureSource | Select source keys, fetch bytes, decode, report tile ownership and availability |
| GDALFeatureSource | Spatial queries against a dataset, CRS/schema metadata, exclusive cursor ownership |
| MapboxStyle | Parse and evaluate supported standard expressions and paint operations |
| FeatureTileCompiler (internal) | Project, clip, simplify, preserve painter order, produce VectorTile |
| VectorTileLayer / terrain | Schedule, publish, inherit, and render artwork through the existing path |

Avoid exposing OGR handles in the new public query API. Use Rocky Feature/Geometry and typed fields as
the contract; immutable WKB may remain an internal optimization. Share conversion code and preserve
field spelling, null values, booleans, numeric types, feature IDs, multipart geometry, and holes.
Use the same attribute conversion for iteration and spatial queries; no case-folding compatibility mode is retained.

## Configuration and the standard boundary

The examples below are accepted by this implementation. Follow-up capabilities are identified above.

`style` accepts an existing URI string, a URI object with `href` and `headers`, or an inline style
document with `version`, `sources`, and `layers`. Reject ambiguous objects. Preserve the authored form
on serialization. Resolve inline-style URLs and sourceBindings paths against the map file; external
style URLs against the style file; TileJSON tile URLs against TileJSON.

`sourceBindings` is a Rocky layer option keyed by the standard style source ID. An explicit binding
replaces that source's loading configuration entirely; it does not partially merge incompatible
dataset and tile settings. Unbound standard vector sources continue to use MVT/TileJSON as today.
Unknown binding IDs and missing required sources are configuration errors.

Keep custom FGB/shapefile drivers out of the standard style's `sources` vocabulary. For a single
collection, declare a valid GeoJSON source with an empty FeatureCollection, then bind its ID to a Rocky
source. Its style layers omit `source-layer`, as required for GeoJSON. This preserves standard style
syntax and paint semantics, but the external dataset binding is Rocky-specific: another renderer sees
the empty collection until the application supplies equivalent data. Do not claim portable dataset
loading merely because the style is valid.

For MVT, existing vector sources and named `source-layer` selectors remain unchanged. Optionally bind
a single FGB/dataset collection to a named collection of an existing vector source when reusing an MVT
style; an explicit `sourceLayer` binding supplies that name. Reject schema/collection mismatches.
Multi-collection binding syntax can follow later; initially use multiple source IDs for multiple
dataset layers.

### Local shapefile: complete layer object

```json
{
  "type": "StyledFeature",
  "name": "Local roads",
  "sourceBindings": {
    "roads": {
      "type": "GDAL",
      "uri": "roads.shp"
    }
  },
  "style": {
    "version": 8,
    "sources": {
      "roads": {
        "type": "geojson",
        "data": { "type": "FeatureCollection", "features": [] }
      }
    },
    "layers": [
      {
        "id": "roads",
        "type": "line",
        "source": "roads",
        "layout": { "line-cap": "round", "line-join": "round" },
        "paint": { "line-color": "#e9b552", "line-width": 3 }
      }
    ]
  }
}
```

GDAL detects the driver and CRS from the dataset. Optional `layer` selects a dataset layer, such as a
GeoPackage table. Require selection when ambiguous. An explicit `srs` supplies or overrides CRS;
diagnose a disagreement and never silently assume EPSG:4326 when metadata is absent. A shapefile is
opened as a dataset with its companion files, not fetched as an isolated .shp byte blob.

### XYZ FGB: replacement for the roads binding above

```json
{
  "type": "TiledFeatures",
  "format": "FlatGeobuf",
  "tiles": ["https://example.org/roads/{z}/{x}/{y}.fgb"],
  "profile": "spherical-mercator",
  "scheme": "xyz",
  "minZoom": 0,
  "maxZoom": 14,
  "geometryMode": "tile-clipped"
}
```

`profile` describes tile footprints and root layout using Rocky Profile semantics. `scheme` describes
row numbering, independently of the file CRS. FGB coordinates use the file CRS, or explicit `srs` if
absent. Validate CRS consistency across tiles. Do not infer Mercator coordinates from XYZ filenames.
The first implementation supports ordinary quadtree source profiles; composite source grids need
an explicit addressing convention later. Existing composite destination profiles remain supported.

`geometryMode` documents the producer contract. For `tile-clipped`, geometry must include adequate
boundary context for the requested strokes, normally through tile buffers. For `whole-features`,
features duplicated into intersecting tiles require a stable `idField` for deduplication. Without
adequate geometry outside tile edges, exact joins cannot be recovered by a decoder. Validate/test
this against a real FGB pyramid before promising seamless rendering.

Expose bounds and an explicit sparse-tile policy as source options. Missing tiles count as empty
only for sources configured to use that convention. Corrupt files and transient I/O failures remain
errors. Data above maxZoom overzooms; data below minZoom is unavailable unless bounded gathering of
minimum-level tiles is explicitly enabled. Never fan out into millions of tiles for a root request.

### Existing MVT style

```json
{
  "type": "StyledFeature",
  "name": "OpenFreeMap",
  "style": "openfreemap-terrain.style.json"
}
```

No bindings are needed. Use `StyledFeature` for MVT as well as other feature sources. Source zoom
and decoded-cache budgets remain useful layer options; explicit bindings carry their own source configuration.

## Query contract

Add an optional query capability to FeatureSource without replacing `iterate(io)`. The new operation
must distinguish unsupported queries, valid empty coverage, failure, cancellation, and budget
exhaustion. Sources without query support must not silently fall back to an entire dataset scan for every tile.

The request contains one or more conservative spatial footprints with SRS, collection selection,
required attributes, target geometric resolution, source detail hint, and cancellation/budget limits.
Style zoom is a separate input to the compiler. The source need not interpret Mapbox expressions;
only provably equivalent attribute predicates may be pushed down, with final filtering in MapboxStyle.

Return an immutable bounded batch, or a stream of bounded batches, carrying source revision,
collection IDs, geometry CRS, feature identity, and coverage/completeness. Tiled sources additionally
report core ownership regions and available buffered geometry. A feature ID is scoped by source,
collection, and, where appropriate, source tile. MVT IDs are not assumed globally unique. Dataset
IDs must remain stable across different spatial queries; per-query counters cannot serve that role.

A spatial filter is a conservative candidate selector, followed by precise geometry handling.
Transform/densify the requested footprint into the dataset CRS, split at wrap/domain boundaries,
and include the style's stroke fringe. Do not use a four-corner rectangle transform as a general
coverage guarantee. Reject/diagnose transformations outside the source's valid domain.

## Tile generation and rendering

For each terrain request:

1. Snapshot style, source revision, destination profile, and camera-derived style zoom.
2. Identify visible style layers and required source collections/attributes. Compute a conservative
   query fringe from supported stroke widths. Attribute-dependent widths need a declared or derived
   bound; if that cannot be established, report the limitation instead of silently truncating strokes.
3. Fetch overlapping source tiles, or spatially query the dataset. Coalesce identical in-flight work.
4. Evaluate styles and compile features in style-layer painter order. Preserve deterministic feature
   ordering within each layer; do not group by color in ways that alter translucent overlaps.
5. Project adaptively and emit existing VectorTile artwork with tile ownership and boundary context.
6. Publish through the existing metered terrain update queue. Obsolete revisions are discarded.

Source tile LOD, terrain LOD, and style zoom remain distinct. A local dataset has no inherent source
tile zoom. Use target resolution for optional geometry generalization; style zoom continues to mean
the camera's established map scale. Switching destination profiles invalidates projected artwork,
but reusable decoded source features survive.

Remove source-Mercator assumptions from the shared compiler. Preserve the existing optimized
Mercator path, but allow arbitrary input CRS and valid polar coverage. Projection and width conversion
use destination derivatives or a suitable local metric frame. GPU strokes remain the default when
their width/shape semantics are correct. Gap strokes and sufficiently distorted cases retain the
buffered-outline fallback. Never buffer angular coordinates as though they were meters.

Clipping applies to painted coverage, with neighboring context retained for strokes; artificial
clip edges must not become line caps. Assign overlapping source buffers to non-overlapping core
coverage so translucency is not painted twice. Deduplication by stable identity is a separate concern.

## Local datasets, LOD, and responsiveness

Use the dataset's existing spatial index where available. GDAL supports shapefile .qix and reads
ESRI .sbn/.sbx indexes; FlatGeobuf supports an embedded spatial index. Do not create sidecar files
in the user's dataset merely by opening it.

For an unindexed small dataset, permit a one-time, cancellable import into a byte-bounded in-memory
feature store and spatial index. Share that initialization among requests. If it exceeds the budget,
report that preparation/indexing is required. Avoid repeated full scans on terrain workers. Initial
budget defaults should be measured with representative datasets and explicitly configurable.

An index speeds spatial selection; it does not create coarse LODs. A world-scale query may still
select the whole dataset. Limit candidates, vertices, bytes, and source fan-out. On exhaustion,
preserve valid ancestor artwork where available and report the reason, rather than publish a random
truncated subset. The initial correctness path can use unsimplified geometry for bounded local data.

Add deterministic resolution-based simplification only with seam tests. Simplify before final tile
clipping at a consistent scale; adjacent cells must agree on shared geometry. Per-feature topology
preservation alone does not preserve shared parcel boundaries. Leave polygon coverage simplification
off initially unless a shared-boundary strategy is implemented. Large dense low-zoom datasets will
need prepared overviews/tiles; automatic persistent overview generation is a later feature.

## Scheduling, caching, and lifecycle

All work uses Rocky's terrain worker scheduling and publication path, not a standalone demo loop or
a second unbounded executor. Bound source concurrency; each active GDAL cursor owns an exclusive
dataset/layer handle, with bounded pooling if useful. Do not share an external OGR layer handle among
terrain workers. Cancellation is checked between read/geometry batches; individual driver calls may
not be interruptible. Closing a layer waits for or retires its active source state safely.

Keep the cache distinction explicit:

| Data | Owner and policy |
| --- | --- |
| Downloaded style, TileJSON, MVT/FGB bytes | IOOptions/Services contentCache, existing URI behavior |
| Open local shapefile/GPKG and indexes | GDAL filesystem access; no duplicate raw-file content cache |
| Decoded features / bounded dataset index | Source-owned byte-bounded memory cache, independent of style |
| Projected/styled artwork and GPU atlases | Terrain revision/profile/style keys and resource lifetime |

Do not add a private source disk-cache folder. Persistent derived indexes/overviews, if later added,
need a Services-level derived-artifact facility rather than overloading the raw URI content cache.
Remote range access for a large standalone FGB dataset is deferred: first support local datasets
and whole-file tiled URI reads, preserving shared authentication and cache behavior.

Source revision participates in every derived cache key. Start with immutable datasets while open
and explicit reload; fingerprint shapefile companion files for reopen invalidation. Style updates
retain decoded features but regenerate affected artwork. File watching and persistent preprocessing
are separate future work. Preserve current raw content-cache freshness semantics; do not promise
HTTP revalidation as part of this design.

## Delivery and acceptance

1. Extract the shared compiler and source-query boundary; keep MVT behavior and examples unchanged.
2. Add StyledFeature serialization, source bindings, and inline styles. Extend MapboxStyle source
   validation to distinguish named vector collections from single GeoJSON-style collections.
3. Implement the FGB tiled adapter and validate with a representative real pyramid.
4. Add spatial queries to GDALFeatureSource and demonstrate a local shapefile in ordinary rocky_demo.
5. Tune budgets and dataset preparation policy using measured low-zoom and rapid-navigation workloads.

Each phase should be independently reviewable. Do not combine a format refactor with new paint types.
Tests should cover geometry/attribute/query contracts and serialization, with interactive validation
in Rocky itself for rendering and responsiveness:

- Same fixture through MVT, tiled FGB, and local dataset: consistent filters, IDs, holes, and coverage.
- Boundaries, transparent overlaps, wide strokes, and repeated whole features; no seams or dark bands.
- Non-Mercator source CRS, geodetic/projected/QSC destination changes, date line and polar domains.
- URI and inline style round-trips, relative paths, bindings, and the StyledFeature factory.
- Indexed and unindexed datasets; budget exhaustion, cancellation, reload, corrupt and sparse tiles.
- Pan/zoom/profile changes alongside imagery in rocky_demo; bounded memory, handles, source requests,
  worker time, and update publication. Measure GPU strokes versus buffered fallback separately.

## References

- Existing code: Feature.h, GDALFeatureSource.h/.cpp, MapboxStyle.h/.cpp,
  StyledFeatureLayer.h/.cpp, VectorTileLayer.h/.cpp, and vsg/terrain.
- Existing design: [Terrain vector textures](terrain-vector-textures.md).
- [GDAL FlatGeobuf](https://gdal.org/en/stable/drivers/vector/flatgeobuf.html).
- [GDAL Shapefile](https://gdal.org/en/stable/drivers/vector/shapefile.html).
- [GDAL threading](https://gdal.org/en/stable/user/multithreading.html).
- [MapLibre source specification](https://maplibre.org/maplibre-style-spec/sources/).
