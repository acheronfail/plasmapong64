# Plasma Pong 64 — Nintendo 64

Arcade and two-player Pong inside a real-time 2D fluid simulation. Cyan and coral dye reveal
currents stirred by the bats. Those same currents accelerate and deflect the ball.
The ball leaves a subtle gold dye trail that mixes into the currents and fades.
In multiplayer, first to **9 points** wins, using N64 controllers in **ports 1
and 2**. Single player uses **port 1** against an AI opponent. On boot, a main
menu runs randomly seeded fluid currents
and mixing colours across the entire screen, with a raised cyan/coral/gold PLASMA PONG 64
block title and shadowed menu text over the fluid. The game screen omits the title;
Player labels sit at matching insets from the court edges. Use up/down to choose
**MULTI-PLAYER**, **SINGLE PLAYER**, or **HIGH SCORES**, then A or START to confirm.
Press START in the lobby to begin.

## Controls

| Control | Action |
| --- | --- |
| Analog stick / D-pad | Move your bat vertically and a short distance forwards/backwards |
| Hold Z | Fire a continuous coloured fluid jet towards the opponent |
| Hold A | Suck nearby fluid towards your bat; catch a nearby ball if it is slow enough |
| Release A | Burst fluid outwards and launch a caught ball; charge for one second and release during the brief green bar for a bonus-speed shot |
| START | Confirm menu selection, start, pause/resume, or multiplayer rematch |
| Up/down, then A on the main menu | Choose a mode or view high scores |
| B in the lobby, pause, or winner screen | Return to the main menu |

Suction reaches about 35 arena pixels; actual capture requires the ball to be
within 18 pixels on the playing side of the bat. Fast shots and strong currents
can beat a grab. You can move while holding the ball and aim a release with the
bat's vertical motion. Z and A can be used together. Unplugging a required
controller pauses the match; reconnect it and press START to resume. Arcade mode
ignores controller 2 during a run.

Fluid burst strength scales with charge. Undercharged ball launches scale up to
200 speed (198 at 99% charge). At one second of suction, the full charge bar turns
green: release during this two-tick window (about 67ms) for a 290-speed shot.
Holding until 1.067s breaks the suction mechanism with a crack and sputter.
The bat turns grey with red cracks and
a shrinking cooldown bar for five seconds of active play. Movement and Z jets
still work. A caught ball drops into the current without a launch or fluid burst.
Release A before suction can restart; pauses and level transitions freeze recovery.

## Endless arcade

Start with **3 lives**. Score **3 goals** to advance a level; conceding costs one
life without removing your goals. Clear every fifth level for an extra life,
up to five. Levels continue until you run out of lives. Transitions freeze the
playfield briefly, followed by the usual serve countdown.

Each goal earns **100 × level**, each level clear earns **500 × level**, and
clearing in under 60 seconds of active play adds up to **600 points** (10 per
remaining second). Pauses, serves and transitions do not consume that bonus time.
Holding or returning the ball gives no points. The same suction overcharge and
five-second recovery rules apply to both the player and the AI.

The AI starts with slow reactions, imprecise predictions and limited movement.
Difficulty rises with diminishing increments: reactions improve from 0.40 toward
0.08 seconds, aiming error decreases, movement approaches the human limit, and
serves accelerate from 108 toward 180 arena pixels/second. The existing ball-speed
cap remains 290. The opponent unlocks jets at level 2 and suction/catches at level
4. It generates ordinary controller inputs and obeys the same physics and catch
rules as the player; it does not run another fluid simulation. From level 6,
cross, rising and swirling currents rotate each level and strengthen with the
difficulty curve. Runs use the same initial AI random seed for repeatability.

The top ten scores include score, level and three initials. At game over, use
left/right to choose a letter and up/down to change it, then A or START to view
the table and save. Return to the menu to start another run. The ROM requests
**4K EEPROM** cartridge save storage, supported by compatible flash cartridges
and emulators. Two versioned, checksummed records alternate writes; the previous
complete record survives an interrupted update. Saving happens only after
confirming initials, outside gameplay. If storage is unavailable or verification
fails, the score screen explicitly reports session-only scores. Cartridge save
behavior still needs a SummerCart64 hardware playtest.

## Build and SummerCart64

