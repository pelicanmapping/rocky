# Additional Slughorn pass integration plan

Status: proposed, awaiting user approval. Prepared 2026-11-10 on
`codex/slughorn-integration` at `8fb25a99`. This document authorizes no default
change; implementation and repeat benchmarking follow approval of this plan.

## Objective and scope

Integrate the vendor's additional pass while preserving Rocky's view lifecycle,
rendering and picking behavior, and scalability under paging. Retain the Tokyo
performance benefit where the pass is supported, without regressing the existing
path when it is disabled or a view needs fallback.

The [Tokyo measurements](slughorn-pr269-tokyo-benchmark.md) are the reference:
9.54 ms mean GPU time and 99.11 fps with the vendor pass, versus 20.57 ms and
47.72 fps for fresh vendor-default controls. These are stationary RTX 5080
results, not loading or general hardware guarantees.

Use vendor head `24f59cd628ee8c25577c91b8e6836459729fda20` as source material,
selectively adapted onto this branch. Preserve the branch's corrected picking,
transparent-outline fallback, and distance evaluator improvements. Exclude the
vendor's optional fill batching and unrelated changes.

Recommended first-release scope:

- Keep `ROCKY_SLUG_PASS=1` as a startup opt-in. No switch means today's branch
  behavior, including baked strokes by default.
- Support the additional pass for compatible single-sample views, including
  perspective and orthographic cameras, independent windows, and viewports.
- MSAA and incompatible/custom framebuffer configurations use the existing
  terrain path per view. Native multisample additional-pass support is deferred.
- Preserve behavior in unsupported compositing/receiver configurations using a
  complete per-view fallback. Do not silently redefine where decals can appear.
- Keep promotion to the default as a later decision based on correctness and
  performance evidence; approval of this plan is not approval to flip it.

## 0. Measure and resolve repeated Optics misses separately

The PR identifies repeated focal-point terrain-ray misses in
`OpticsSystemNode::updateProjections`. The source confirms that only successful
intersections are cached (`intersectionCacheValid = canCache && found`). A miss
therefore permits another traversal on the next frame even without a projector
change. The depth-range cache is separate and already caches its own misses;
these two caches must not be conflated.

First instrument CPU time in `updateProjections`, rays attempted, hits/misses,
cache hits, invalidation reasons and repeated misses per projector/view in the
same Tokyo scenario. Record effective placement/depth-range validity as well as
source geometry and atlas readiness. Fully paged tiles and idle background jobs
do not establish that projection fitting is idle or successful.

Determine why the affected rays miss before choosing a fix. Inspect the separately
reported clamp fix if available and integrate it as an isolated commit after
review; otherwise implement a focused local correction. Do not assume that
suppressing retries repairs incorrect placement. Preserve existing geospatial
precision and the protection against hitting the far side of the globe.

Where misses can be cached safely, represent a completed miss separately from
an uncomputed query. Invalidate it on projector/pose or SRS changes, target
replacement, and relevant terrain loads, bounds changes and resets. Store a
conservative query footprint for misses, using broader invalidation if the
footprint is unknown. The current loaded-tile invalidation skips entries without
a valid focal point; that must be changed along with any negative-result cache.
Targets without reliable change notifications must retain a retry mechanism.
Do not fix this by permanently disabling optics or freezing failed placements.

Acceptance: stationary unchanged failed queries do not traverse terrain every
frame when caching is safe; a missing hit is retried when terrain becomes
available; motion and target changes recover correctly; effective receiving
volumes and picking remain correct. Keep regression probes transient.

Maintain attribution in the benchmark: measure the Optics change alone with the
additional pass disabled, then compare off/on rendering with the same Optics
revision and input scene. Keep untouched baseline/vendor results as historical
controls, clearly labeled. Report CPU Optics cost separately from GPU render time
and end-to-end throughput; do not estimate CPU cost by subtracting GPU time from
frame time, since CPU and GPU work overlap. If the fix changes fitted placements,
record that change and use matching placements for renderer-only comparisons.

## 1. Separate payload generation from render-path selection

Primary files: `Application.cpp`, `SlugSystem.cpp`, `SlugResource.h`, and
`ecs/slug/SlugAdapter.{h,cpp}` under `src/rocky/vsg/`.

Replace the vendor's process-global mutable enable/disable state with a startup
request in the rendering context and a resolved backend/readiness state per
view. Distinguish requested, eligible, preparing, active, and fallback states.
Shader/depth support and successful resource preparation must precede removing
an overlay from the terrain path. Missing shaders, failed compilation, unsupported
formats, or capacity exhaustion retain a functioning fallback with a concise
reason reported once when the state changes.

