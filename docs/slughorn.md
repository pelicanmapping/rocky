# Slughorn dependency

Experimental vector overlay support is enabled by default
(`ROCKY_SUPPORTS_SLUGHORN=ON`); disable it with `-DROCKY_SUPPORTS_SLUGHORN=OFF`.
Rocky does not embed the Slughorn SDK sources in its repository.
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
geometry, style, or relevant authoring inputs change; automatic projector refits
and creation of the Raster mesh do not repeatedly retry the failed atlas.

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