Dependencies: Docker, `just`, and `sc64deployer` for deployment. The pinned
libdragon Docker image and library commit match `../n64-util-rom`.

```sh
just build     # produces plasmapong.z64 with RSP pressure solving
just check     # portable physics tests and SVG previews in build/
just deploy    # build the RSP ROM, then upload to /CUSTOM/plasmapong.z64
just clean
```

Power off the N64 before `just deploy` so it releases the SummerCart64 SD-card
lock. Turn it on afterwards and select `CUSTOM/plasmapong.z64` in the cartridge
menu. Deployment uploads to the SD card; it does not automatically start a game.

The first Docker build needs network access to fetch the pinned image and
libdragon revision. Subsequent builds use Docker's cache. If you already have a
compatible libdragon install, `N64_INST=/your/toolchain make -j4` also works.
`just check` needs only a C11 compiler and the system math library.

### GitHub releases

Every push to `master` runs checks, builds with the same pinned Docker toolchain,
and publishes `plasmapong.z64` as a GitHub release. The release name is the Actions
run number (1, 2, 3, …), with tag `build-<number>`. Failed runs may leave gaps.
The **ROM release** workflow can also be run manually on `master`. Reruns reuse
their original number and leave an already published release intact; an
interrupted draft is completed before publishing.

## Ares

An existing Ares checkout was found at `../ares` (the optional emulator directory
in the original request was spelled `../area`). The default recipe uses it:

```sh
just emulate
just emulate /path/to/ares
```

The recipe enables Ares Homebrew Mode. Configure the required N64 controller ports in
Ares **Settings → Input** before playing, including the analog axes, A, Z, and
START. On-screen hints use white lettering on blue A, grey Z/stick, green B, and red START
badges. Arcade requires one emulated N64 pad in port 1; multiplayer requires two.

## Implementation

- `src/fluid.c`: 48 × 33 Eulerian grid covering a 288 × 198 arena. Velocity and
  three dye concentrations use bilinear semi-Lagrangian advection. An 8-iteration
  Gauss–Seidel pressure solve uses bounded Q12 integer arithmetic to reduce
  divergence; curl confinement preserves
  small swirls. Boundary cells enforce zero wall-normal velocity. Momentum and
  dye decay gradually so old currents dissipate. Velocity and dye use aligned
  alternating buffers, with no full-grid copies between steps.
- Bat movement and Z jets inject momentum and dye into the grid. A is an
  intentional local pump/source/sink applied after pressure projection so the
  incompressibility solve does not immediately cancel the suction or burst.
  Dye advection runs after the pump update, so suction visibly draws existing
  colours inward rather than emitting new dye. Bats stir the grid through forces; they are not rasterized as solid fluid
  obstacles. Ball/bat collisions are handled independently.
- `src/arcade.c`: lightweight AI, diminishing difficulty increases, arcade goal
  rewards, lives, arena-current patterns, and ranked scores with saturating points.
- `src/save.c` and `src/save_n64.c`: portable score serialization/checksums and a
  two-slot EEPROM backend. ROM metadata requests 4K EEPROM.
- `src/game.c`: fixed 30 Hz simulation, four ball collision substeps per tick,
  velocity-field coupling, conditional catches, launch, scoring, and match state.
  Bats stay in their own end of the court. Time accumulation supports PAL and
  NTSC; catch-up is capped after long stalls.
- `src/main.c`: libdragon input and 320 × 240 RDP rendering. Dye is uploaded as a
  small RGBA32 texture and enlarged with bilinear filtering. Keeping eight bits
  per colour channel until filtering reduces gradient quantization compared with
  the former RGBA16 upload; the framebuffer remains 16-bit. Padded texture rows
  avoid RGBA32 block-upload artifacts in the pinned libdragon version, and
  filtered tile overlaps keep chunk boundaries smooth. RDP completion is
  synchronized after the next simulation step and before reusing texture memory.
  Immutable drawing commands are recorded as RSPQ blocks; rendering follows the
  30 Hz simulation instead of generating duplicate frames between updates. Game state occupies about 70 KB;
  the ROM does not require an Expansion Pak. Emulator debug output reports the
  average simulation cost every 150 active steps and draw cost every 150 frames.
  The fluid source uses `-O3` on N64, and shared timestep/radius factors are
  calculated outside its cell loops to leave more CPU time for rendering.