Keep ordinary atlas/layer metadata and picking coverage valid independently of
additional-pass segment data. A payload can be shared by an accelerated view and
a fallback view at the same time. Generate/cache compatible representations only
when needed; do not turn an atlas into a segments-only payload that fallback
cannot draw. Define the interaction with `ROCKY_SLUG_STROKES=distance` explicitly:
it continues to select the existing terrain path's stroke representation, while
the pass selects its compatible segment representation without changing other
views. Publish payload revisions and draw routing together after GPU readiness.

Acceptance: the same overlay renders in single-sample and MSAA windows
simultaneously; unsupported paths and missing resources neither drop nor double
render it. Pass-disabled mode allocates no additional-pass buffers or depth usage.

## 2. Give each view a complete, owned render sequence

Primary files: `DisplayManager.{h,cpp}`, `Application.{h,cpp}` and the new private
pass implementation extracted from `DecalSystem.cpp`.

Own depth attachment references and their recreation state by window/framebuffer,
and pass graph, viewport/scissor, bindings, camera state and draw lists by view.
Avoid sharing a mutable depth pointer across windows. Attach each view's main
pass, depth dependency, additional pass and UI in their correct order before the
next view. Keep the additional pass's private recording state distinct from the
main view's state while binding the correct main-view data.

Integrate with `Window::addView`, `Window::removeView`, window removal and
`View::dirty`, plus framebuffer recreation. Resize must refresh depth descriptors,
render area and viewport. Removal must retire the additional graph's compile
contexts before disposing view state, detach its barrier/graph and release its
resources safely. Clear all per-view records before a recycled view ID is reused.
Check standalone DisplayManager/custom-view users as well as Application users.
Custom windows whose depth was not created with the necessary support fall back.

Acceptance: two independent windows; overlapping/inset views with nonzero viewport
origins; repeated resize, close/reopen and view add/remove; mixed MSAA settings;
no stale depth, orphaned commands, cross-view rendering, or new validation errors.

## 3. Share segment geometry, not its placement

Primary files: `SlugResource.h`, `DecalSystem.{h,cpp}`, private pass resources,
`rocky.decal.record.h.glsl` and `rocky.slug.pass.vert`.

Replace the vendor's one-projector-per-segment-group mapping with immutable
geometry ranges keyed by payload identity and generation, plus per-view visible
instance records. Each instance references its own projector, transform, style,
visibility and shared segment range. Use explicit integer IDs/ranges rather than
packing growing indices into floats. Build bounded draw/indirection records whose
per-frame cost follows visible instances, not a duplicate copy of every segment
for every placement. Retain culling and the current outline/core ordering contract.

Acceptance: one payload placed multiple times in one view and across views,
including different transforms, colors, visibility and picking targets; updating
or removing one instance must not alter the other instances.

## 4. Make paging updates incremental and bounded

Primary files: `DecalSystem.cpp` and private pass buffer/residency implementation.

Use payload revision/residency changes to queue uploads, rather than adding a
whole-resident-set signature scan every frame. Allocate stable ranges in bounded
segment storage; upload changed payloads only and reuse retired ranges only after
their in-flight GPU references are safe. Separate geometry uploads from changing
per-view placement records. Existing per-view culling still runs as needed.

Batch dirty ranges, reserve/grow deliberately, validate overflow and device limits,
and avoid full compaction on ordinary tile arrival or eviction. Preserve the
branch's descriptor publication delay and keep the previous valid generation
until its replacement is ready. Exhaustion must produce a coherent fallback;
use whole-view fallback where mixing paths would change compositing order.

Add low-cost diagnostic counters for resident bytes, high-water marks, uploaded
bytes, geometry rebuilds, visible instances and fallback reasons. Detailed timing
and logging stay behind diagnostics; no verbose steady-state logging.

Acceptance: a stationary scene uploads no unchanged segment geometry; one tile
change does not rebuild unrelated resident geometry; repeated pan/zoom cycles
reach a bounded memory plateau; temporary exhaustion does not hide features.

## 5. Match the rendering and picking contract

Primary files: `rocky.slug.pass.{vert,frag}`, `rocky.slug.path.glsl`,
`rocky.decal.h.glsl`, and picking/coverage code as necessary.

Compute inverse projection once per view and reconstruct camera-relative scene
positions using the actual projection and viewport, with correct Vulkan depth
conventions and background rejection. Correct the road antialiasing footprint
for orthographic cameras and transformed projectors as well as perspective.
Preserve double-precision CPU/geospatial transforms until conversion to the
existing camera-relative GPU representation.

