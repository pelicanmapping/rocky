# Unified ECS picking prototype

```cpp
#include <rocky/vsg/ecs/ECSIntersector.h>

rocky::ECSIntersector picker; // Reuse for any associated View.
auto entities = picker.intersect(view, x, y);
```

`ECSIntersector` is stateless and has no `Application` dependency or view-bound
constructor. Each `intersect` call returns a fresh `Results` (`std::unordered_set<entt::entity>`), with all ordinary
geometry hits plus at most one vector-overlay instance. Results are deduplicated,
unsorted and not ordered by depth. No previous hits carry over to the next call.

An optional fourth argument sets the geometry search radius in window pixels
(default 3; negative values act as zero). Vector coverage remains a precise query
at the cursor pixel, not a rectangular or all-overlay-hits query. Its contribution
threshold is 0.01. Invalid views, missing cameras and cursor pixels outside the
viewport return an empty set. Missing terrain or unready vector state simply omits
the vector result; ordinary geometry picking still works.

The supplied `View` is the entry point. Ordinary intersections traverse its actual
scene children, rather than an application's shared scene. The view caches observer
pointers to its compute branch, that branch's `DecalSystemNode`, and its scene's
`ECSNode` (for the registry used to validate geometry results). A fourth observer
records which underlying VSG view those associations describe, preventing a replaced
raw view from using the old registry. These references do not keep a compute graph
or system alive. Queries promote them only for their synchronous duration; there is
no per-query graph search to locate the decal system or registry. Participating ECS
nodes within a view must use the same registry, since results contain bare entity IDs.
Entity markers are scoped to scene branches so unowned terrain, diagnostic or UI
geometry cannot inherit a preceding ECS entity's identity.

`Application::onAddView` associates the shared compute graph for initial views,
additional views, and views added to later windows. Several views can observe the
same compute branch, while each retains its own scene and renderer submission state.
The normal `View` constructor also associates ordinary ECS geometry when no compute
branch is present. Standalone graph assembly uses:

```cpp
view.setComputeGraph(computeGraph); // nullptr for ordinary geometry only
```

Call this setup method again after replacing the scene, compute branch, or systems
inside that branch, even if the graph pointer is unchanged. `View::dirty()` also
refreshes these associations. View copies copy the observers; refresh every retained
copy used after reassembly. Merely keeping an old system alive elsewhere does not
keep an expired compute branch pickable. Missing/expired compute state omits only
vector results; missing scene ECS context omits ordinary entity results. Replacing
`view.vsgView` without refreshing its association returns an empty result.

Callers do not locate systems, query terrain, filter projected artwork or merge
separate intersection results. The polytope visitor is an implementation detail in
`rocky::detail`, declared in ECSVisitors.h; the line-segment visitor remains public.
The overlay-specific helper remains a private implementation detail of DecalSystemNode.

The Intersection demo stores one intersector and its picking call is only
`hits = intersector.intersect(view, e.x, e.y, buffer);`. Existing hover tint, Pulse,
and color controls remain intact. Highlights belong to the returned projected
instance, including when multiple instances share a payload or projector.

## Coverage and publication

The C++20 Slughorn adapter decodes each unique packed shape once during atlas
construction. An immutable `SlugCoverage` interface owns the decoded curves,
bands and indirection tables. Published layer metadata shares these objects;
queries neither decode nor copy atlas data. Replacing or deleting a resource
releases its old coverage cache through ordinary shared ownership. No Slughorn
SDK types enter Rocky's C++17 API.

The adapter evaluates `renderSampleBanded` with reciprocal em-per-pixel widths.
It first applies Rocky GLSL's outer-band rejection, which the SDK sampler does
not perform. Nonfinite inputs and degenerate footprints return zero coverage.
Holes, stroke caps/joins, outline rings and points use the same authored curves
as rendering, rather than testing authoring polygons or their bounding boxes.

Decal submission captures each instance's effective world transform after optics,
terrain focal placement, receiving-depth adjustment and facade sizing. The CPU
inverse remains double precision. Only ready, resident vector submissions are
pickable; perspective vector projections remain unsupported just as in rendering.
The snapshot records the resource revision. Queries reject removed instances,
changed payloads, stale atlas revisions, unready descriptors, hidden instances,
explicit raster mode and capacity-driven raster fallback.

## Receiver, footprint and ordering

A synchronous resident-terrain intersection supplies the receiver point and
triangle normal. Adjacent pixel rays intersect that triangle's tangent plane to
estimate the screen-space footprint. World/projector calculations remain double
precision until sampling the bounded shape coordinates. The layer UV-to-em rows
transform both the point and footprint; widths use `abs(dx) + abs(dy)` as in GLSL.
This is an approximation at terrain silhouettes, discontinuities and strong
perspective variation. It rejects degenerate grazing configurations.

The query reproduces the screen-cell frustum cull and its bounded submission
list on the CPU. Raster volumes consume the same capacity even though they are
not pickable. It visits cores in reverse submission/layer order, then outlines
in reverse order, matching rendering's global outline pass followed by cores.
The result is the last vector layer whose `coverage * layerAlpha * instanceAlpha`
meets the internal 0.01 threshold; it is not a sum of alpha across layers.

## Scope and threading

Call on the application thread, serialized with ECS update, without holding the
registry lock. The query uses the most recently submitted per-view projections.
A camera change requires another submission before its cell lists are valid.
Before the first submission, without resident terrain, or without Slughorn,
there is no vector hit. Geometry results are still returned. This API performs no
GPU readback or GPU picking.

Terrain receiver coverage and scene occlusion are separate operations. The API
does not test whether a model, other geometry, or a raster decal obscures the
terrain hit. Applications needing frontmost visible scene selection must resolve
that occlusion separately. Raster coverage, rectangular/all-overlay-hit queries
and new model-picking/highlighting behavior are outside this prototype. Ordinary
geometry retains the existing polytope visitor behavior.

Ordinary ECS intersectors continue to pick main-view geometry. They skip Overlay
artwork and entities with `RenderParticipation.mainView == false`, preventing
source artwork from masquerading as a projected hit.