- `src/ui.c`: animated main menu, score, controls, bat effects, and lobby/pause/winner screens.
- `src/sound.c`: 16 kHz stereo sample mixer with separate one-shot and sustained
  voices, player panning, quiet loops, and click-resistant gain fades. Game events
  trigger bat, boundary, goal, and victory sounds once; active powers drive loops.
  Pausing, disconnecting, returning to the menu, or winning fades the loops out.

## Validation

`just check` covers two-player start gating, movement stirring, jet direction,
suction/release flow, catches for both players, rejection of fast catches,
bat/wall collisions, scoring and rematches, disconnect/pause/resume, pressure
projection, velocity decay, actual fluid-to-ball coupling, inward dye transport
from both bats, gold-trail advection/fading, and a deterministic
120-second numerical stress test. The arcade suite covers one-controller start,
AI reaction intervals, forced releases, level/extra-life rules, pause timing,
score overflow, ranked initials, score retention across runs, and stability at
levels 1, 26, 51, 76 and 101. Save tests check serialization, corrupt bytes,
incomplete records and invalid initials. It also verifies the main-menu flow, live menu
currents without controllers, sound event timing, sampled effects, stereo panning,
loop persistence, fade-out, and mix headroom. `build/sound-demo.wav` auditions the
actual runtime mix.

`build/preview-{menu,play,lobby,paused,finished}.svg` use the real game simulation and
UI drawing code with a host renderer. They show grid cells and a substitute font;
the ROM uses hardware texture filtering and libdragon's own font. They are layout
previews, not emulator captures.

The normal ROM boots to the animated main menu in the existing Ares checkout with
paraLLEl-RDP. The separate scripted-input ROM also runs gameplay and renders the
fluid correctly. Before the current optimization pass, its measured simulation cost was
about 30.1–30.3 ms per 30 Hz step in Ares, versus 31.4–31.6 ms before. Drawing
costs another 6.2 ms per rendered frame with RGBA32, comparable to 6.4 ms with
RGBA16 in the same test. These are separate costs, not a guarantee of sustained
30 FPS within the 33.3 ms frame budget. The spatial grid stays at 48 × 30 to avoid
adding simulation load. AddressSanitizer and UndefinedBehaviorSanitizer pass the
host gameplay suite (leak detection disabled in the sandbox).
The one-controller arcade smoke ROM measures about 29.7–29.9 ms per simulation
step in Ares at the initial level, plus about 6.2 ms drawing. A level-101 smoke
run measured about 29.9–30.2 ms simulation and 6.3–6.5 ms drawing. The EEPROM
fixture successfully recovered its score after an Ares restart. That is not a claim
of sustained 30 FPS on hardware. Real-console frame rate, controller feel, AI
balance, and later-level difficulty still need a SummerCart64 playtest.

For a reproducible automated gameplay run, `just smoke` builds
`plasmapong-smoke.z64` with scripted inputs for both controllers. Open that ROM
in Ares with Homebrew Mode enabled. This exercises the actual N64 rendering and
simulation and emits timing measurements to the emulator debug log. It is a
separate testing artifact: `just build` produces the human-controlled RSP ROM,
and `just deploy` builds and uploads that same ROM.
`just smoke-arcade` builds
`plasmapong-arcade-smoke.z64`, which navigates to single player and exercises the
real AI using one scripted human controller. Host previews also include
`preview-{arcade,gameover,scores,level}.svg`. `just smoke-save` builds a separate
EEPROM fixture ROM: the first boot saves an ACE score, and a second boot asserts
that it survived. This fixture never runs in the playable ROM.

### Fluid performance baseline (2026-10-02)

`just benchmark` builds `plasmapong-benchmark.z64`, a separate scripted two-player
ROM with opt-in `FLUID_PROFILE=1` timing probes. Run it in Ares with Homebrew Mode
and capture stdout, for example:

```sh
just benchmark
timeout 55s ../ares/build/rundir/bin/ares \
  --setting Developer/HomebrewMode=true --system 'Nintendo 64' \
  --no-file-prompt plasmapong-benchmark.z64 > build/fluid-profile.log 2>&1
```

