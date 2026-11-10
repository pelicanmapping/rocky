# Slughorn PR #269 integration review

Reviewed 2026-11-10 on `codex/slughorn-integration` against Rocky
`1f8995523b5d847e3f292a1a1c5c83e91c9f3508`.
Vendor PR: <https://github.com/pelicanmapping/rocky/pull/269>, head
`24f59cd628ee8c25577c91b8e6836459729fda20` (cubicool).

## Decision

Integrate the pinned SDK/API update and a guarded centerline-stroke experiment.
Keep baked strokes and the original terrain coverage math as the default.
Do not merge the proposed separate render pass or fill batching yet. This is a
selective integration, not a merge of the complete vendor commit.

Included:

- SDK pin `1e09656e5fe2da7a24c164fe8697187ebf28f673`, verified archive checksum,
  and an updated vcpkg port version so dependency caches rebuild it.
- Adaptation to `Point`, `canvas::Stroke`, and the SDK's stroke-aware band counts.
  Explicit `Stroke::Method::Outline` preserves the existing default.
- Startup-only `ROCKY_SLUG_STROKES=distance`, selecting both shader and producer
  from the same context setting. The default shader has no distance-stroke branch.
- The vendor's stable root solver only in the experimental shader variant.
- A straight-segment distance shortcut: SDK `lineTo` encodes `{p1,p3,p3}`, which
  otherwise reaches the vendor's general cubic/trigonometric distance solver.
- Correct per-layer CPU picking radius. The vendor cached one decoded sampler by
  shape key, so both core and casing used the outer radius even when the GPU
  rendered different widths. The integration decodes the two radii separately.
- Baked fallback for translucent outlined strokes. Replacing an outline ring with
  a full casing changes alpha compositing underneath a translucent core.
- Vendor demo with an offline grid, argument validation, nonblocking timestamp
  reads, timestamp queue/rollover checks, a collection timeout, and readiness/
  geometry reporting. Paging callbacks own their inputs through asynchronous shutdown.
  No persistent test modifications.

Public structure layouts and exported function signatures do not change, so
`PROJECT_VERSION_ABI` remains 63. Only the private C++20 adapter input changes;
Rocky's public and demo code remain C++17.

## Why the complete pass is deferred

These findings refer to the vendor head, not the integrated branch:

1. **Window/view ownership:** `SlugPassNode` has one `window` and `_depthView`,
   overwritten by `setSlugPassWindow` for every new view. All view-local descriptor
   sets consequently sample the last window's depth. `onAddView` also appends a
   sibling graph/barrier without integrating their removal into view lifecycle.
   Use window-owned pass/depth resources and explicit add/remove/resize handling.
2. **Custom MSAA windows:** the warning in `Application::onAddView` explicitly says
   overlays will not draw. The terrain layer range has already been emptied.
   A working fallback must be decided per view/window before creating dependent
   resources, including mixed-sample windows; a mutable process-global switch
   does not provide that lifecycle contract.
3. **Shared payloads:** `groupDecals[group]` holds a single projector index. Multiple
   projected instances referencing one road atlas overwrite that index; only the
   last placement survives. Separate immutable segment storage from per-instance
   draw records.
4. **Paging cost:** `updateDecalsSSBO` hashes the resident set every update and
   reconstructs/uploads the entire resident segment buffer whenever it changes.
   A parked-camera benchmark does not establish pan/zoom scalability. Use stable
   suballocations, incremental residency updates, explicit buffer limits, and
   measurements of upload bytes and frame-time tails during tile churn.
5. **Rendering contract:** the pass performs global outlines-then-cores draws,
   reconstructs perspective depth only, and directly blends layers against the
   loaded color attachment. Projection modes, overlapping overlay order,
   highlights, translucent receivers, depth-dependent visibility and picking
   parity all need targeted checks before enabling this as a general decal pass.
6. **Fill batching:** splitting connected/overlapping polygon components can
   introduce seams and double-blend translucent overlaps, as the vendor notes.
   Keep capacity-based partitioning with existing hole/overlap guarantees.

