# Slughorn PR #269: fully paged Tokyo MVT comparison

Measured 2026-11-10 on `codex/slughorn-integration`.

## Result

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
other zoom levels, hardware, projection modes, or the optional separate pass.

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