Timeout exit status 124 is expected. Keep the emulator focused so it runs.
Every 150 steps ending in PLAY, the log reports average microseconds per step
for disjoint fluid stages and their call counts. A transition into PLAY can have
no fluid update; those steps remain included to match the existing simulation
counter. Non-PLAY steps are excluded. Rendering is measured separately.
Normal builds compile out all probes. Use `just smoke` and the same emulator
command with `plasmapong-smoke.z64` for the uninstrumented control.

Measured against game revision `fae1ba5`, using the pinned Docker toolchain,
fluid `-O3`, the unchanged 48 × 30 grid and eight pressure iterations, and Ares
`5f2f7dc0d` with paraLLEl-RDP. Both runs lasted 55 wall-clock seconds with audio
and rendering enabled and yielded ten complete 150-step simulation windows
(1,500 steps per ROM). These are emulated N64 timer measurements, not host CPU
benchmarks or hardware measurements.

| Fluid stage | Mean ms/step | Share of profiled simulation |
| --- | ---: | ---: |
| Dye advection, three channels | 6.949 | 22.8% |
| Pressure solve, eight passes | 6.925 | 22.7% |
| Velocity advection | 5.254 | 17.2% |
| Curl confinement forces | 2.936 | 9.6% |
| Divergence, pressure clear, initial walls | 1.876 | 6.1% |
| Pressure gradient and final walls | 1.727 | 5.7% |
| Three dye array copies | 1.654 | 5.4% |
| Curl calculation | 1.291 | 4.2% |
| Two velocity array copies | 1.092 | 3.6% |
| Splats | 0.432 | 1.4% |
| Pumps | 0.086 | 0.3% |
| Ball dye | 0.044 | 0.1% |
| Ball velocity sampling | 0.013 | <0.1% |

The uninstrumented simulation averaged **30.284 ms/step**, with window averages
between 30.138 and 30.377 ms. Drawing averaged **5.994 ms/frame** across five
150-frame windows. The profiled simulation averaged 30.537 ms, about 0.253 ms
(0.8%) higher; probes also affect compiler layout and interrupt timing, so this
difference is an estimate of measurement perturbation, not a calibrated overhead
to subtract from each stage. Stage timings include interrupts/audio work that
occurs during them. Window ranges do not measure individual-step tail latency.
Local raw logs are `build/fluid-profile.log` and
`build/fluid-profile-baseline.log`.

The optimization targets identified by this baseline were:

- **Advection (12.203 ms, 40.0%)**: inspect interpolation arithmetic, clamping,
  float/integer conversions and memory access in both full-grid sweeps.
- **Projection (10.528 ms, 34.5%)**: the solve alone visits 10,304 interior
  cells per step, with a sequential pressure dependency. Optimize that loop
  before considering fewer iterations, which would change fluid quality.
- **Copies (2.746 ms, 9.0%)**: five copies move 28,800 payload bytes per step.
  The current N64 assembly uses paired `ldl/ldr` and `sdl/sdr` unaligned
  instructions. Investigate guaranteed alignment and aligned copies, or buffer
  swapping with careful handling of the reused velocity/dye scratch arrays.
- **Curl and confinement (4.226 ms, 13.8%)**: confinement performs a square root
  and reciprocal for each of 1,144 interior cells. Any approximation needs
  numerical and visual validation.

Splats, pumps and ball sampling together take under 2% in this replay. At one
render per simulation step, simulation plus drawing costs about **36.28 ms**,
roughly **2.95 ms above** the 33.33 ms budget before other frame work. Copy removal
alone would not provide enough margin. No solver optimizations or quality changes
were made for that baseline run; the subsequent changes are described below.

### Fluid optimization results (2026-10-02)

The optimized solver keeps the 48 × 30 grid, eight lexicographic pressure passes,
three dye channels, and curl confinement. The changes are:

- Align all grid and row starts to 16 bytes. Explicit alignment assumptions on
  pressure-edge copies prevent GCC from emitting paired unaligned instructions.
  The final N64 fluid object contains no `ldl/ldr/sdl/sdr` instructions, and the
  linked game state is aligned to 16 bytes.
- Alternate between two velocity banks and two dye banks. This eliminates five
  full-grid copies (28,800 copied payload bytes per step). Bank indices keep
  ordinary `Game` assignment safe; there are no pointers into the copied object.