Unrelated CURL/ImGui/comment fixes were left out of this focused integration.
The vendor's test changes forced baked mode in four tests; keeping baked mode as
our default makes those environment-mutating test changes unnecessary.

## Validation and measured limits

Windows/MSVC RelWithDebInfo, NVIDIA GeForce RTX 5080, 1920x1080, single sample.
Same camera, deterministic 25-cell grid, 7,200 straight road segments and 3,600
buildings, no network imagery/elevation. The old binary and shader tree were
preserved separately before replacing the SDK. GPU timestamps bracket the window
command graph. Measurements are specific to this workload and hardware.

An initial paired comparison measured the unconditionally enabled stable solver
at 3.295 ms mean GPU time versus 3.042 ms baseline (about 8% slower). Running the
new SDK against the original shader restored roughly 3.070 ms. The shader
precision change was therefore removed from the default path.

Three interleaved 1,000-frame runs after warmup, with experimental shader math
compiled out of the default path:

| Mode | GPU mean range (ms) | GPU median range (ms) | GPU p95 range (ms) |
| --- | --- | --- | --- |
| Baseline | 3.243-3.252 | 3.003-3.015 | 4.290-4.307 |
| Updated default | 3.266-3.277 | 3.044-3.047 | 4.310-4.320 |
| Opt-in distance | 0.884-0.886 | 0.883 | 0.900-0.902 |

A final run with source/readiness accounting confirmed identical geometry and
50 ready atlases in all three modes: baseline 3.012 ms, default 3.047 ms, distance
0.888 ms mean GPU time. The default retained 3,686,400 atlas bytes; distance used
1,024,000 (72% less). A token comparison with the experiment disabled confirmed
both default GLSL evaluators are unchanged from baseline.

The updated default measured about 1% slower; this is not proof of zero
regression. The opt-in variant reduced GPU time by about 73% in this grid, but
that is not a general frame-rate or real-world map claim. It remains opt-in.
Visual inspection confirmed buildings and roads in both variants. This scene
does not cover moving-camera paging, multiple windows, real city datasets,
steep terrain, transparent layers, or a range of GPUs.

- Normal configure/build/install succeeded using the repository scripts; vcpkg
  downloaded and checked the pinned archive.
- All 50 `[projection]` tests passed, with 311,096 assertions.
- Full suite: 118/119 pass both before and after, with identical 16 failures in
  the existing SRS vertical-datum test on this environment. `PROJ_DATA` was set to
  the installed PROJ share directory; the vertical-datum failures were not repaired as part of this update.
- Transient adapter checks passed for core/casing hit widths, round caps,
  transparent-outline fallback, and dense row-spanning distance atlases.
  A 200-segment sample used 30,720 atlas bytes versus 93,696 baked (67% less).
- `--msaa 4` with distance strokes rendered all 50 atlases. A validation run
  reported `VUID-VkDeviceCreateInfo-enabledLayerCount-12384` at device creation
  and `VUID-vkDestroyInstance-instance-00629` for a leaked debug messenger at
  shutdown. Both reproduce with the preserved baseline binary, so Vulkan
  validation is not clean on this environment; these are not claimed as passes.
- The public MVT endpoint initially returned HTTP 403 during the synthetic
  comparison. A subsequent real-demo comparison successfully loaded Tokyo:
  [fully paged Tokyo benchmark](slughorn-pr269-tokyo-benchmark.md). The exact
  vendor default reduced mean GPU time by 8.5% across two paired runs, with
  identical source geometry. This is a separate comparison from the selective
  integration variants measured above.

Transient probe sources and raw logs live under the out-of-source build folder,
not in the repository. No new permanent regression tests are committed.

## Before promotion

Obtain reproducible local MVT datasets; compare roads-only, fills-only and mixed
scenes at several zoom levels and with elevation. Include sparse and very dense
roads, closed contours, alpha/outline combinations, picking boundaries, MSAA,
and devices with different shader throughput. Measure atlas construction time,
peak memory, upload bytes, p50/p95/p99 frame times and tile churn. Promote the
stroke default only after those checks meet the project's regression threshold.
The separate pass needs the ownership/residency fixes above before that matrix.
