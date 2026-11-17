# Slughorn dependency

Vector overlay support is enabled by default
(`ROCKY_SUPPORTS_SLUGHORN=ON`); disable it with `-DROCKY_SUPPORTS_SLUGHORN=OFF`.
Rocky does not embed the Slughorn SDK sources in its repository.

`Overlay::mode` defaults to `OverlayMode::Vector`, including in builds without
Slughorn support. This is an intentional default while vector feature support
continues to expand; applications can explicitly choose `OverlayMode::Raster`.
When vector rendering is unavailable, Rocky uses Raster and emits a warning,
without changing the application's requested mode.

Unsupported styling does not suppress an otherwise valid vector overlay or
switch it to Raster. Rocky emits `Log()->warn` messages identifying each omitted
feature and renders the supported appearance: solid lines instead of stippling,
uniform style colors instead of per-vertex Line/Point/Mesh colors, and uniform
`PointStyle::width` instead of per-point widths. Mesh textures, stippling,
wireframe, and lighting are likewise omitted with warnings. These warnings occur
when changed source content is processed, not every frame; the caller's styles
and requested mode are left intact. Invalid geometry still fails validation,
and the capacity-related Raster fallback described below remains in effect.

When enabled with the VSG renderer, dependency resolution is:

1. `-DSLUGHORN_SOURCE_DIR=/path/to/slughorn` uses an existing source checkout.
   An invalid explicit path is an error; it never silently selects another SDK.
2. Otherwise, use an installed CMake package providing `slughorn::slughorn`.
   Set `CMAKE_PREFIX_PATH` or `slughorn_DIR` for a nonstandard installation.
3. Otherwise, automatically download a pinned, SHA512-verified source archive
   with CMake FetchContent and build the SDK in the build tree.

There is no separate fetching switch. If downloading is unavailable, supply a
checkout/package or disable `ROCKY_SUPPORTS_SLUGHORN`. CMake's standard
`FETCHCONTENT_SOURCE_DIR_SLUGHORN` override is also accepted, and its disconnected
controls apply to the FetchContent path. A fresh offline build needs sources or
an installed package; disconnected mode does not create a missing dependency.

## vcpkg

Rocky's `vcpkg-ports/slughorn` overlay port installs a static SDK with serialization
support. Enabling the Rocky option selects the manifest's `slughorn` feature
before `project()` invokes the toolchain. An explicit source override skips that
automatic feature selection. Existing Windows builds can continue using
`x64-windows-release`. The port is not Windows-specific.

## Without vcpkg

The same Rocky option automatically fetches/builds Slughorn if no package is
installed. The SDK requires a C++20 compiler; Rocky's public interface and
applications remain C++17. Its serialization dependency, `nlohmann_json`, comes
from the same installed package Rocky already requires. Python, pybind11, SDK
examples, and unrelated Slughorn backends are not built.

The small CMake project in `vcpkg-ports/slughorn` is shared by the port and both
source-build paths. It can also build/install Slughorn independently, without
vcpkg, by setting `SLUGHORN_SOURCE_DIR` when configuring that directory.

Source-built SDKs install their library, headers, license, and package config with
Rocky. Static Rocky installations additionally install the private adapter and
resolve Slughorn via `find_dependency`; the dependency remains link-only for
application compilation. Shared Rocky incorporates the static SDK and adapter,
so applications do not need a Slughorn DLL or its headers.

## Vendor update and experimental strokes

The pinned SDK is now `1e09656e5fe2da7a24c164fe8697187ebf28f673` (still reporting
SDK version 0.0.2). Source/package overrides must provide the new `Point` and
`canvas::Stroke` APIs; an older 0.0.2 package is not compatible with this adapter.

Baked outlines remain the default. Before constructing `Application`, set
`ROCKY_SLUG_STROKES=distance` to try centerline strokes. This selects matching
atlas and shader variants once per application; do not change the variant after
resources/pipelines have been built. Applications assembling their own systems
can insert `ROCKY_SLUG_DISTANCE_STROKES` into the context's shader compile
settings before initializing their systems and pipelines. `SlugSystem` consumes
that same setting, so producer and renderer cannot independently select modes.
Unset the environment variable or select `baked` to use the default.