- Store pressure and divergence in Q12 fixed point. The bounded eight-pass solve
  uses integer additions and shifts; velocity, dye, advection, and confinement
  remain floating point. Divergence is saturated at ±32760 before conversion,
  well outside normal gameplay, to guarantee signed intermediates cannot
  overflow. The first pass exploits zero initial pressure and avoids clearing
  the whole pressure grid. Curl shares scratch storage with divergence.
- Separate boundary work from interior loops, use separable bilinear
  interpolation, and compute the shared confinement scale once per step.
- Generate the whole RGBA32 dye texture in one call, keeping bank selection and
  function-call overhead out of the pixel loop. Texture padding and filtering
  are preserved.

With the same toolchain, Ares revision, renderer, and 55-second scripted run as
above, the final uninstrumented multiplayer ROM produced ten 150-step windows
and fifteen 150-frame draw windows:

| Measurement | Original | Optimized | Reduction |
| --- | ---: | ---: | ---: |
| Simulation | 30.284 ms/step | 23.688 ms/step | 21.8% |
| Drawing | 5.994 ms/frame | 5.373 ms/frame | 10.4% |
| One simulation step + one draw | 36.278 ms | 29.062 ms | 19.9% |

The final simulation window averages span 23.551–23.763 ms. At one draw per
step, this leaves about **4.27 ms** of the 33.33 ms budget for other frame work,
where the original exceeded it by 2.95 ms. These are elapsed emulator timings;
they do not establish real-console frame rate or worst-case individual latency.

The final profiled run averages 23.809 ms/step over 1,500 steps. Pressure solving
falls from 6.925 to **4.312 ms**; both bank swaps together cost about **0.002 ms**
versus 2.746 ms for the former copies. Velocity advection falls from 5.254 to
4.817 ms, and curl plus confinement from 4.226 to 3.700 ms. Dye advection remains
about **6.901 ms** and is now the largest individual stage. Raw final logs are
`build/fluid-optimized-{profile,baseline,arcade}.log`; final assembly is in
`build/fluid-optimized-disassembly.txt`.

A separate 55-second level-101 arcade replay averages **23.601 ms/step**
(ten 150-step windows, range 23.366–23.725 ms), plus **5.674 ms/draw**
(fifteen 150-frame windows). This includes the normal replay's serves and phase
transitions, so it is not a worst-case continuously active fluid measurement.

The state grows by 5,780 bytes, from 63,756 to **69,536 bytes**. This remains
comfortably within the base N64's memory. The normal playable ROM is rebuilt
with these changes; profiling stays opt-in.

`just check` now includes a frozen scalar reference solver and differential
checks over 32 seeded random fields at velocities up to ±420, four successive
steps, zero timestep, boundary backtraces, and both buffer parities. Maximum
observed velocity difference was **0.002381 pixels/second**, with dye difference
**0.00001830**; the enforced limits are 0.005 and 0.00003 respectively. These are
short-horizon numerical checks, not a promise of identical long-term chaotic
trajectories. Additional checks cover copied-state independence, alignment,
extreme finite pressure inputs, and exact RGBA texture output/padding. The
existing 120-second gameplay stress and late-level arcade tests pass, as do
AddressSanitizer and UndefinedBehaviorSanitizer checks (leak detection disabled).

### Modern homebrew guidance applied (2026-10-02)

The most relevant primary resources for this game are:

- [Libdragon RDPQ blocks](https://libdragon.dev/ref/group__rdpq.html): record
  repeated command sequences to avoid rebuilding them on the CPU. This game now
  records the fluid upload/blit, static menu title, and court markings. The blit
  still uploads the current texture pixels on every replay; only its commands
  are reused. Changing between menu and court geometry rebuilds it after a full
  synchronization, so no queued block is freed while in use.
- [Libdragon asynchronous presentation](https://libdragon.dev/ref/rdpq__attach_8h.html):
  `rdpq_detach_show` lets the CPU continue while the RDP finishes. The texture
  reuse wait now happens after the next simulation update, before drawing begins.
  Consecutive same-colour rectangles also reuse their fill mode.
- [Libdragon cache/DMA interfaces](https://libdragon.dev/ref/group__n64sys.html):
  CPU caches require explicit coherency when sharing memory with hardware. A
  cached texture-write experiment with explicit writeback was correct but slower
  in this Ares build, so it was discarded. The existing uncached texture allocation
  is retained. Hardware measurements could justify revisiting that decision.
- [RSPQ overlays](https://libdragon.dev/ref/rspq_8h.html) and
  [RSPL](https://github.com/HailToDodongo/rspl): the route to custom vector work
  on the RSP. My recommendation for a larger follow-up is a batched fixed-point
  advection overlay, starting with dye. It needs a DMA-friendly layout, bounded
  fixed-point precision, and differential/visual tests. The pressure solver has
  a serial left-neighbour dependency; it cannot simply become eight independent
  SIMD lanes without changing the algorithm. That rendering pass did not add custom microcode; the experimental pressure
  backend below now provides the first fluid overlay.

The application also rendered many frames without a new simulation update.
There is no interpolation in the renderer, so these repeated the same state.
It now waits with audio interrupts enabled until the next 30 Hz update, then
renders once. Input, menu animation, physics, fluid resolution, and pressure
iterations retain their existing simulation rate and equations. Catch-up still
handles delayed steps. This reduces redundant work rather than lowering the
rate of distinct animation frames.

A fresh 55-second Ares comparison used the pinned toolchain and Ares
`5f2f7dc0d`, Homebrew Mode and paraLLEl-RDP, with normal audio enabled. The control
uses revision `fae145f`'s original renderer/pacing plus matching frame counters;
the extracted control main is `build/perf-control-main.c`. Both runs produced ten
150-step simulation windows. The control produced sixteen 150-frame windows;
the final build produced ten.

| Measurement | Control | Final |
| --- | ---: | ---: |
| Simulation, elapsed ms/step | 23.679 | 23.626 |
| Main-thread drawing, elapsed ms/frame | 5.240 | 4.763 |
| Frame submission interval, ms | 21.806 | 33.354 |
| Frames submitted per emulated second | 45.86 | 29.98 |
| Steps / frames in complete frame windows | 1,569 / 2,400 | 1,500 / 1,500 |
| Main-thread drawing time per emulated second, ms | 240.3 | 142.8 |

Drawing takes **9.1% less time per submitted frame**, and avoiding duplicate
frames reduces measured drawing time per emulated second by **40.6%**. This
recovers about 98 ms of main-thread time each second. Simulation speed is
essentially unchanged; this is not a 40.6% overall game speedup. The old submission
rate counted repeated states, while the new rate follows the 30 Hz simulation.
Drawing measurements include synchronization and audio interrupts; the new wait
is deferred, so they are not isolated GPU execution timings or pure CPU usage.
These are window averages in an emulator, not worst-case or real-console results.
Raw logs: `build/perf-{control,final}.log`. The normal `plasmapong.z64` has been
rebuilt. Real-console frame pacing and controller feel still need a hardware test.

A separate 55-second main-menu comparison reduced drawing from **8.175 to
4.661 ms/frame (43.0%)**. Menu fluid seeds are randomized, so this is not a
pixel-identical replay; the static title drawing is unchanged. The final menu
also reports one simulation step per submitted frame at approximately 30 Hz.
Logs: `build/perf-menu-{control,final}.log`.

Validation: all host suites pass, and all nine generated SVG previews are
byte-identical to previews made with the original UI source. Scripted N64
multiplayer renders correctly in Ares. A 55-second level-101 arcade run
averaged 23.562 ms/step and 4.996 ms/draw, with 1,500 steps for 1,500 frames
in complete windows (`build/perf-late.log`). Its mean submission interval was
33.563 ms, including phase/save transitions. A separate 25-second smoke run
with `rdpq_debug_start()` enabled reported no RDP validation errors
(`build/perf-validate.log`). Menu and gameplay screenshots were visually checked.
The numerical solver is unchanged.

The regular log now includes frame interval and simulation-step/frame counts.
`just benchmark` additionally reports audio callback cost; that time is already
included in elapsed simulation/draw measurements and must not be added again.
The exploratory gameplay run spent about 4–5% of emulated time in audio, making
fluid advection the stronger candidate for substantial RSP acceleration.

### RSP pressure backend

The rendering/pacing checkpoint is commit `fae187b`. The next optimization starts
with the existing Q12 pressure solve, because it can move to the RSP without
changing numerical precision or the eight-pass lexicographic algorithm.

```sh
just build           # playable plasmapong.z64 with RSP pressure solving
just benchmark-rsp   # boot-time differential checks, then profiled scripted play
just benchmark       # default RSP backend, profiled scripted play
just cpu             # CPU reference backend, playable plasmapong-cpu.z64
just benchmark-cpu   # CPU reference backend, profiled scripted play
```

`just build`, `just emulate`, and `just deploy` use the RSP pressure solver.
Deployment uploads `plasmapong.z64` to `/CUSTOM/plasmapong.z64`, without scripted
inputs or boot-time test fixtures. `just rsp` remains available to build the same
backend under the separate name `plasmapong-rsp.z64`.
`FLUID_RSP=1` is the Makefile default; set `FLUID_RSP=0` for the CPU reference.
Default objects live in `build/rsp` or `build/cpu`; use a separate build directory
when changing backend or instrumentation flags. The pinned libdragon RSP build
rule cannot generate symbols correctly for directories containing hyphens; the
provided recipes use `build/rsp` and `build/rsp_benchmark`.

`src/rsp_fluid.S` implements one pressure pass per RSPQ command, allowing
high-priority RSPQ work between passes. Three rotating pressure rows plus one divergence row
use **768 bytes of DMEM scratch**. The normal row is unrolled in IMEM to avoid
per-cell address/loop instructions; the linked RSP text occupies **2,832 bytes**.
All DMA is row-aligned and completes before its scratch row is consumed or reused.
Each pass reloads its inputs, so it does not rely on scratch surviving overlay
switches. This is a scalar integer RSP kernel, not a SIMD rewrite of Gauss–Seidel.

`src/fluid_rsp.c` handles overlay registration, cache writeback/invalidation,
eight queued passes, and a completion syncpoint before CPU gradient subtraction.
The pressure output owns complete cache lines and is invalidated before DMA to
prevent stale CPU writebacks. Divergence is read-only. The normal grid/state size
is unchanged; velocity, curl, confinement, pressure-gradient subtraction and dye
advection still run on the CPU.

`tests/rsp_fluid_smoke.h` checks all 1,440 pressure cells for each of 64 fields:
zero, maximum positive/negative divergence, alternating signs, impulses and seeded
random data at ordinary and extreme magnitudes. Every output must match the CPU
solver bit-for-bit. Dirty output is overwritten from a zero initial solve, input
integrity and DMA guard regions are checked through RDRAM, and RDP commands force
overlay switches between calls. `RSP_TEST=1` enables these boot-time checks only;
they do not run in the playable ROM. The portable reference/gameplay,
arcade, save and audio suites also pass after extracting the CPU solver.

With the same pinned toolchain and Ares `5f2f7dc0d` configuration as above,
55-second uninstrumented scripted runs produced ten 150-step/frame windows each:

| Measurement | CPU backend | RSP pressure backend |
| --- | ---: | ---: |
| Simulation average, ms/step | 23.681 | 22.053 |
| Simulation window range, ms/step | 23.541–23.760 | 21.921–22.125 |
| Drawing average, ms/frame | 4.757 | 4.719 |
| Frame submission interval, ms | 33.353 | 33.353 |

That is **1.628 ms less simulation time per step (6.9%)**. The separate 64-field
boot test measures approximately **4.10 ms CPU versus 2.83 ms RSP per solve**,
including the RSP API's cache maintenance and completion wait. These synthetic
fields and cache states differ from gameplay, so the kernel measurement is not
interchangeable with the full-frame measurements. Both backends produced 1,500
updates and 1,500 submitted frames in complete windows. Logs are
`build/rsp-{cpu-gameplay,pressure-gameplay}.log`.

Separate profiled gameplay runs measure the pressure stage at **4.401 ms CPU
versus 2.792 ms RSP (36.6% reduction)**, including cache maintenance, queueing and
waiting. The CPU run yielded nine complete 150-step windows and the RSP run ten;
logs are `build/rsp-cpu-profile-final.log` and `build/rsp-pressure-unrolled.log`.
These instrumented measurements are separate from the table above.

The host suites pass. The RSP boot checks also pass with libdragon's RDP validator
enabled, and the 25-second combined graphics/fluid run reports no RDP validation
errors (`build/rsp-validate.log`). Gameplay was visually inspected in Ares. A separate 55-second level-101 RSP
arcade run completed without errors (`build/rsp-pressure-late.log`).
These results are emulator timings, not a real-console speed guarantee. Following
a successful user-reported hardware playtest, the RSP pressure backend is now the
default. No grid reduction or pressure iteration reduction is involved; hardware
performance has not yet been quantified.

This establishes the queue, DMA and numerical-test infrastructure for later RSP
components. Dye/velocity advection still uses floating point; a future RSP port
needs an explicitly validated fixed-point/vector design. The next pass below
improves the existing CPU advection kernels first.

### CPU advection optimization (2026-10-02)

`src/fluid_advection.c` contains separate velocity and dye kernels. Explicitly
non-overlapping input/output banks and a separate compilation unit let the pinned
MIPS compiler use simpler sample addresses and schedule independent channels
together. Float loop coordinates avoid per-cell integer-to-float conversions.
A finite-binary32 coordinate clamp uses one unsigned comparison on the common
path instead of two serial FPU comparisons. The grid, interpolation arithmetic,
decay factors, pressure solver and simulation frequency are unchanged.

The comparison pins gameplay/UI/audio to `fae1d58` so concurrent gameplay changes
cannot affect the results. Both versions use the **48 × 33** grid, RSP pressure,
the pinned libdragon toolchain and Ares `5f2f7dc0d` with paraLLEl-RDP and audio.
Each profiled run lasted 55 seconds and produced ten 150-step windows:

| Profiled stage | Before, ms/step | After, ms/step | Reduction |
| --- | ---: | ---: | ---: |
| Velocity advection | 5.289 | 4.843 | 8.4% |
| Dye advection | 7.616 | 7.051 | 7.4% |
| Combined advection | 12.905 | 11.893 | 7.8% |

Separate 55-second runs with profiling disabled averaged **24.348 → 23.403 ms
per simulation step**, saving **0.946 ms (3.9%)**. Each run again produced ten
150-step windows. Drawing averaged 4.472 → 4.481 ms/frame; the submission interval
remained 33.353 → 33.352 ms, providing more headroom within the existing 30 Hz
schedule. All profiled stage call counts match between the two versions.

These are emulator timings; the new optimization has not been timed on hardware.
Earlier tables used the smaller 48 × 30 grid and are not a direct comparison.
Controlled logs and ROMs are in `build/advection_compare/`, including
`control-profile.log`, `final-profile.log`, `control.log` and `final.log`.

`just check` includes a 64-field exact float-reference regression for both kernels:
zero/signed-zero velocity, constant/ramp/checkerboard/random dye, strong flows,
boundary-crossing backtraces, zero dt and three nonzero time steps. Every output
is checked and the source banks must remain unchanged. The existing full-fluid
reference, gameplay, arcade, save and audio tests also pass. Address/undefined
sanitizers pass with leak detection disabled (the local runner uses ptrace).

`just benchmark-advection` builds a separate scripted ROM that runs these same
64 cases on the N64 before reporting stage timings. The target checks pass in
Ares, as do the existing RSP pressure checks in a combined verification ROM.
Normal `just build` / `just deploy` use the optimized kernels without test probes.
For benchmarks, add `--setting Input/Defocus=Allow` to the Ares command so losing
window focus does not pause the run.

## Sound assets

The effects use CC0 stock recordings from Kenney, trimmed, filtered, looped,
and resampled for a compact N64-era feel. Suction and jet peaks are roughly
16–18 dB below the bat hit so they can be held without dominating the match.
Suction has a one-time attack and 1.2-second transition into a soft sustained hum;
menu confirmation and back actions also have distinct cues. These are modern
library recordings, not samples extracted from GoldenEye 007.
See [audio sources and licenses](assets/audio/README.md) for the exact recordings,
pack links, included licenses, processing, and audition order.

The checked-in sample bank means `just build` needs no downloads or audio tools.
To regenerate it from the included originals, use `python3 tools/prepare-audio.py`
with ffmpeg installed. This also writes individual WAV auditions in `assets/audio/`.
