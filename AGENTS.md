# Development workflow

Read `docs/development-loop.md` before performance work. Use `just dev-loop NAME`
with a unique name to retain source/build provenance, Ares validation and console
measurements. The connected console is NTSC-J, with SummerCart64 on
`serial:///dev/ttyUSB0`. Read the remote ESPHome power URL from `N64_POWER_URL`
in the environment or ignored `.env` file. Never commit the local outlet address.

Use `tools/n64_power.py` for explicit, verified on/off control. For hardware
captures, use the upload/listen/power sequencing in `tools/benchmark-hardware.py`.
Console uploads and remote power control are part of this project's development
workflow requested by the user. Do not change cartridge firmware or outlet
configuration as part of routine testing.

Use test-ROM input replays or Ares's own controls. Do not inject desktop inputs
through X11/KDE. The development loop does not use the legacy X11 screenshot path.

Optimize for fast, bounded currents that visibly transport dye and predictably
affect the ball. Approximate solvers may change numerical results; preserve
gameplay expectations rather than requiring bit-exact agreement with the old
algorithm. Keep fractional velocity and smooth sampling unless measurements and
playback support an alternative. Seek headroom for features and a finer fluid grid.

Hardware timing decides performance; Ares checks correctness and helps diagnose
regressions. Compare matched workloads and separate validation/profile captures
from ordinary timing captures. Report simulation time, presentation misses and
audio behavior. Draw submission time alone does not measure completed RDP work.