The experimental variant stores road centerlines and widths instead of expanded
outlines, uses an exact segment-distance shortcut for straight roads, and enables
the vendor's stable quadratic-root solver. The default GPU coverage evaluator
retains its previous math: the stable solver showed a measurable slowdown in our
baseline workload. Translucent outlined strokes stay baked to retain ring
compositing. Distance-stroke core and casing layers have separate CPU picking
radii even though they share atlas bands. Existing band/texture capacity limits
and Raster fallback remain active.

The additional pass is integrated behind the startup opt-in described below.
Vendor fill batching remains excluded; `ROCKY_SLUG_FILL_BATCH` has no effect.
See [the integration review](design/slughorn-pr269-integration.md) for its scope
and measured performance limits.

`rocky_demo_slughorn` builds with Slughorn and GDAL. It supports the vendor's MVT
city scenes and a deterministic offline scene:

```text
rocky_demo_slughorn --synthetic --bench --strokes baked
rocky_demo_slughorn --synthetic --bench --strokes distance
rocky_demo_slughorn --synthetic --bench --no-roads
rocky_demo_slughorn --synthetic --bench --no-buildings
rocky_demo_slughorn --city tokyo --tiles /path/to/cached/tiles --bench
```

Use `--help` for timing controls. The benchmark reports GPU timestamps, frame
wall time, source geometry counts, ready atlas counts, and atlas bytes. It rejects
empty or unfinished vector scenes, rejects unsupported timestamp queues, and
bounds timestamp collection. Results marked `UNSETTLED` are not regression
measurements. Network-loaded MVT results require checking that the same complete
tile set loaded in both runs; `--synthetic` avoids that source of variation.

## Updating and testing

Polygon groups are partitioned into independent shapes within one atlas when
needed to fit the SDK's 16-bit band offsets/counts. An indivisible polygon (with
its holes or overlapping polygons), stroke, outline, point group, or Mesh fill
first tries 32, 16, then 8 bands per axis. Capacity recovery does not go below
8: excessively coarse bands make complex shapes too costly in the shader.
Small shapes that already fit retain their normal, possibly lower band count.
Checks use the actual SDK-generated curves, including rounded caps and joins, not just input
vertices. This preserves geometry at the cost of additional shader curve
testing. If any shape still cannot fit at 8 bands, its entire Overlay uses Raster
at its configured resolution. Band reductions and Raster fallbacks are logged
at info level when accepted, not every frame. Other overlays remain Vector, and the requested
`Overlay::mode` is unchanged. The fallback stays in effect until its source
geometry, style, relevant authoring inputs, or device dimension limit change;
automatic projector refits and creation of the Raster mesh do not repeatedly
retry the failed atlas. The complete curve and band textures are also checked
against the active GPU's `maxImageDimension2D` before creating upload images.
If either texture is too wide or tall, the same logged Raster fallback applies,
even when each individual shape fits the SDK's band limits.

The source revision and archive checksum are shared in
`vcpkg-ports/slughorn/slughorn-version.cmake`. Update those together and update the
port version in `vcpkg.json` inside that directory. Both acquisition paths use
the same pin; neither follows a moving branch. Explicit source/package overrides
are useful for testing newer SDKs but must remain compatible with Rocky's adapter
and shaders; an SDK version number alone does not establish atlas-format compatibility.

Run the `[projection]` unit tests after updating. They cover holes, row-spanning
band/curve addresses, polygon partitioning, and `.slug` serialization. Also run
the MVT and overlay demos: shader/atlas compatibility needs visual verification.

`src/tests/cmake/slughorn` is a standalone dependency smoke test. It builds the
real adapter behind a C++17 consumer, exercises atlas creation and serialization,
and exports the adapter for an installed-consumer check. Configure it with a
source override, an installed package prefix, or neither (to exercise downloading).
Only nlohmann-json and a C++20 compiler are needed; no Vulkan or vcpkg toolchain
is required.


