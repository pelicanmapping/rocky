# Slughorn PR #269: fully paged Tokyo MVT comparison

Measured 2026-11-10 on `codex/slughorn-integration`.

## Default-path result

The vendor PR's default path improved steady-state Tokyo performance in both
paired comparisons. Mean GPU time fell 6.1% in the first pair and 10.9% in the
reverse-order pair. Across 6,000 measured frames per version, GPU time fell
8.5%, frame throughput increased 9.3%, and atlas texture payload fell 9.4%.
No performance regression was observed in this stationary, fully paged scenario.

This measures the unmodified vendor renderer, not our selective integration's
baked default or its optimized opt-in distance implementation. It supersedes the
earlier inability to fetch the live MVT dataset; it does not replace the separate
synthetic comparison in [the integration review](slughorn-pr269-integration.md).

## Versions and setup

- Baseline: Rocky `1f8995523b5d847e3f292a1a1c5c83e91c9f3508`, using the engine DLL
  and shader tree preserved before upgrading Slughorn.
- Vendor: [PR #269](https://github.com/pelicanmapping/rocky/pull/269), exact head
  `24f59cd628ee8c25577c91b8e6836459729fda20`, built in an isolated worktree using
  `build/configure.bat` and `build/build.bat`.
- All `ROCKY_SLUG_STROKES`, `ROCKY_SLUG_PASS`, and `ROCKY_SLUG_FILL_BATCH` overrides
  were unset. The vendor therefore used its default distance strokes, terrain
  rendering path, and unbatched fills. Its separate pass was not enabled.
- Windows, MSVC RelWithDebInfo, NVIDIA GeForce RTX 5080, driver 610.60.
- Actual `rocky_demo`, GIS > Vector tiles (MVT Features), Tokyo button viewpoint:
  longitude 139.743, latitude 35.684, range 13.5 km.
- 1920 x 1080 render extent, single sample, `--novsync`, continuous rendering.
- Unchanged demo styles, z14 paging and screen-space error 256. Black 7 m roads
  with white 0.5 outline, yellow buildings, ReadyMap imagery dataset 7 and
  elevation dataset 116, with the demo's elevation clamping.
- Live MVT source: `https://readymap.org/readymap/mvt/osm/{z}/{x}/{y}.pbf`.

## Paging and measurement checks

Identical transient hooks in the demo selected Tokyo, opened only the relevant
GUI section, tracked tile callbacks, and collected Vulkan GPU timestamps. No
renderer or feature-selection code was changed for measurement. The baseline
instrumented demo executable used the preserved baseline engine and shaders;
the vendor demo used its own engine and shaders.

Each process warmed up for at least 60 seconds. Measurement required at least
20 consecutive seconds with unchanged geometry, completed tile count, atlas
readiness, and draw count; no active tile callbacks, failed loads, or pending
paging jobs. The default job pool's permanent delete-queue waiter was excluded
from the idle check. In practice each run was stable for 51-53 seconds before
measurement started.

All four runs loaded exactly the same scene:

| Check | Every run |
| --- | ---: |
| Successful MVT tiles | 48 |
| Failed MVT loads | 0 |
| Tile coordinates | z14, x14548-14555, y6448-6453 |
| Line points | 56,678 |
| Polygon parts | 519,166 |
| Ready overlays / published draws | 96 / 96 |
| Atlas layers | 234 |
| Source-coordinate hash | 5239511238660498999 |

The source hash covers the clamped double-precision geometry coordinates,
aggregated independently of load order. Scene counts, geometry hash, tile
completion count, and idle state were rechecked after measurement. Both builds
were also visually inspected in the real Tokyo scene, with no obvious missing
features; this was not an exhaustive image-difference validation.

GPU timestamps bracketed the window command graph, using a nonblocking eight-slot
query ring. After scene inspection, the ring was flushed before collecting 3,000
samples per run. No registry scans occurred inside the timed loop. Frame time is
wall time spent in `app.frame()`, including update, rendering submission and
waiting; it is not an isolated CPU-work measurement. GPU and frame samples are
steady-state distributions, not same-frame latency pairs.

## Measurements

Run order was baseline 1, vendor 1, vendor 2, baseline 2. Only one benchmark ran
at a time. Values below are milliseconds; smaller is better.

| Run | GPU mean | GPU p50 | GPU p95 | GPU p99 | Frame mean | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline 1 | 23.0728 | 22.7213 | 24.6425 | 25.7738 | 23.6094 | 25.1929 | 26.2502 |
| Vendor 1 | 21.6542 | 21.3626 | 23.3551 | 23.8436 | 22.1047 | 23.7459 | 24.3302 |
| Vendor 2 | 20.5816 | 20.2806 | 22.1368 | 22.8140 | 20.8896 | 22.5490 | 23.2468 |
| Baseline 2 | 23.0877 | 22.7557 | 24.6076 | 25.8274 | 23.3943 | 25.0562 | 26.2011 |

Equal sample counts allow averaging the run means. Percentiles above remain
per-run values; they are not pooled percentiles.

| Aggregate metric | Baseline | Vendor default | Change |
| --- | ---: | ---: | ---: |
| Mean GPU time | 23.0803 ms | 21.1179 ms | 8.5% lower |
| Mean frame time | 23.5019 ms | 21.4972 ms | 8.5% lower |
| Throughput, 1000 / mean frame time | 42.55 fps | 46.52 fps | 9.3% higher |
| Atlas texture payload | 80.73 MiB | 73.11 MiB | 9.4% lower |

Atlas payload is the sum of curve/band texture data sizes, not total GPU memory
or driver allocation size. Both runs of each version had identical atlas bytes:
84,649,984 baseline and 76,662,784 vendor.

## Interpretation and scope

The real Tokyo workload benefits, but the improvement is much smaller than the
synthetic road-grid result. Both mean and tail timings improved in both pairs.
The vendor run-to-run variation means 8.5% is an observed average, not a precise
universal speedup. These results cover a stationary camera on this GPU after
paging completes. They do not establish loading speed, moving-camera behavior,
other zoom levels, hardware, or projection modes. The optional separate pass is
measured separately in the follow-up below.

This follow-up does not change the integration branch's runtime defaults. The
other correctness findings and promotion checks in the integration review still
apply; a positive parked-camera result alone does not resolve them.

## Local reproduction artifacts

The transient harness and logs remain outside the repository under
`D:/devel/rocky/build/mvt-comparison/`:

- `MvtBenchmark.h` and `instrumentation.patch` contain the measurement hooks.
- `baseline-1.log`, `vendor-1.log`, `vendor-2.log`, `baseline-2.log` contain raw
  readiness, tile-set and timing output.
- `restore-build.log` records rebuilding the normal demo after restoring its
  original sources. No measurement hooks or regression tests were committed.

The baseline executable and shaders are under
`D:/devel/rocky/build/slughorn-baseline/`; the vendor worktree is
`D:/devel/rocky/pr269-benchmark/` and its installation is
`D:/devel/rocky/install-pr269-benchmark/`.

To rerun the instrumented binaries, set `ROCKY_MVT_BENCH=1`, optionally set
`ROCKY_MVT_LABEL`, unset the three Slughorn overrides above, and launch with
`--novsync`. Point `ROCKY_FILE_PATH` at the matching baseline or vendor shader
parent directory, set `PROJ_DATA` to the installed PROJ share directory, and
include `D:/devel/rocky/install/bin` on `PATH` for runtime dependencies.

Engine DLL SHA-256:

- Baseline: `57c413a017ff75ce60d658bd48a604b71b179c68852da014517023f09c30a6f3`
- Vendor: `24893f4c71992f3b523ff28c5cef43f6879331a9309eed550ec18e4db0817edb`


## Additional-pass follow-up

At the user's request, the same vendor binary was benchmarked with
`ROCKY_SLUG_PASS=1`. `ROCKY_SLUG_STROKES` and `ROCKY_SLUG_FILL_BATCH` remained
unset. No renderer changes or integration fixes were applied. The camera, data,
1920 x 1080 extent, single sampling, no-vsync setting, warmup/readiness checks,
and 3,000 measured frames per run were unchanged.

The GPU timestamp range surrounds the entire window command graph, including
both render passes and the barrier between them. Thus the timing includes the
additional pass's cost. Visual inspection during warmup showed the Tokyo roads
and buildings present. All registered overlays were ready; the source geometry
hash and exact tile set matched the original comparison.

The additional pass stores a layer for each road segment's core and casing,
rather than using the default's atlas-wide road layers. Its recorded resource
layer count is therefore 56,816 instead of 234. The 96 published decal records
count projector records, not Vulkan draw calls. Atlas texture payload is still
76,662,784 bytes (73.11 MiB); this excludes the additional pass's segment buffers
and depth resources and is not a total-memory comparison.

Two 3,000-frame runs per mode were collected in the order additional pass 1,
fresh default 3, fresh default 4, additional pass 2. The fresh default controls
used the same vendor executable with only `ROCKY_SLUG_PASS` unset. All four
runs passed the unchanged readiness and post-measurement checks.

| Run | GPU mean | GPU p50 | GPU p95 | GPU p99 | Frame mean | Frame p95 | Frame p99 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Additional pass 1 | 9.5343 | 9.8927 | 10.0588 | 10.1543 | 10.1117 | 10.7445 | 10.9249 |
| Fresh default 3 | 20.5692 | 20.2630 | 22.1295 | 22.9417 | 20.8856 | 22.5512 | 23.4355 |
| Fresh default 4 | 20.5632 | 20.2480 | 22.1483 | 22.9295 | 21.0244 | 22.5648 | 23.5462 |
| Additional pass 2 | 9.5370 | 9.9049 | 10.0514 | 10.1396 | 10.0684 | 10.9094 | 11.7442 |

All timings are milliseconds. Per-run percentiles are not pooled.

| Aggregate metric | Earlier baseline | Fresh PR default | Additional pass |
| --- | ---: | ---: | ---: |
| Mean GPU time | 23.0803 ms | 20.5662 ms | 9.5357 ms |
| Mean frame time | 23.5019 ms | 20.9550 ms | 10.0901 ms |
| Throughput, 1000 / mean frame time | 42.55 fps | 47.72 fps | 99.11 fps |

Compared with its fresh default controls, the additional pass reduced mean GPU
time by 53.6% and mean frame time by 51.8%, yielding 2.08 times the throughput.
Compared with the earlier baseline, GPU time was 58.7% lower and throughput was
2.33 times higher. Baseline was not rerun in this follow-up. The fresh default
controls are close to the earlier faster default run; the original default-path
measurements above are retained rather than mixed with these new controls.

Both additional-pass runs improved the p95 and p99 GPU and frame timings as well.
The measured benefit is substantial for this fully paged, stationary Tokyo view.
This benchmark does not evaluate paging churn or resolve the separate integration
issues; no runtime defaults were changed as part of this follow-up.

Raw follow-up logs are `pass-1.log`, `vendor-3.log`, `vendor-4.log`, and
`pass-2.log` under `D:/devel/rocky/build/mvt-comparison/`.
`comparison-results.json` collects all eight completed runs. Reproduce the
additional-pass runs with the same instructions above, setting
`ROCKY_SLUG_PASS=1` while leaving the other two overrides unset.


## Optics cost caveat

The PR reports repeated terrain-placement focal-ray misses in
`OpticsSystemNode::updateProjections`. Baseline, vendor head and the integration
branch used the same Optics source for these measurements. It caches successful
intersections but leaves misses uncached, permitting repeated CPU terrain
traversals on subsequent frames.

The benchmark did not instrument ray counts or isolate Optics CPU time, so it
does not establish how often this happened in Tokyo or its measured cost. Stable
MVT/atlas counts and idle paging jobs do not prove that these queries stopped or
that every effective receiving volume was successfully fitted. Matching source
geometry hashes do not independently verify effective fitted projector transforms.

The frame-throughput results include whatever Optics work occurred. GPU timestamps
measure the submitted window graph, not these CPU raycasts; the repeated-miss
issue does not by itself explain away the measured GPU-time reduction. Fixing
Optics may change throughput and, if placement changes, the rendering workload.
The [integration plan](slughorn-additional-pass-plan.md) now calls for explicit
Optics instrumentation and matched off/on comparisons using the same Optics
revision, keeping that benefit separate from the pass's benefit.


## Completed opt-in integration: matched Optics controls (2026-11-10)

These measurements supersede the prototype timings for the integrated path.
The default remains unchanged; enable the additional pass explicitly with
`ROCKY_SLUG_PASS=1`. Baked fallback/picking atlases remain available, and no
optional vendor fill batching is enabled.

All stationary runs use the same fully paged MVT Features Tokyo view, 1920x1080,
single sample, no vsync, RTX 5080, 60-second minimum warmup, 20 seconds stable
with idle paging, and 3,000 measured frames. GPU timestamps cover the entire
window command graph. No build, second benchmark, or other agent-controlled
rendering work ran during a timed sample.

Every run retained 48 tiles, 56,678 line points, 519,166 polygon parts, 96 ready
overlays and 96 published placement records. Source hash:
`5239511238660498999`. Integrated and vendor-with-Optics runs also report the
corrected placement hash `11964528720301840533`. The older control harness did
not record that hash; it uses the identical Optics implementation and camera.
All 96 projections fitted successfully; 23 terrain-edge hits were recovered
during preparation and zero rays were cast during the measured interval.

The control is the preserved pre-pass branch with the Optics fix. The vendor
control is PR #269 with that same Optics fix applied; it is **not the untouched
vendor build** used in the historical sections above. This separates the pass
from the Optics correction and matches the actual receiving-volume placements.

| Run | GPU mean | GPU p50 | GPU p95 | GPU p99 | Frame mean | Frame p95 | Frame p99 | Update mean |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| final-control-1 | 20.1333 | 20.1233 | 20.3709 | 20.5063 | 21.5641 | 22.5545 | 43.3349 | — |
| final-off-1 | 20.1347 | 20.1251 | 20.3625 | 20.4928 | 21.5069 | 22.2582 | 22.6623 | 0.5221 |
| final-vendor-1 | 8.8039 | 8.8007 | 8.8945 | 8.9452 | 10.0062 | 11.0929 | 11.6814 | 1.2708 |
| final-vendor-2 | 8.8086 | 8.8052 | 8.9008 | 8.9439 | 10.0147 | 10.9173 | 11.4209 | 1.1461 |
| final-off-2 | 20.1246 | 20.1171 | 20.3476 | 20.4962 | 21.8698 | 43.4153 | 62.3476 | 0.5304 |
| final-control-2 | 20.1043 | 20.0953 | 20.3344 | 20.4401 | 21.6430 | 43.3091 | 62.7532 | — |
| final-on-3 | 14.2159 | 13.7210 | 16.1345 | 16.4506 | 16.0627 | 16.9795 | 17.6098 | 0.3818 |
| final-on-4 | 14.8187 | 14.3174 | 16.8116 | 17.1856 | 16.3078 | 17.5324 | 18.6988 | 0.3670 |
| final-fallback-2 | 21.2045 | 21.1878 | 21.4966 | 21.8227 | 21.5857 | 22.3347 | 41.1472 | 0.4388 |

All times are milliseconds; percentiles remain per-run, not pooled. The older
control harness did not record total CPU update time; it did record Optics time.

The disabled integration averaged 20.1296 ms GPU versus 20.1188 ms for the control (+0.05%). There is no repeatable GPU regression evident in these pairs. Both the control
and disabled runs show intermittent wall-clock frame stalls; their similar
tails should not be interpreted as a new renderer cost or a universal latency
guarantee.

The final additional pass averaged 14.5173 ms GPU (27.8% less than the matched control), and 16.1852 ms per frame (61.78 fps versus 46.29 fps). Its GPU and frame tails also remain below the control in this scene.

The vendor pass averaged 8.8063 ms GPU. The integration deliberately preserves relative overlay order and global
outline/core order. Grouping all fills ahead of roads recovered prototype speed
in a diagnostic (8.136 ms), but can change overlapping/translucent composition.
A shader-based correction was too expensive; conservative non-overlap sorting
produced no useful gain. Neither experiment ships. The remaining gap against
the vendor prototype is explicit; it does not erase the gain against baseline
and is not grounds to change the default.

A requested-but-ineligible view initially measured 21.2836 ms GPU
(`final-fallback-1.log`). Its extra terrain shader shortcut was removed; the
ordinary terrain shader is now unchanged. `final-on-1/2` used that earlier
revision and are not included in the final active-path aggregate. The final
active runs (`final-on-3/4`) and final fallback use the unmodified terrain
shader. The final C++ cleanup also removes the now-unused shader define/header
flag; it does not change shader code or the measured drawing path.

Final forced fallback measured 21.2045 ms GPU, with all 96 placements and no pass geometry allocation.

Removing the shortcut did not eliminate the requested-but-ineligible overhead: the final GPU result is 5.4% above the matched pre-pass control. Its mean wall-clock frame time remains comparable. The attached extra graph and sampleable-depth configuration remain present in this state; their individual costs were not isolated. This is an explicit opt-in fallback limitation, not a claim of zero regression for every requested configuration. The default-off path does not attach those resources.

### Storage, updates and paging

The ordinary atlas payload is 84,647,936 bytes (80.73 MiB), retained for fallback
and picking. Additional geometry is 4,545,280 bytes (4.33 MiB), shared across
placements and views. All 96 payloads upload during preparation; every stationary
active sample records **zero geometry upload bytes** during measurement. The
vendor atlas is smaller at 76,662,784 bytes (73.11 MiB), because it does not
retain the same baked fallback representation. These figures exclude CPU copies,
depth/framebuffers, metadata and other GPU allocations; they are not total VRAM.

A deterministic four-stop route pans east, pans north/zooms out to 18 km,
returns west/zooms in to 8.5 km, then returns to the exact 13.5 km Tokyo view.
Three cycles ran per mode with capacity 256 in both modes, to avoid the default
128-atlas cap at the wider stop. This capacity change is confined to the route;
the stationary comparison remains at 128. Each stop waits at least ten seconds
and five stable/idle seconds.

| Mode/cycle | GPU mean | GPU p95 | GPU p99 | Frame mean | Frame p95 | Frame p99 | Update mean | Geometry uploaded (MiB) | Resident at return (MiB) | Peak (MiB) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| on/0 | 12.374 | 17.799 | 18.392 | 16.455 | 18.677 | 114.822 | 4.195 | 5.080 | 4.335 | 7.068 |
| on/1 | 12.685 | 17.836 | 18.367 | 17.310 | 18.777 | 165.518 | 4.870 | 5.131 | 4.335 | 7.068 |
| on/2 | 12.446 | 17.585 | 18.130 | 16.337 | 18.596 | 108.648 | 4.256 | 5.080 | 4.335 | 7.068 |
| off/0 | 17.672 | 20.204 | 20.353 | 22.236 | 21.844 | 119.874 | 4.483 | 0.000 | 0.000 | 0.000 |
| off/1 | 17.675 | 20.185 | 20.373 | 22.091 | 21.911 | 147.941 | 4.467 | 0.000 | 0.000 | 0.000 |
| off/2 | 17.712 | 20.209 | 20.380 | 22.211 | 22.003 | 147.093 | 4.724 | 0.000 | 0.000 | 0.000 |

All 12 settled stop hashes and draw counts match between modes: **True**.
Pass residency returns to 4.33 MiB on each Tokyo return and peaks at 7.07 MiB,
with no cycle-over-cycle growth. Uploads follow changed/reloaded payloads rather
than rebuilding the entire resident geometry buffer. No failed stops or
cancelled payload jobs occurred. Paging frame tails include source processing,
resource compilation and uploads. First-idle timings include network/cache wait
and are recorded in raw logs; this probe does not separately measure network
and local construction time, so it is not an isolated network benchmark.

### Validation, limits and reproducibility

See the [implementation plan's validation record](slughorn-additional-pass-plan.md)
for the 156 checks at capacities 128/256, the final 158-check run including
reflected projectors, image comparisons and startup failure/MSAA fallback.
A projection-switch defect found by image review was fixed by keeping frustum
grid projection interpretation synchronized with its inverse matrix. Native
window resize could not be reliably verified through the desktop helper; inset
resize and independent-window lifecycle were exercised. Other GPUs/platforms,
projected maps and steep-terrain/near-plane image coverage remain unverified.

Transient instrumentation and tests are not committed. Raw logs, the harness,
`run-final-matrix.ps1`, `run-route-final.ps1`, and `final-results.json` are under
`D:/devel/rocky/build/mvt-comparison/`. The instrumented final executable/library
and matching shader tree are preserved under
`D:/devel/rocky/build/slughorn-integrated-final/`. Use the normal installed
dependencies on PATH and the vcpkg PROJ data directory. Reapply the transient
harness to `rocky_demo` to reproduce the original demo scenario; the standalone
Slughorn demo is a different workload.

Final clean builds succeeded with Slughorn enabled and disabled, after removing
all transient instrumentation and tests. Both full test runs reported only the
same pre-existing vertical-datum test failure (16 assertions) seen in the
baseline. No test-source changes are included. Build and test logs are
`final-clean-build2.log`, `final-off-build2.log`, `final-clean-tests.log` and
`final-off-tests.log` in the raw-results directory above.

## Merge retest (2026-11-17)

The integration was combined with main's camera fix (`f814414b`) in `4d9e44c7`.
The combined ABI is 65. The additional pass remains opt-in; the ordinary terrain
renderer remains the default.

The enabled build, clean production rebuild and Slughorn-disabled build passed.
Both full test suites had
exactly the same 16 vertical-datum assertion failures as the previous baseline,
with no additional failing assertions. Offline synthetic rendering smoke tests
passed for baked/default strokes, the active additional pass, and distance
strokes. Each completed with 14,400 road points, 3,600 polygon parts and 50 ready
atlases. These short synthetic runs are smoke tests, not replacements for the
fully paged Tokyo performance comparison or the earlier 158 integration checks.

The Tokyo rerun was blocked before timing: the preserved matched-control binary
could not load any of its 48 MVT tiles. The readymap.org tile endpoint now returns
a Cloudflare Access sign-in redirect and HTML instead of MVT data. The harness
correctly reported `NOT_SETTLED` and emitted no timing result. Consequently the
2026-11-10 timings above remain the latest valid Tokyo measurements, and the
Tokyo-dependent integration checks could not be repeated on the merged code.
No access controls were bypassed and no alternate dataset was substituted.

Raw evidence is under `D:/devel/rocky/build/mvt-comparison/`, with filenames
starting `retest-20261117-`. All temporary demo instrumentation was restored out
of the source tree before the clean production rebuild.
