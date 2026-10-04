# Real-console performance findings — 2026-10-04

## The final revelation

The apparent simulation slowdown was largely **simulation waiting behind
rendering on the shared RSP command queue**. On the real N64, RDP backpressure
blocked the RSP while it tried to submit another rendering command buffer.
Fluid simulation jobs queued behind that work inherited the delay. Ares did
not reproduce this sustained hardware wait, so its simulated 60 FPS concealed
the scheduling bottleneck.

The passive trace identified `RSPQCmd_RdpSetBuffer` waiting for the RDP's
`END_VALID` bit to clear: 92,869 of 100,990 queue-wait samples (91.959%) had
that wait marker, all with caller offset `0x194`. None of the nonzero markers
included the routine's command-busy or DMA-busy wait bits. This locates the
observed submission stall; it does not imply that RDP drawing itself is free
or that reducing buffer switches alone would remove the delay.

The first high-priority experiment exposed a second scheduling mistake:
it reopened the high-priority queue after each completed simulation batch
and kept it open during CPU-only work. An empty open high-priority batch
prevented ordinary rendering commands from progressing. That moved waiting
into drawing rather than improving total frame time.

**The successful fix opens high priority only immediately before submitting
RSP simulation jobs, then closes and synchronizes the batch at the existing
output wait. It leaves the queue closed during CPU work.** Chained simulation
jobs stay together, while rendering can progress between batches. Texture
production remains on the ordinary queue, with its existing source-ownership
guard. Native priority switching still occurs only at command boundaries.

The menu's measured simulation time fell from **15.941 to 10.467 ms/step**,
removing roughly **5.47 ms** of inherited scheduling delay. Grid size,
pressure iterations, physics, RGBA32 fluid generation, bilinear filtering,
particle appearance and resolution were retained.

## Real hardware acceptance results

Test setup: this connected NTSC N64, 8 MiB RDRAM, SummerCart64 firmware
2.20.2, USB logging, pinned libdragon `e356bf3f56f7afbf7e5246329562f145965cfdfc`.
Both comparisons use the same deterministic workload and framebuffer-bank
placement; the menu comparison also retains its exact source and foreground
caches. Each acceptance run captured ten windows of 150 rendered frames.

| Workload | Previous control | Yielding scheduler | Presentation and audio |
| --- | ---: | ---: | --- |
| 640 × 480 particle menu | 47.195 FPS | **59.941 FPS** | One startup repeat, none thereafter; zero underruns or audio debt |
| Four-player 640 × 480 tails/stress gameplay | 51.174 FPS | **59.942 FPS** | 1,490 new PLAY frames / 1,490 VI; zero repeats, underruns or audio debt |

Menu draw submission averaged 0.782 ms/frame. Gameplay simulation averaged
10.000 ms/step across nine reported simulation timing windows, and draw
submission averaged 2.287 ms/frame. Each render window contained 150 simulation
steps per 150 frames. The interlaced high-resolution target is one new
framebuffer per NTSC field, about 60 fields/second, rather than 60 complete
two-field scans/second.

Local raw evidence and JSON summaries are under `build/hardware/` (ignored
build artifacts): `menu-recovery-01`, `menu-rdp-trace-01`, `menu-yield-01`,
`gameplay-candidate-01`, and `gameplay-yield-01`. The README retains the full
experiment history, including unsuccessful candidates and the profiler panic.

## Shipping settings and validation

The complete RSP backend now defaults to `FLUID_HIGHPRI=1` and
`FLUID_HIGHPRI_YIELD=1`. Unsupported comparison backends disable high priority
automatically; explicit queue-wait profiling uses ordinary scheduling.
The validated framebuffer placement, exact fixed-backend menu source cache
and menu foreground cache also ship by default. Historical hardware recipes
specify their old settings explicitly, so their controls remain reproducible.

`just build` produces the human-controlled `plasmapong.z64`. The benchmark
ROMs remain separate; `just benchmark-hardware-yield` and
`just benchmark-hardware-yield-gameplay` reproduce the accepted workloads.

Validation completed:

- Host gameplay, numerical, menu-cache and audio checks: `./tools/check.sh`.
- Ares RDP validation for high-resolution four-player stress gameplay with
  both 8 MiB and 4 MiB RAM.
- Ares RDP validation of low/high/low/high resolution changes through Options
  with the promoted defaults on both memory configurations.
- Playable and CPU comparison ROM builds.
- The two real-hardware acceptance captures above.

The observed result establishes the target cadence in these captured
workloads; it is not an exhaustive claim about every gameplay state or PAL
timing. Rejected frame recording/template, streaming, RGBA16 ink, VI point
sampling and library tail-sync experiments remain disabled.