## Opt-in additional rendering pass

Set `ROCKY_SLUG_PASS=1` before creating `Application` to request the additional
pass. The default remains the terrain renderer. This switch is independent of
`ROCKY_SLUG_STROKES=distance`, which selects the ordinary atlas encoding retained
for fallback and picking. Optional vendor fill batching is not enabled.

The additional pass draws opaque road segments in small receiving volumes and
uses atlas coverage for other vector layers. Every payload retains its ordinary
atlas and picking coverage. Its alternate geometry is immutable and shared
across placements and views; placement, highlight, depth bindings and camera
state belong to each view. Stationary views do not reupload segment geometry.
Adjacent draws of the same material are batched, preserving the original overlay
order and global outlines-before-cores rule. Fills are not moved ahead of roads
when doing so could change overlapping or translucent artwork.
Only changed payload revisions allocate/upload replacement buffers. Retired
buffers remain alive until their recorded uses are safe to release.

The initial supported scene is opaque terrain in a single-sample window, with a
VSG perspective or orthographic camera. Multiple independent windows and inset
viewports are supported. MSAA windows retain the terrain path and may coexist
with accelerated windows. A whole view falls back for mixed raster/vector
projections, projector opacity, perspective or reflected projectors, unknown scene geometry,
custom render targets, unsupported camera projections, mixed segment/generic
layers within one payload, or resource exhaustion. The device must enable
`shaderStorageBufferArrayNonUniformIndexing` and support the configured number
of storage-buffer descriptors plus the renderer bindings. Unsupported devices
retain terrain rendering.
The conservative Application policy also excludes terrain wireframe/triangle
visualization and sky/custom scene additions. This avoids drawing terrain-only
overlays onto other depth-writing objects or changing their composition order.
Fallback preserves the complete overlay stack and picking data.

`View::slugPassPolicy` stores the receiver/compositing contract, shared by copies
of the same logical view. `Application` supplies a conservative policy. Standalone
clients must configure sampleable single-sample depth through
`DisplayManager::configureTraits` before creating their window, and provide a
`fallbackReason` callback and enable the required storage indexing feature when
creating the shared device. Externally created devices without a known enabled
feature fall back. Return an empty string only while final depth identifies
compatible opaque terrain and the render sequence preserves decal composition.
Unknown/custom scenes should return a reason. Change the callback on the app's
update thread; do not replace the shared policy object after attachment. A pager
whose payloads contain only ECS artwork may explicitly declare
`rocky.terrainOverlayPayloadsOnly=true`; Application still checks its ECS artwork.
This declaration must not be used for arbitrary scene-graph payloads.

Call `View::dirty()` after changing its camera/viewport properties. Window
framebuffer recreation refreshes depth bindings automatically. Removing a view
also removes its additional graph and compilation contexts. Startup without the
request adds no pass buffers or sampled-depth usage; an MSAA-only display does
not generate alternate payloads.

The renderer-private diagnostics in `ecs/SlugPass.h` expose published instance
counts and the current active/fallback reason. `DecalSystemNode::slugPassStatistics`
reports GPU geometry bytes, peak bytes, upload bytes and upload count. The default
geometry budget is 256 MiB; each allocation also respects the device's storage-buffer range.
Capacity failure selects whole-view fallback and can recover after retirement.
These counters measure alternate geometry, not atlas textures or total GPU use.
Detailed Optics timing is separately opt-in with `ROCKY_OPTICS_STATS=1`.

Requesting the pass can still add cost when a view becomes ineligible: the final
Tokyo fallback measurement used about 1.1 ms more GPU time than the matched
control, with comparable mean frame time. The default-off path was unchanged.
See the [Tokyo benchmark report](design/slughorn-pr269-tokyo-benchmark.md) for
the measured gains, fallback cost and validation limits.