Use the current terrain path as the behavior reference: outlines before cores,
ordering among vector and raster overlays, opacity, highlight/style modulation,
receiver boundaries, depth occlusion, closed contours, caps/joins, polygon holes,
and picking coverage/radii. The existing renderer already groups outlines before
cores; preserve that rule rather than treating it as a new vendor defect.

A final-depth sample does not identify whether the receiver is terrain. Initially
allow the pass only where receiver identity and composition are known compatible.
Views with other depth-writing receivers, translucent terrain, or mixed decal
ordering that cannot be preserved must use the terrain path for the complete
stack. Eligibility must be explicit and refreshed when scene/configuration changes;
unknown custom scenes fall back rather than relying on an incomplete scene scan.
A general receiver mask/depth-isolation solution is a later extension, not an
implicit change to decal semantics in this integration.

Use baked/generic atlas coverage in the pass for styles not safely representable
as straight-segment core/casing draws. If a case still cannot preserve semantics,
fall back for the view. Carry the branch's per-radius picking fix forward and
check the existing picking API's documented receiver-coverage behavior separately
from visible-scene occlusion; do not change that API contract inadvertently.

Acceptance: focused image comparisons and pick probes for the cases above,
including transparent outlines, intersections, near-plane/camera-inside volumes,
steep terrain and a projected map. Intentional AA differences must be identified;
missing coverage, wrong placement/order, seams and incorrect picks are failures.

## 6. Validate, then repeat the benchmark matrix

Use `build/configure.bat` and `build/build.bat`. Keep C++17 outside the existing
private C++20 SDK adapter; document every new helper and preserve CRLF/formatting.
Review public/exported layouts and signatures and increment `PROJECT_VERSION_ABI`
if required, including class-size changes from new private members. Do not assume
that renderer-private naming alone makes a publicly installed layout ABI-neutral.

Run the existing projection/Slughorn and full suites. Separate the already
recorded SRS and Vulkan baseline failures from newly introduced failures. Use
transient regression probes for lifecycle, instancing, allocation and rendering.
Per AGENTS.md, do not commit new permanent tests without separate approval.

After correctness checks, benchmark these four modes using preserved matching
binaries and shader trees: original baseline, this branch before the pass with
its default off, untouched vendor additional pass, and integrated additional
pass. Keep the vendor-default results for reference.

For the fully paged Tokyo case, retain the exact camera, 1080p single-sample
viewport, RTX 5080, no-vsync setting, tile set and geometry hash. Freeze/cache
matching input data where possible. Require the same readiness checks and at
least 60 seconds of warmup. Run at least two interleaved 3,000-frame samples per
mode, reversing order; repeat when variance obscures a difference. Time the entire
window graph, including every pass and dependency. Report mean and per-run
p50/p95/p99 GPU/frame times, CPU update time, atlas and segment memory separately,
and upload bytes. Run no other GPU benchmark concurrently.

Add a deterministic camera pan/zoom route around Tokyo that triggers load and
eviction, then returns to the original view. Separate network wait from local
construction/upload/render cost, and report frame-time tails, time to readiness,
geometry-upload volume and memory high-water marks. Repeat several cycles to
expose unbounded growth. Check roads-only, fills-only and mixed workloads at
additional zoom levels; measure fallback behavior with MSAA and multiple views.

Performance acceptance:

- No repeatable regression in the disabled/fallback path against the equivalent
  pre-change configuration. Absence of a measured difference is not proof of zero
  cost on every device.
- Preserve the vendor pass's steady-state benefit after the correctness fixes.
  Investigate changes over 3% in mean time or 5% in p95/p99 against fresh paired
  controls; these are investigation triggers, not allowed regression budgets.
- No resident-size geometry reupload on ordinary single-tile churn, no unbounded
  memory growth, and no hidden geometry loss presented as a speedup.
- Report any remaining correctness/performance tradeoff for review rather than
  enabling the pass by default. Other GPU coverage remains a separate validation
  task if suitable hardware is unavailable locally.

## Delivery and approval boundary

Implement in reviewable commits corresponding to the stages above on the existing
branch, with normal build checks as each stage lands. Stage 0 isolates the Optics
issue, stages 1-5 establish the pass correctness baseline, and stage 6 measures
the integrated result. Include matched off/on builds with the same Optics fix so
its CPU benefit is not credited to the additional pass. Update the existing
integration review and Tokyo report with final supported/fallback cases and data.

This turn adds only this proposed design document. After the user approves this
scope, proceed through implementation, validation and repeat benchmarking without
another routine approval gate. Changing the default, committing permanent new
tests, or accepting an unresolved regression is outside this plan's approval.
