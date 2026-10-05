# Plasma Pong 64 — Nintendo 64

Arcade and two- to four-player Pong inside a real-time 2D fluid simulation. Cyan and coral dye reveal
currents stirred by the bats. Those same currents accelerate and deflect the ball.
The ball leaves a subtle gold dye trail that mixes into the currents and fades.
In two-player matches, first to **9 points** wins. Select **2P**, **3P**, or **4P**
with left/right on the multiplayer menu row. Multiplayer is greyed out with fewer
than two controllers, and larger matches are available only when enough pads are
connected. Players are assigned connected N64 controllers in port order.

Choose **OPTIONS → RESOLUTION** to switch between **LOW RES** (320 × 240,
60 FPS, default) and **HIGH RES** (640 × 480 interlaced, 60 updates per second).
Up/down selects a row; left/right changes its value immediately. Each change saves
automatically to cartridge EEPROM alongside flow effects and high scores. Older
save records retain their scores/effect and default to low res. Existing records
with the former 30 FPS setting select high res; 60 FPS selects low res.
Both resolutions use the same 60 Hz physics, emission and timer settings.
On NTSC, high res targets a new framebuffer each field: roughly 60 fields,
or 30 complete interlaced scans, per second.

Real-console benchmarks on SummerCart64 now sustain **59.94 FPS** in the
high-resolution particle menu and scripted four-player high-resolution tails
stress gameplay. The default RSP scheduler runs simulation jobs at high
priority and yields between batches, allowing rendering to progress during
CPU work. The gameplay capture had no repeated fields or audio underruns over
ten windows; the menu had one startup repeat and none thereafter. Detailed
captures and controls are recorded in the hardware benchmark notes below.
See [the final console bottleneck and fix](docs/hardware-performance.md) for
the concise findings and acceptance results.

A console performance overlay is enabled for testing: **L** on controller port 1
shows/hides it; **R** resets its counters. `FPS` shows newly presented frames per
second followed by measured video refresh rate. `MISS` counts repeated refreshes
beyond one refresh per frame. `GAP` is the longest interval in refreshes
(1 is ideal). Counters restart on scene or resolution changes. After entering gameplay,
press R and exercise four-player jets, suction, and all flow effects for several
minutes. NTSC is roughly 60 Hz; PAL's native video mode remains roughly 50 Hz,
with physics running at 60 Hz. The overlay updates its cached
text twice a second; hiding it removes its drawing cost while sampling continues.

Three- and four-player matches use a square court: P1 (cyan) on the left, P2
(coral) on the right, P3 (mint) at the bottom, and P4 (violet) at the top. The top
is a wall in 3P. Diagonal corner walls and inset movement limits keep adjacent
paddles apart. Each player starts with **three lives**; missing your side loses
one. Eliminated players' sides become walls, and the **last player standing** wins.
A rematch restores every player's lives and requires all selected pads.
Single player uses **port 1** against an AI opponent. On boot, a main
menu runs randomly seeded fluid currents
and mixing colours across the entire screen, with a raised cyan/coral/gold PLASMA PONG 64
block title and shadowed menu text over the fluid. The game screen omits the title;
Two-player labels sit at matching insets from the court edges; 3P/4P uses the
colour-matched life counters above the court. The active menu row uses bright
text and continuously emits gold dye. Dedicated triangles surround the player
count, with unavailable directions dimmed. Use up/down to choose
**MULTI-PLAYER**, **SINGLE PLAYER**, **HIGH SCORES**, or **OPTIONS**, then A, Z, or START to confirm.
Press START in the lobby to begin.

## Controls

| Control | Action |
| --- | --- |
| Analog stick / D-pad | Move along your side and a short distance forwards/backwards |
| Hold Z | Fire a continuous coloured fluid jet into the court |
| Hold A | Suck nearby fluid towards your bat; catch a nearby ball if it is slow enough |
| Release A | Launch a caught ball with an outward fluid burst; hold the ball for one second and release during the brief green bar for a bonus-speed shot |
| START | Confirm menu selection, start, pause/resume, or multiplayer rematch |
| Left/right on MULTI-PLAYER | Select an available player count |
| Up/down, then A, Z, or START on the main menu | Choose a mode, view high scores, or open options |
| B in the lobby, pause, or winner screen | Return to the main menu |

Stick movement ramps smoothly from zero outside a 12% dead zone to full speed
at full tilt. Paddles move up to 140 arena pixels per second along their side
and 210 forwards/backwards; the D-pad moves at full speed. Moving towards the
centre as you hit the ball adds power to the rebound, up to the 290 ball-speed cap.

Suction reaches about 35 arena pixels; actual capture requires the ball to be
within 18 pixels on the playing side of the bat. Fast shots and strong currents
can beat a grab. You can move while holding the ball and aim a release with the
bat's motion along its side. Z and A can be used together. Unplugging a required
controller pauses the match; reconnect it and press START to resume. Arcade mode
ignores the other controllers during a run. Eliminated players can disconnect
without pausing the remaining players.

The charge bar appears and starts filling only while a ball is caught. Suction
without a ball does not charge or overheat, and releasing A without a ball causes
no burst. Fluid burst strength scales with charge. Undercharged ball launches scale up to
200 speed (198 at 99% charge). After holding the ball for one second, the full charge bar turns
green: release during this two-tick window (about 67ms) for a 290-speed shot.
Above 240 speed, the ball and its motion streak turn red and it deposits red dye
instead of gold. Perfect shots cross this threshold; normal colouring returns
as the ball slows, while the red dye already deposited keeps moving with the fluid.
Holding the ball until 1.067s breaks the suction mechanism with a crack and sputter.
The bat turns grey with red cracks and
a shrinking cooldown bar for five seconds of active play. Movement and Z jets
still work. A caught ball drops into the current without a launch or fluid burst.
Release A before suction can restart; pauses and level transitions freeze recovery.

## Endless arcade

Start with **3 lives**. Score **3 goals** to advance a level; conceding costs one
life without removing your goals. Clear every fifth level for an extra life,
up to five. Levels continue until you run out of lives. Transitions freeze the
playfield briefly, followed by the usual serve countdown.

The player's paddle shrinks by one arena pixel of total height per level,
from 28 pixels at level 1 to a minimum of 16 at level 13. Its drawing, collision
height, movement limits and suction capture radius track the smaller size.
The opponent's paddle and all multiplayer paddles retain their usual size.

Each goal earns **100 × level**, each level clear earns **500 × level**, and
clearing in under 60 seconds of active play adds up to **600 points** (10 per
remaining second). Pauses, serves and transitions do not consume that bonus time.
Holding or returning the ball gives no points. The same suction overcharge and
five-second recovery rules apply to both the player and the AI.

The AI starts with slow reactions, imprecise predictions and limited movement.
Difficulty rises with diminishing increments: reactions improve from 0.40 toward
0.08 seconds, aiming error decreases, and movement approaches the human limit.
Every serve starts at rest in the centre; after the countdown, fluid currents
and players' jets set the ball in motion. There is no automatic low-speed boost.
The ball's response to currents also increases each level: 1× at level 1,
1.5× at level 5, 2× at level 13, approaching 3× at high levels. This makes
both player-generated and ambient currents accelerate and deflect it more
strongly, while still-water drag stays the same. The ball-speed cap remains 290.
The opponent unlocks jets at level 2 and suction/catches at level
4. Its jet force increases with difficulty: 1.25× the player's at level 5,
1.5× at level 13, approaching 2× at high levels. Player and multiplayer jets
retain their usual strength. It generates ordinary controller inputs and obeys
the same physics and catch rules as the player; it does not run another fluid
simulation. From level 6,
cross, rising and swirling currents rotate each level and strengthen with the
difficulty curve. Runs use the same initial AI random seed for repeatability.

The top ten scores include score, level and three initials. At game over, use
left/right to choose a letter and up/down to change it, then A or START to view
the table and save. Return to the menu to start another run. The ROM requests
**4K EEPROM** cartridge save storage, supported by compatible flash cartridges
and emulators. Two versioned, checksummed records alternate writes; the previous
complete record survives an interrupted update. Saving happens after confirming initials or changing a flow option, outside gameplay. If storage is unavailable or verification
fails, the score screen explicitly reports session-only scores. Cartridge save
behavior still needs a SummerCart64 hardware playtest.

## Build and SummerCart64

Dependencies: Docker, `just`, and `sc64deployer` for deployment. The pinned
libdragon Docker image and library commit match `../n64-util-rom`.

```sh
just build     # produces plasmapong.z64 with RSP pressure and dye advection
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
  dye decay gradually so old currents dissipate. Velocity damping is 0.08 per
  second, with swirl confinement at 1.25: sustained jets build currents that
  linger after release. The ball responds to flow with a 1.7 coupling strength
  while retaining its 0.297 still-water drag and 290 speed cap. Velocity and dye use aligned
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
- `src/game.c`: fixed 60 Hz simulation, four ball collision substeps per tick,
  velocity-field coupling, conditional catches, launch, scoring, and match state.
  Bats stay in their own end of the court. Time accumulation supports PAL and
  NTSC; catch-up is capped after long stalls.
- `src/main.c`: libdragon input and selectable 320 × 240 / 640 × 480 RDP rendering.
  High res doubles drawing coordinates, bitmap font pixels and particle dots,
  while retaining the same fluid grid and gameplay coordinates. Video switches
  drain queued drawing, discard cached commands and replace the framebuffers.
  Presentation counting removes interlaced field offsets before detecting swaps.
  Dye is uploaded as a
  small RGBA32 texture and enlarged with bilinear filtering. Keeping eight bits
  per colour channel until filtering reduces gradient quantization compared with
  the former RGBA16 upload; the framebuffer remains 16-bit. Padded texture rows
  avoid RGBA32 block-upload artifacts in the pinned libdragon version, and
  filtered tile overlaps keep chunk boundaries smooth. RDP completion is
  synchronized after the next simulation step and before reusing texture memory.
  Immutable drawing commands are recorded as RSPQ blocks; rendering follows the
  selected simulation frequency instead of generating duplicate frames between
  updates. Game state occupies about 57 KiB;
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

`just check` covers 2P/3P/4P controller gating, square courts, elimination walls,
lives, rematches, rotated paddle powers, four-player audio, and two-player start gating, movement stirring, jet direction,
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
just build           # playable plasmapong.z64 with RSP pressure and dye advection
just benchmark-rsp   # boot-time differential checks, then profiled scripted play
just benchmark       # default RSP backend, profiled scripted play
just cpu             # CPU reference backend, playable plasmapong-cpu.z64
just benchmark-cpu   # CPU reference backend, profiled scripted play
```

`just build`, `just emulate`, and `just deploy` use RSP pressure solving, vector dye/velocity advection, and integer curl/confinement.
Deployment uploads `plasmapong.z64` to `/CUSTOM/plasmapong.z64`, without scripted
inputs or boot-time test fixtures. `just rsp` remains available to build the same
backend under the separate name `plasmapong-rsp.z64`.
`FLUID_RSP=1 DYE_RSP=1 VELOCITY_RSP=1 CONFINEMENT_RSP=1` are the Makefile defaults; use `just cpu` (all four flags
set to zero) for the CPU reference, or `just float-dye` for float dye with RSP
pressure. `just float-velocity` keeps RSP pressure/dye with float velocity.
Default objects live in `build/rsp_confinement`; use a separate build directory
when changing backend or instrumentation flags. The pinned libdragon RSP build
rule cannot generate symbols correctly for directories containing hyphens; the
provided recipes use underscore-separated directories.

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
was unchanged at this checkpoint. Pressure-gradient subtraction runs on the CPU;
dye/velocity advection and curl/confinement use the RSP backends described below.

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
components. At this checkpoint, dye/velocity advection still used floating point.
The next pass below improves the CPU kernels before the vector dye implementation.

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
The float comparison backend retains both optimized kernels. Normal `just build` /
`just deploy` use vector RSP velocity and dye advection without test probes.
For benchmarks, add `--setting Input/Defocus=Allow` to the Ares command so losing
window focus does not pause the run.

### Default vector RSP dye advection (2026-10-02)

```sh
just build            # playable plasmapong.z64 with RSP pressure and dye
just deploy           # upload as /CUSTOM/plasmapong.z64; power off N64 first
just float-dye        # comparison ROM with float dye and RSP pressure
just benchmark-dye    # integer/float comparison and DMA checks, then profiled play
```

Following a successful user-reported hardware playtest, `just build`, `just emulate`
and `just deploy` use this implementation by default. `DYE_RSP=1` automatically
enables `DYE_FIXED=1`. Production objects live in `build/rsp_confinement`; float comparison
objects remain in `build/rsp`, preventing reuse across storage formats.
`just dye` and `just deploy-dye` remain compatibility recipes for the same backend
under the separate name `plasmapong-dye.z64`.
`DYE_FIXED=1 DYE_RSP=0 VELOCITY_RSP=0 CONFINEMENT_RSP=0` runs the same integer dye model on the CPU for comparison.

Dye now persists in Q13 halfwords in both ping-pong banks. Injection is quantized
at its source, and texture colors use integer arithmetic directly. At this checkpoint,
velocity, pressure and ball physics retained their existing representations and algorithms.
`FluidInk` selects storage at build time; callers reading or writing dye in
physical units use `fluid_ink_decode` / `fluid_ink_encode`. Game copies also copy
the temporal-rounding phase, preserving deterministic independent simulations.

`src/rsp_dye.S` handles one channel per RSPQ command. It loads that channel's
3,168-byte source grid, gathers eight arbitrary bilinear samples at a time, then
uses vector interpolation and decay. Q15 fractions share one CPU-generated
trace grid across all three channels. DMA moves 24 traces/results per chunk.
The linked overlay uses **1,136 bytes of IMEM** and **3,920 bytes of DMEM**
(560 bytes of data plus 3,360 bytes of scratch). Each command reloads its scratch,
and all DMA completes before returning to RSPQ. The CPU wrapper writes back
inputs, invalidates the destination, and waits before swapping banks.

Interpolation rounds to nearest. Decay uses a deterministic changing Q16
rounding threshold, avoiding both a permanent faint residue and the bias of
always rounding down. This changes dye numerically: it is checked against a
specified error budget, while RSP output must match the integer oracle exactly.

The initial float-to-fixed-to-float adapter took about 9.5 ms per dye step and
was slower than the CPU. Persistent storage removes those full-grid conversions.
With gameplay pinned to `fae144d`, 48 × 33 cells, eight RSP pressure passes, the
pinned libdragon toolchain and Ares `5f2f7dc0d` / paraLLEl-RDP with audio:

| Measurement | Float dye | RSP fixed dye | Reduction |
| --- | ---: | ---: | ---: |
| Profiled dye stage, including traces/DMA/wait | 7.052 ms | 4.879 ms | 30.8% |
| Uninstrumented simulation | 23.405 ms | 21.291 ms | 9.0% |
| Uninstrumented drawing | 4.534 ms | 2.332 ms | 48.6% |
| Uninstrumented simulation + drawing | 27.938 ms | 23.623 ms | 15.4% |

Each of the four runs lasted 55 seconds and produced ten 150-step/frame windows.
Profiled call counts match. Submission intervals stay near 33.35 ms: this saves
about **4.32 ms of measured work** within the existing 30 Hz schedule. These are
emulator results, not measured hardware gains. Logs and ROMs are retained under
`build/dye_compare/`; `results.json` records means, ranges and window counts.

A separate 35-second control runs the portable scalar integer reference with the
same fixed-point storage and integer colors. Across its six complete windows,
dye takes **8.202 ms on CPU versus 4.869 ms on RSP** for the matching first six
windows (40.6% reduction). Thus the RSP improves the advection work itself; the
drawing improvement comes from integer color conversion. This is a comparison
with the reference kernel, not a claim about every possible CPU implementation.

Validation includes:

- 64 seeded/boundary/zero/maximum/faint fields: RSP output matches the integer
  reference exactly, with source/trace integrity, DMA guards and overlay switches
  checked. Largest one-step float error is **0.00027514**, at most **1 RGB level**.
- 3,600 host steps of injection then fading: maximum dye error **0.00489778** and
  **2 RGB levels**; all dye eventually reaches zero. Limits are 0.006 and 3 levels.
- Both storage formats pass fluid/reference, gameplay and arcade tests, including
  suction transport, gold trails, bank copies and 120-second stability. Their
  recorded ball/score/velocity physics traces match exactly (`b9c7dafe`).
- Address/undefined sanitizers pass for the dye model and fixed-storage gameplay
  with leak detection disabled for the local ptrace-based runner.
- A deterministic host gameplay preview differs by at most **1 RGB level** from
  float dye (mean absolute difference 0.040 levels across RGB pixels).
- Combined dye/pressure fixtures and a 30-second level-101 arcade run pass with
  the RDP command validator enabled (`build/dye-validate.log`).

`RDP_VALIDATE=1` enables libdragon's RDP command validator in a separate build;
use a distinct `BUILD_DIR` when changing test or profiling flags.

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

### Default Q4 velocity and RSP advection (2026-10-02)

```sh
just build                # playable plasmapong.z64 with RSP velocity, dye and pressure
just deploy               # upload as /CUSTOM/plasmapong.z64; power off N64 first
just float-velocity       # previous implementation: float velocity, RSP dye/pressure
just benchmark-velocity   # velocity/dye/pressure checks, then profiled gameplay
```

Following a successful user-reported hardware playtest, `just build`, `just emulate`
and `just deploy` use Q4 velocity and RSP advection by default. `VELOCITY_RSP=1`
automatically enables `VELOCITY_FIXED=1`. Production objects use `build/rsp_confinement`;
the previous float-velocity comparison uses `build/rsp_dye`. `just velocity` and
`just deploy-velocity` remain compatibility recipes for the same default backend
under the separate name `plasmapong-velocity.z64`. `VELOCITY_FIXED=1 VELOCITY_RSP=0 CONFINEMENT_RSP=0` builds the scalar CPU reference with the
same representation; it still uses RSP dye by default. Instrumentation flags
need their own build directory. Fixed velocity requires fixed dye, and RSP
velocity requires the RSP dye overlay.

Both velocity banks now store signed Q4 samples by default: **1/16 pixel/second**
precision, saturated to **±1023.9375 pixels/second**. This deliberately restricts
samples to ±16383 so their differences fit a signed vector lane. `FluidFlow`
selects storage; use `fluid_flow_encode` / `fluid_flow_decode` when accessing
physical units. Ball position/velocity and gameplay calculations remain floats.

`src/fluid_velocity_fixed.c` prepares integer backtraces using Q12 grid
coordinates and a shared Q20 timestep coefficient. Coordinates keep the existing
clamped-edge sampling policy, with a slightly quantized upper boundary. The same
trace preparation also accelerates dye. Divergence and pressure-gradient
subtraction use integer arithmetic. At this checkpoint curl/confinement still
used floats; the integer RSP implementation below is now the default. The Q12 pressure solve and its eight
iterations are unchanged.

The existing eight-lane `rsp_dye.S` interpolation/decay kernel processes the two
signed velocity channels through `fluid_channels_rsp`. No additional microcode
is needed. DMA/cache ownership and completion waits are retained, and all waits
are included in timings. A changing per-fluid rounding threshold prevents
nearest-rounded decay from leaving stationary low-speed residue. Copying a
`Fluid` also copies that phase. The initial conversion-wrapper prototype cost
about 6.32 ms per advection step and was rejected; persistent storage avoids
packing/unpacking the velocity grids each frame.

Against a frozen `fae143f` baseline, using the pinned toolchain, 48 × 33 grid,
Ares `5f2f7dc0d`, paraLLEl-RDP, audio and identical scripted controller inputs:

| Measurement | Float velocity | Q4 + RSP velocity | Reduction |
| --- | ---: | ---: | ---: |
| Profiled velocity advection, including traces/DMA/wait | 4.850 ms | 2.952 ms | 39.1% |
| Profiled dye advection | 4.879 ms | 3.460 ms | 29.1% |
| Profiled divergence | 1.633 ms | 0.708 ms | 56.7% |
| Uninstrumented simulation | 21.290 ms | 16.968 ms | 20.3% |
| Uninstrumented drawing | 2.330 ms | 2.310 ms | 0.9% |
| Uninstrumented simulation + drawing | 23.620 ms | 19.278 ms | 18.4% |

Each of these four runs lasted 55 seconds and produced ten complete 150-step/frame
windows. The submission interval remains about 33.35 ms: the saving is **4.34 ms
of work per update**, not a change to the 30 Hz schedule. These are emulator
measurements. Hardware playtesting succeeded; hardware timing has not been measured.
Confinement gets slower (2.866 → 3.356 ms) because updates now require rounding;
the larger savings elsewhere outweigh this cost. As trajectories diverge, some
ball-dependent work differs despite identical controller inputs.

A separate 35-second/six-window control runs the same Q4 algorithm on the CPU.
Velocity advection averages **5.264 ms CPU versus 2.953 ms RSP** in the matching
first six RSP windows, a **43.9%** reduction. This is the portable scalar oracle,
not a claim that no faster CPU implementation is possible. Logs, ROMs and timing
summaries are in `build/velocity_compare/`, including `results.json`.

Validation and accuracy:

- `just check` includes 64 signed velocity fields with zero/unity decay,
  alternating signs, random fields, edge-crossing traces and saturation. The RSP
  matches the integer oracle exactly on N64 emulation, including source/trace
  integrity, DMA guards and overlay switches.
- Maximum one-step error against float advection is **0.4144 pixels/second** at
  normal timesteps and **1.156 pixels/second** including 0.2-second stress steps.
  These bounds cover unsaturated inputs within ±420; deliberately saturated
  million-unit inputs are checked for bounded output, not float equivalence.
- Gameplay and arcade tests pass in both representations, including the
  120-second stability replay. Quantized release injections allow one velocity
  LSB of error; gameplay rules and exact release-speed tests remain intact.
  Address/undefined sanitizers pass with leak detection disabled for the ptrace
  runner. The default float backend's 120-second comparison CSV matches the
  frozen baseline exactly on the host.
- A separate identical-forcing flow run injects jets for 60 seconds and fades for
  60 seconds. Average RMS speed changes by **0.31%**, maximum sampled velocity is
  428.19 pixels/second, and no sampled cells saturate. Faint residual currents
  fade faster: final RMS is 0.0035 versus 0.4058 pixels/second.
- Ball paths and scores can diverge. The gameplay replay's average RMS flow speed
  differs by 0.27%, but this is not a bound on local errors or trajectory drift.
  Combined velocity/dye/pressure fixtures and level-101 arcade gameplay pass with
  the RDP validator enabled.

`build/velocity_compare/preview.html` is a standalone comparison with selectable
6-, 18-, 60- and 120-second snapshots of gameplay and identical fluid forcing.
These are deterministic **host-model previews**, not emulator screenshots; the
integer kernel is separately verified against the RSP. `tools/velocity-replay.c`
emits reproducible per-frame CSV statistics and optional field snapshots when
compiled for either backend. `tools/preview.c` accepts an optional second argument
for the number of gameplay steps, preserving its default 540-step snapshot.

### Default integer curl and RSP confinement (2026-10-02)

```sh
just build                   # playable plasmapong.z64
just deploy                  # upload as /CUSTOM/plasmapong.z64; power off N64 first
just float-confinement       # previous float curl/confinement for comparison
just benchmark-confinement   # all four RSP fixture suites, then profiled gameplay
```

Following a successful user-reported hardware playtest, integer curl and RSP
confinement are the default for `just build`, `just emulate`, and `just deploy`.
`CONFINEMENT_RSP=1` automatically enables `CONFINEMENT_FIXED=1` and uses
`build/rsp_confinement` objects to produce `plasmapong.z64`.
`just float-confinement` disables this backend and builds the previous version
as `plasmapong-float-confinement.z64` in `build/rsp_velocity`.
`just confinement` and `just deploy-confinement` remain compatibility recipes
for the default backend under the name `plasmapong-confinement.z64`.
`CONFINEMENT_FIXED=1 CONFINEMENT_RSP=0` selects the portable integer CPU reference
in `build/confinement_cpu`. Both require fixed-point velocity; use a separate
build directory for profiling, scripted input or boot tests.

Curl is a signed halfword: half the raw Q4 velocity-difference stencil, rounded
to nearest with ties toward positive infinity. Dividing by 96 gives physical
curl. Its ±32766 bound keeps the differences of absolute curl within a signed
vector lane, and the squared gradient length below 2^31. Storage shares the
existing curl/divergence union, so the game state remains **44,784 bytes**.

The confinement direction uses the RSP's 32-bit reciprocal-square-root lookup,
followed by full 16×32-bit products to retain precision for large gradients.
The force amplitude uses a Q15 timestep coefficient; both amplitude and force
are rounded, and output velocity retains its ±1023.9375 pixels/second limit.
Zero gradients produce zero force. Nonzero gradients omit the old float epsilon;
together with curl quantization, this changes forces most noticeably for tiny
perturbations in almost-uniform vortices. These are deliberate approximations,
not a bit-exact replacement for the float calculation.

`src/rsp_confinement.S` supplies separate curl and confinement commands. Each
streams complete rows, reloads its scratch, and waits for DMA completion before
using a row. The two-cell confinement border is preserved. CPU wrappers write
back input cache lines, invalidate outputs and wait before the next stage.
Linked overlay sizes are **1,552 bytes IMEM** and **1,088 bytes DMEM** (560 bytes
of data plus 528 bytes of scratch). Timings include cache maintenance, queueing,
DMA and completion waits.

Adding a third overlay exposed a cache-line sharing issue in the pinned embed
rule: an overlay's eight-byte empty saved state could share a CPU cache line
with the next overlay's code. Registering that next overlay cached the line
before RSP state DMA wrote it. Eight trailing padding bytes in each custom
overlay separate the state from the following blob; numerical buffers were not
involved. The combined validation run checks the resulting overlay transitions.

The instruction/pipeline reference is the
[SGI RSP Programmer's Guide](https://ultra64.ca/files/documentation/silicon-graphics/SGI_Nintendo_64_RSP_Programmers_Guide.pdf).
`tools/generate-rsqrt.py` generates the portable lookup constants from the integer
ROM definition also modeled by [ares](https://github.com/ares-emulator/ares/blob/5f2f7dc0d/ares/n64/rsp/rsp.cpp).
The host implementation performs no per-cell float square root or division.

Against the frozen `fae1bf7` baseline, with the pinned toolchain, 48 × 33 grid,
Ares `5f2f7dc0d`, paraLLEl-RDP, audio and identical scripted controller inputs:

| Measurement | Float curl/confinement | Integer + RSP | Reduction |
| --- | ---: | ---: | ---: |
| Profiled curl | 0.881 ms | 0.303 ms | 65.6% |
| Profiled confinement | 3.356 ms | 0.534 ms | 84.1% |
| Profiled curl + confinement | 4.237 ms | 0.837 ms | 80.3% |
| Uninstrumented simulation | 16.969 ms | 13.611 ms | 19.8% |
| Uninstrumented drawing | 2.310 ms | 2.435 ms | −5.4% |
| Uninstrumented simulation + drawing | 19.279 ms | 16.046 ms | 16.8% |

Each of the four main runs lasted 55 seconds and produced ten complete
150-step/frame windows. The submission interval remains about 33.35 ms: the
saving is **3.23 ms of work per update**, with the 30 Hz schedule unchanged.
These are emulator timings, not measured hardware gains. The differing fluid
and ball trajectories can also change some gameplay-dependent work.

A 35-second/six-window control runs the same integer algorithm on the CPU:
curl + confinement averages **3.877 ms CPU versus 0.840 ms RSP** in matching
first-six-window measurements. This portable scalar reference saves little
compared with the original float loop; the vector implementation and RSP lookup
provide most of the benefit. All final timing and validation logs are free of
DMA cache warnings after the saved-state padding fix.

Validation and accuracy:

- The 64-field suite covers zero and constant flow, extreme stencils, random
  fields, impulses, near-zero gradients and nearly uniform vortices with a
  one-LSB perturbation. Zero dt, 1/60, 1/30 and 0.2-second steps are covered.
  RSP curl and velocity output must match the integer reference exactly; source
  integrity, DMA guards, overlay switching and untouched borders are checked.
- Maximum tested one-step force error versus the frozen float calculation is
  **0.8125 pixels/second at normal timesteps**, or **4.6875 pixels/second** including
  the 0.2-second stress step. The near-uniform-vortex fixture produces the largest
  difference. These are measured fixture maxima, not universal error bounds.
- Gameplay and arcade suites, the 120-second stability replay, and address/undefined
  sanitizers pass. The default float-confinement backend's 120-second host replay
  remains identical to the frozen `fae1bf7` baseline.
- Under identical fluid-only forcing (60 seconds of jets, 60 seconds of fade),
  average RMS velocity changes by **−0.57%**; maximum sampled velocity is 428.0
  pixels/second and no sampled cells saturate. Final RMS is 0.0065 versus 0.0035
  pixels/second. The separate gameplay replay's average RMS differs by +0.91%.
  Local fields, ball paths and scores can diverge despite similar aggregate flow.
- All four RSP fixture suites and a 35-second level-101 arcade run pass with the
  RDP validator enabled, including transitions between all custom overlays.

The host-model comparison in `build/confinement_compare/preview.html` provides
6-, 18-, 60- and 120-second snapshots of identical fluid forcing and gameplay.
It is not an emulator capture; the integer kernels are checked separately on RSP
emulation. Logs, ROMs, per-frame CSVs and numeric summaries are retained in
`build/confinement_compare/`. Hardware playtesting succeeded; hardware timing has not been measured.

### Flow display options

Choose **OPTIONS** from the main menu. Up/down selects **FLOW EFFECT** or
**RESOLUTION**; left/right changes the selected value. Flow effects are **NONE**,
**PARTICLES**, **PARTICLE TAILS**, **SPEED**, **VORTEX**, **RELIEF**, **BANDS**, and **PRESSURE**. **LOW RES** uses 320 × 240 at
60 FPS; **HIGH RES** uses 640 × 480 interlaced with 60 updates per second. B returns to the main menu. The animated background
previews the selected effect. Each change automatically saves to cartridge EEPROM;
missing storage or a failed write is shown in the options screen. Successful
saves are silent, without confirmation text in the options or high-score screens.

Particles use 96 visual-only tracers sampled from the current velocity field at
the selected simulation frequency, with periodic distributed respawns. Heads and
tails are single framebuffer pixels, strongly tinted cyan, coral or gold by the dominant local dye (mixed/clear
fluid uses a dim blue-grey). Four colour batches per layer keep render-state changes
bounded; tinting reads one grid cell per particle without extra interpolation.
Particle tails add four fading history markers spanning eight simulation ticks
(about 267 ms), with a 24-arena-pixel maximum extent. They freeze with gameplay and
do not change the fluid or ball physics. SPEED maps velocity magnitude to a fixed
colour spectrum: dark navy at rest, blue at 16, cyan at 32, green at 64, yellow
at 128, and red at 256 or more arena pixels/second. The magnitude is the existing
inexpensive max-plus-half-min approximation. Integer interpolation provides
smooth transitions, with wider high-speed bands to expose weaker currents.
Colours depend only on velocity, so dye concentration cannot hide fast flow.
SPEED uses the existing texture upload, with no contour overlay or extra texture.
NONE preserves the original dye rendering.

The new VORTEX, RELIEF, BANDS and PRESSURE prototypes share this texture path.
Their screenshots, emulator performance results and hardware acceptance recipes
are in [Fluid visualisation prototypes](docs/flow-visualizations.md).

The version-2 EEPROM record preserves the two 144-byte slots and existing high
scores. Version-1 saves load with flow effect NONE. Settings are retained when
starting a new match or arcade run. `just smoke-flow` builds a separate scripted
ROM cycling the effects every 450 simulation ticks with RDP validation enabled.
Host checks cover navigation, save migration/corruption, tracer movement and pause,
and identical physics with effects enabled. Hardware timing and visual tuning
still require a console playtest.

The flow-effect validation ROM completed a 68-second Ares run with all four modes,
no RDP validation errors, and approximately 33.3 ms average frame intervals.
The separate EEPROM fixture recovered both its score and PARTICLE TAILS setting
after restart and verified a write to the alternate slot. These are emulator
checks, not hardware measurements. Logs are in `build/flow-effects/`.

After removing contours and switching SPEED to the velocity spectrum, its drawing
cost measured about 3.6 ms/frame in Ares with RDP validation enabled, versus about
13.4 ms with contours. The 68-second validation run maintained approximately
33.3 ms frame intervals with no RDP validation errors; hardware timing is untested.
The log is `build/flow-effects/spectrum-emulator.log`.

### Further RSP and renderer optimization (2026-10-03)

The default still updates and submits frames at **30 Hz**. Grid resolution,
pressure iterations, numerical formats, physics, audio samples and visual effects
are unchanged. `PREPARE_RSP=1` adds a fourth overlay when RSP velocity is enabled;
`PREPARE_RSP=0` retains CPU preparation for comparison. Use distinct build
directories when changing flags.

- Eight-lane RSP backtraces preserve the CPU's integer arithmetic exactly,
  including the final-cell fractional clamp. The initial scalar RSP prototype
  saved almost no simulation time and was replaced by this vector version.
- RSP divergence produces the same interior Q12 words. RSP dye-to-RGBA32
  conversion preserves every output color and leaves row padding untouched.
  Each public operation includes cache maintenance, DMA and a completion wait.
- The first pressure pass now uses constant-offset unrolled cells, as subsequent
  passes already did. The eight-pass lexicographic solve remains bit-exact.
- A bounded 32-entry text command cache avoids repeatedly parsing unchanged HUD
  labels. Only entries from completed frames can be evicted; static block
  recording and cache overflow use the original text path. Three/four-player
  corner masks also reuse a static command block; alive/dead wall colors remain
  dynamic. Their host SVG output is byte-identical to the original.
- Audio mixes in 64-sample blocks, skipping inactive voices once per block.
  PCM, gain ramps, looping, panning and saturation match the old mixer exactly.
  CPU texture generation (including SPEED) now writes through the data cache
  and writes back before RDP upload.

Reproduce measurements and validation with:

```sh
just benchmark-prepare
python3 tools/benchmark-ares.py plasmapong-prepare-benchmark.z64 build/prepare.log
just benchmark-complete
python3 tools/benchmark-ares.py plasmapong-complete-benchmark.z64 build/complete.log
```

The runner uses a private copy of the local ares settings, permits emulation
while unfocused, runs until ten complete 150-step/frame windows exist, and writes
raw logs plus JSON means/ranges. Run benchmarks sequentially. `FLUID_PROFILE=1`
also separates pixel conversion, texture submission, previous-frame wait and
other draw work. Audio interrupt time is included in elapsed stage measurements;
the separate audio total must not be added to those measurements again.
`DRAW_SYNC_PROFILE=1` adds a final RSP/RDP wait inside the drawing timer to measure
completed drawing; this wait is absent from normal builds.

Validation covers 80 exact trace/color/divergence fields, full supported timestep
range, saturation and edge cases, source integrity, padded rows and DMA guards;
the existing 64-field pressure, velocity, dye and confinement suites also pass.
The blocked mixer matches the original PCM and complete voice state across
1,200 variable-sized callbacks and passes address/undefined sanitizers. Portable
physics, arcade, save, multiplayer, sound and numerical tests pass.

Measurements use the pinned libdragon toolchain, ares `5f2f7dc0d`, paraLLEl-RDP,
audio enabled, and the same scripted inputs against baseline `fae1860`.
Two-player comparisons use ten complete windows; four-player comparisons use
six. These are emulated elapsed times, not hardware measurements or worst-case
individual-frame bounds.

| Completed-frame measurement | Baseline | Optimized | Reduction |
| --- | ---: | ---: | ---: |
| 2P simulation | 13.851 ms | 10.993 ms | 20.6% |
| 2P drawing, including RSP/RDP completion | 2.790 ms | 1.322 ms | 52.6% |
| 2P simulation + completed drawing | 16.641 ms | 12.315 ms | 26.0% |
| 4P simulation | 15.233 ms | 12.455 ms | 18.2% |
| 4P drawing, including RSP/RDP completion | 3.904 ms | 1.931 ms | 50.6% |
| 4P simulation + completed drawing | 19.137 ms | 14.386 ms | 24.8% |

The ordinary asynchronous two-player build measures **10.997 ms simulation +
1.027 ms draw submission**, versus **13.862 + 2.386 ms** before. Completed-frame
measurements above include the remaining drawing tail and an explicit diagnostic
fence. Submission intervals remain about **33.34 ms** in both builds.

The largest remaining profiled stages are **pressure solving (2.855 ms)**,
**dye advection (2.507 ms)**, **velocity advection (1.904 ms)** and the **CPU pressure
gradient (1.525 ms)**. Backtraces are included in the advection stages. Previously,
velocity/dye advection took 2.988/3.510 ms, divergence 0.786 ms (now 0.433), and
pressure solving 3.180 ms. The gradient is the next substantial CPU candidate for
RSP work, but requires preserving its signed 32-bit differences and exact rounded
division. Pressure is already on RSP; its serial left-neighbor dependency limits
simple vectorization without changing the solver.

Outside the simulation, profiled pixel conversion fell from about **1.581 to
0.573 ms**, and other drawing work from **0.749 to 0.439 ms**. The baseline detail
run has four windows; these are supporting diagnostics, not the main ten-window
comparison. Audio elapsed time per output sample fell from about **3.61 to
2.39 microseconds**. Particle tails still add dynamic draw commands and tracer
sampling; four-player effects should be budgeted separately from plain two-player
play. RDP validation adds substantial overhead and is not used for the performance
table.

Logs, frozen baseline sources, exact-fixture results, screenshots, intermediate
iterations and numeric summaries are retained in `build/perf_iteration/`.

A final 70-second/fourteen-window four-player run cycled NONE, PARTICLES,
PARTICLE TAILS and SPEED with RDP validation enabled, maintained roughly 33.34 ms
submission intervals, and reported no RDP/DMA errors. Emulator screenshots of
two-player dye rendering and four-player SPEED were visually checked.

### Exact gradient and SPEED offloads after `fae1c0d` (2026-10-03)

Commit `fae1c0d` saves the previous performance checkpoint. The next iteration
keeps the same 30 Hz schedule and adds these default optimizations:

- **Pressure gradient on RSP:** eight signed lanes retain the full 32-bit pressure
  difference. The existing rounded division by 3072 is computed exactly using
  `n=(abs(difference)+1536)>>10` and `floor(n*43691/131072)`. For `n>=98304`, the
  final velocity clamp permits saturating the intermediate quotient to 32767.
  This is integer arithmetic, not an approximate reciprocal. Normal wall
  velocities are zeroed and tangential edge velocities receive the same update.
  Three rotating pressure rows fetch each row only once.
- **SPEED pixels on RSP:** vector magnitude calculation retains the existing
  max-plus-half-min rule. A 260-word DMA-aligned palette is generated once from
  the original CPU interpolation code. The palette contains all 257 distinct
  integer speed entries plus padding; no colors or thresholds change.
- **Queued advection:** backtrace generation and interpolation share one final
  completion wait. Trace buffers are invalidated before enqueueing, remain
  untouched by the CPU while owned by RSP, and are consumed in queue order.
  The standalone trace API remains synchronous for callers needing CPU access.
- **Pressure DMA prefetch:** double-buffered divergence rows overlap the next
  row's input transfer with the existing scalar solve. The following synchronous
  pressure write completes the prefetch before its buffer is consumed. The
  solver's order, eight iterations and full Q12 words remain unchanged.

`GRADIENT_RSP=0`, `SPEED_RSP=0` and `ADVECTION_CHAIN=0` retain the corresponding
CPU or separate-wait paths for comparison. They only take effect with RSP
preparation enabled. `SMOKE_EFFECT=3` selects a fixed SPEED benchmark;
`just benchmark-speed` builds it with stage and completed-drawing timers. As
before, use separate build directories for flag combinations.

The preparation overlay uses **3,472 bytes IMEM** and **2,944 bytes DMEM**
(including its header/state); pressure uses **3,936 bytes IMEM** and **1,520 bytes
DMEM**. A tested alternative that vectorized independent pressure neighbor sums
was bit-exact but slower in ares (about 3.85 versus 2.85 ms for pressure). It was
removed; its source and measurements remain in `build/rsp_next/`.

Validation adds **162 gradient fields** covering rounding ties, carry boundaries,
quotient saturation, small/large random pressures, every wall and DMA guards.
The reciprocal identity is exhaustively checked over its 98,304-value
unsaturated domain. The 80-field preparation suite also checks SPEED pixels,
padded rows and source integrity, and the velocity fixtures exercise the complete
queued backtrace/interpolation path. Existing pressure, dye, velocity, confinement
and portable gameplay suites remain enabled.

Final ares comparisons use the same pinned toolchain/emulator and scripted inputs
as above. The two-player default comparison reruns the checkpoint for ten windows;
SPEED and four-player results use six windows. SPEED runs include stage profiling
on both sides. The four-player checkpoint is the preceding iteration's measured
result. All performance rows include completed RSP/RDP drawing, with validation
disabled.

| Completed-frame measurement | `fae1c0d` | This iteration | Reduction |
| --- | ---: | ---: | ---: |
| 2P simulation | 10.993 ms | 10.159 ms | 7.6% |
| 2P completed drawing | 1.322 ms | 1.316 ms | 0.5% |
| 2P simulation + completed drawing | 12.315 ms | 11.475 ms | 6.8% |
| 2P SPEED completed drawing | 3.276 ms | 1.391 ms | 57.5% |
| 2P SPEED simulation + completed drawing | 14.459 ms | 11.605 ms | 19.7% |
| 4P simulation + completed drawing | 14.386 ms | 13.535 ms | 5.9% |

Relative to the original `fae1860` baseline, normal two-player simulation plus
completed drawing is now **31.0% lower** (16.641 to 11.475 ms). These averages
remain emulator measurements, not individual-frame bounds or hardware guarantees.
Submission intervals remain approximately **33.34 ms**; the 30 Hz target is unchanged.

The final profiled SPEED run puts the largest stages at pressure **2.776 ms**,
dye advection **2.307 ms**, velocity advection **1.825 ms**, gradient **0.970 ms**,
and splats **0.647 ms**. The gradient previously cost **1.517 ms** in the matching
SPEED run. Separate controlled six-window runs attribute about **0.116 ms** to
sharing the advection completion wait and **0.069 ms** to pressure DMA prefetch.
Pressure and advection are already on RSP; further gains need better memory access
or scheduling, rather than simply moving those stages. CPU splats remain a smaller
offload candidate; particle tails and tracer sampling also warrant separate
effect-specific profiling.

All six on-console numerical suites passed, including the new 162-field gradient
suite. A fourteen-window four-player run cycled all four effects with RDP validation
and no reported assertion, RDP or DMA errors. The portable test suite and CPU-only
fallback build also pass. Logs, intermediate experiments, source/ROM hashes and
aggregate results are retained in `build/rsp_next/` (`results.json`).


### 60 Hz migration and console presentation monitor (2026-10-03)

The default timestep is now 1/60 second. Charge still takes one second, the
perfect-release window remains about 67 ms, and cooldown lasts five active
seconds. Menu and ball dye emissions and moving-jet forces retain their amount
per second; menu fade uses the square root of its old per-update multiplier.
Arcade currents retain their 10 Hz injection schedule. Tracers retain their
1.5–4.5 second lifetimes and approximately 267 ms histories, sampled into the same
five visible tail dots. A circular history avoids copying every sample at 60 Hz.
The shorter fluid-advection timestep loses less momentum to interpolation; the
four-second residual-flow test now checks the measured 60 Hz float/fixed range.

Four-player tails initially exceeded the new budget. The final path prepares all
dots for each tracer while its history is cached, then submits their existing
back-to-front/color order in one RDP command buffer. Explicit pipe syncs surround
raw color changes, and the frame-start RSP/RDP wait protects buffer reuse.
Gameplay and UI source files also use `-O3`. No particles, visible tail layers,
fluid cells or pressure iterations were removed.

The monitor samples VI_ORIGIN after libdragon's display swap handler, counting
actual framebuffer changes rather than submitted frames or simulation steps.
This is specific to the current non-interlaced display mode. L hides the panel;
R resets the scene's measurements. Numbers update every half second using cached
text commands. A matching ten-window two-player comparison measured **1.028 ms
with the panel versus 0.994 ms hidden** for draw submission: about **0.034 ms/frame**
of additional drawing cost. Both runs retained the sampler and had zero repeated
refreshes. This comparison is an ares measurement, not a hardware overhead bound.

The normal asynchronous rendering path is the performance acceptance path.
`DRAW_SYNC_PROFILE=1` deliberately serializes completed drawing and reduces
pipeline overlap; RDP validation also adds substantial overhead. Those diagnostic
builds can drop frames even when the ordinary build presents every refresh.
Do not use their frame rates as the console release frame rate.

Portable tests cover presentation counting/reset, circular-history wraparound,
wall-clock gameplay durations, and 120 seconds of gameplay at 60 Hz. All six RSP
numerical suites pass. A four-player all-effects RDP validation run reported no
RDP errors. The larger five-command preparation overlay exposed a shared cache
line between the registration command table and empty saved state when embedded
at an eight-byte RDRAM alignment; padding now separates them. The benchmark runner
also treats DMA/cache warnings as failures and supports `--draw-only` for menus.

Measurements, intermediate stress failures, screenshots and final summaries are
retained in `build/60fps/`. Hardware validation is still required; the diagnostic
panel is enabled in the default playable ROM for that purpose.

Final ares presentation checks (audio and HUD enabled, no diagnostic draw fence):

| Workload | Duration | Presentation result |
| --- | ---: | --- |
| 2P scripted play | ~25 s | No repeated refreshes |
| 4P cycling NONE, PARTICLES, TAILS, SPEED | ~70 s | No repeated refreshes; 16.714 ms average submission interval |
| 4P continuous TAILS | ~45 s | 4 repeats out of 2,694 measured refreshes; longest gap 2 VI |
| Main menu | ~10 s | One initial repeat, no additional repeats after startup |

Continuous tails averaged **12.017 ms simulation + 3.561 ms draw submission**.
The default is now 60 Hz, but the heaviest workload is not yet guaranteed to stay
locked to every refresh.


### Saved frame-rate selection (2026-10-03)

The frame-rate option persists in version-3 EEPROM records, using one formerly
unused byte and the existing checksummed, alternating-slot write sequence.
Version-1 and version-2 records remain readable. Selecting a menu row does not
write EEPROM; changing its value does. New matches and rematches retain the
setting. Switching frequency discards the accumulator after the save, avoiding a
catch-up burst. Timers use a 60 Hz counter base, advancing by two units at 30 FPS.
Tail history interpolates the intermediate sample at 30 FPS to retain its duration.

Portable tests cover both rates across all four host fluid backends, menu input
debouncing, restart retention, charge/grace/cooldown duration, paddle motion,
tracer history, save migration, invalid saved values, and presentation counting.
The dedicated EEPROM ROM verifies 30 FPS and then 60 FPS across successive boots.
Ares automatically flushes save memory every 30 host seconds, so persistence runs
must allow that interval before terminating the emulator.

Four-player continuous-tail benchmarks with audio and HUD enabled are retained
in `build/fps_options/`. At 30 FPS, simulation averaged **12.776 ms** and drawing
submission **3.731 ms**, with **33.351 ms** average frame intervals. The free-running
scheduler produced 32 intervals of three refreshes across 1,780 measured refreshes;
30 FPS is not perfectly locked to alternate VIs. At 60 FPS, simulation averaged
**12.545 ms** and drawing submission **3.710 ms**, with **17.496 ms** average frame
intervals and 83 repeated refreshes out of 1,873. This demanding run is worse than
the earlier fixed-60 checkpoint; neither mode should be described as perfectly
paced based on emulator averages. Console testing remains the final check.

### Multiplayer visibility and ring rendering (2026-10-03)

The 3P/4P court is visually smaller, but it still uses the complete **48 × 33**
fluid grid and full **288 × 198** fluid texture. The square court covers the side
strips and corner wedges after drawing. Hidden fluid cells continue participating
in pressure and advection, and all 96 tracers continue updating. Extra paddles,
jets, suction and HUD elements therefore add work without reducing the solver.
A true 33-column square simulation would remove about 31% of cells, but would also
change boundary conditions and flow; this is not an equivalent rendering change.

Two rendering changes avoid work without changing physics or particle density:
tracer dots behind the square court's side masks are omitted, and each ring's 20
invariant sine/cosine direction pairs are evaluated once, using the platform's
own math implementation. Ring positions, pulse animation and clipping are retained.
The corner-mask block also groups fills by color; tested alone this did not improve
presentation, so it is not counted as a measured saving.

Matched eight-window ares runs use 60 FPS, continuous TAILS, scripted controllers,
audio and HUD enabled, without a diagnostic draw fence:

| Workload/change | Simulation, ms/step | Draw submission, ms/frame | Frame interval, ms | Repeated VI |
| --- | ---: | ---: | ---: | ---: |
| 2P baseline | 11.171 | 3.232 | 16.714 | 0 / 1,194 |
| 4P baseline | 12.801 | 3.750 | 17.885 | 83 / 1,273 |
| 4P grouped mask only | 12.767 | 3.798 | 17.885 | 83 / 1,273 |
| 4P plus hidden-dot culling | 12.688 | 3.544 | 17.132 | 29 / 1,219 |
| 4P plus cached ring directions | 12.643 | 3.382 | 16.757 | 3 / 1,193 |

Draw submission drops about **0.368 ms (9.8%)** against the 4P baseline. The solver
was not changed; differences in simulation averages include scheduling and replay
progress when frames are missed. These timings are not serialized CPU+RSP+RDP
completion measurements. The portable suite passes; deterministic host previews
retain identical pixels for the mask/culling changes and identical SVG output for
active-suction ring caching. Logs and comparisons are in `build/multiplayer_perf/`.

The final 18-window continuous-tail run (~45 seconds) recorded **3 repeated
refreshes out of 2,693**, longest gap 2 VI. A separate RDP-validation run completed
without assertion, RDP or DMA/cache warnings. Its slower instrumented frame rate
is not the acceptance measurement. The default playable ROM includes these changes;
hardware confirmation is still needed before claiming a locked 60 FPS.

### Current-game profiling baseline (2026-10-03)

Fresh builds of `fae17e5e16c66bd745dde2ab78606c036c9e2375`, before further game
optimisations. Ares ran each ROM separately for twelve 150-frame/150-PLAY-step
measurement windows (about 30 emulated seconds). All runs explicitly select
60 FPS and a fixed flow effect, with scripted controllers, audio and the HUD
on. Default RSP backends are enabled; neither RDP validation nor the diagnostic
end-of-draw fence is enabled. No warm-up windows are discarded. Drawing windows
include the short startup/menu transition; simulation windows count PLAY only.
The replay includes moving paddles, jets and intermittent suction, and normal
match progression; it is not a bound for every possible four-player action.

| Workload | Simulation ms/step | Draw submission ms/frame | Sum, ms | Frame interval, ms | Repeated VI |
| --- | ---: | ---: | ---: | ---: | ---: |
| 4P NONE | 11.606 | 1.426 | 13.032 | 16.714 | 0 / 1,790 |
| 4P PARTICLES | 12.423 | 2.092 | 14.516 | 16.714 | 0 / 1,790 |
| 4P TAILS | 12.451 | 3.388 | 15.838 | 16.789 | 7 / 1,797 |
| 4P SPEED | 11.606 | 1.601 | 13.207 | 16.714 | 0 / 1,790 |
| 2P TAILS | 11.177 | 3.128 | 14.305 | 16.714 | 0 / 1,794 |

The sum is a useful approximate work budget, not a serialized CPU+RSP+RDP
completion measurement. Simulation and drawing windows are independently
counted; simulation can overlap the preceding frame's rendering. Four-player
tails has only **0.829 ms nominal average margin** against 16.667 ms and still
misses refreshes (longest gap: two VIs). Window averages hide individual-frame
spikes; the runner's min/max values are extrema of window averages, not frame
percentiles. These are emulator observations, not a hardware 60 FPS guarantee.

A separate `FLUID_PROFILE=1` 4P TAILS run gives the following disjoint breakdown.
The profiled run totals 12.652 ms simulation + 3.256 ms draw submission and has
9 repeats / 1,799 VI. Instrumentation changes timing and scheduling, so use the
ordinary ROM above for presentation acceptance and this run for attribution.

| Simulation stage | ms/step |
| --- | ---: |
| Dye advection | 2.351 |
| Velocity advection | 1.929 |
| Pressure solve (eight passes) | 2.817 |
| CPU splats (paddles, jets, bursts) | 1.435 |
| Pressure gradient | 1.025 |
| Velocity sampling (tracers and gameplay) | 0.604 |
| Confinement | 0.538 |
| Divergence and wall preparation | 0.439 |
| Curl | 0.303 |
| Suction/burst pumps | 0.257 |
| Ball dye | 0.060 |
| Velocity/dye bank swaps combined | 0.004 |
| Remaining simulation and profiling overhead | 0.891 |

| Drawing stage | ms/frame |
| --- | ---: |
| Tail point preparation | 1.520 |
| Tail command emission/submission | 0.333 |
| Fluid pixel generation | 0.727 |
| Texture command submission | 0.005 |
| Frame-start completion wait | 0.046 |
| Remaining UI/court/HUD/submission work | 0.624 |

Stage timings include interrupts/audio occurring inside them and, for RSP
stages, CPU cache maintenance, queue submission and completion waits. They are
not isolated coprocessor execution timings. Audio must not be added again.
Tail preparation and emission are already included in `draw_other`; the table
subtracts them to avoid double counting. Texture submission does not measure
RDP texture rendering, and the small frame-start wait does not imply the RDP
has no cost: other work can hide completion latency.

Recommended optimisation targets:

1. **Shared RSP advection: 4.280 ms**, approximately 27% of profiled simulation
   plus drawing. Both velocity (two channels) and dye (three channels) use
   `fluid_channels_rsp` / `rsp_dye.S`, so one improvement benefits every mode.
   The current kernel reloads traces per channel and synchronously transfers
   24-cell trace/output chunks around scalar-addressed gathers and vector
   interpolation. Benchmark larger chunks or DMA double buffering within DMEM
   limits, and separate trace generation, cache work and kernel completion
   before choosing a rewrite. A hypothetical 20% stage reduction would recover
   about **0.856 ms**; that saving has not yet been demonstrated.
2. **Tail preparation: 1.520 ms CPU-side**, plus 0.333 ms emission. This is a
   contained first experiment for the workload currently missing refreshes:
   inspect history memory traffic and per-dot coordinate/math work while
   retaining all layers, clipping and draw order. TAILS adds about 1.295 ms of
   draw submission over PARTICLES in the ordinary runs. Both effects update
   the same tracers, so this comparison points directly at tail rendering.
3. **CPU splats: 1.435 ms average**, ranging from 1.095 to 1.798 ms across the
   profiled windows. Repeated float/fixed conversions and full bounding-square
   processing in `fluid_splat` are candidates for equivalent simplification
   or batching. This work grows with active paddles/jets and is relevant to
   adding mechanics. Check zero-weight cells and numerical equivalence before
   skipping or combining writes.
4. **Pressure solve: 2.817 ms**, the largest single stage, but a less contained
   change: the existing unrolled Gauss-Seidel kernel already prefetches
   divergence and has a serial left-neighbour dependency. Further DMA overlap
   is worth investigating; vectorizing or changing the solver can alter the
   fluid. Pressure gradient (1.025 ms) is another shared RSP candidate.

Reducing grid size, pressure iterations or particle density would change the
simulation or effect and is not needed to begin these experiments. A useful
next-round target is to reclaim 2–3 ms in the heavy workload, with frame-level
spike measurements and a longer presentation run before spending that budget
on new effects.

Raw logs, per-run JSON, aggregate `results.json`, and source/ROM/emulator hashes
are retained in `build/round2/`. The benchmark runner now preserves existing
draw/flow detail lines in JSON and waits for complete flow windows. Its parser
was checked by replaying the captured profile log and comparing every summary.
No game code or playable default ROM was changed for this profiling round.

Reproduce the heavy baseline and its instrumented counterpart:

```sh
./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_FPS=60 SMOKE_EFFECT=2 ROM=round2_base4 BUILD_DIR=build/round2/base4
python3 tools/benchmark-ares.py round2_base4.z64 build/round2/base4.log --windows 12 --timeout 240
./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_FPS=60 SMOKE_EFFECT=2 FLUID_PROFILE=1 ROM=round2_profile4 BUILD_DIR=build/round2/profile4
python3 tools/benchmark-ares.py round2_profile4.z64 build/round2/profile4.log --windows 12 --timeout 240
```

Use distinct ROM/build directories for comparisons: `SMOKE_EFFECT=0`, `1`, `3`
select NONE, PARTICLES and SPEED; `SMOKE_PLAYERS=2` selects the two-player replay.

### Second optimisation round: more 60 FPS headroom (2026-10-03)

The final ordinary build reduces four-player continuous TAILS work from
**15.839 ms to 13.649 ms**, a **2.190 ms / 13.8% reduction**. Nominal average
headroom against 16.667 ms grows from 0.829 ms to **3.018 ms**. These comparisons
use the first twelve windows of the same scripted workload, audio/HUD enabled,
with neither fluid profiling, RDP validation nor the diagnostic drawing fence.

| Matched workload | Simulation ms/step | Draw submission ms/frame | Sum, ms | Frame interval, ms | Repeated VI in first 12 windows |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original 4P TAILS | 12.451 | 3.388 | 15.839 | 16.789 | 7 / 1,797 |
| Final 4P TAILS | 10.887 | 2.761 | 13.649 | 16.715 | 0 / 1,790 |
| Original 2P TAILS | 11.177 | 3.128 | 14.305 | 16.714 | 0 / 1,794 |
| Final 2P TAILS | 9.821 | 2.647 | 12.469 | 16.715 | 0 / 1,794 |

The final 24-window four-player run (about 60 emulated seconds) averaged
**10.756 ms simulation + 2.741 ms drawing = 13.498 ms**, with no repeated refreshes
reported in any window and a longest observed gap of one VI. The presentation
counters reset on a match transition; the last snapshots of its two observed
segments account for 3,481 refreshes. That count excludes any refreshes between
the last pre-transition snapshot and the reset. Normal match progression also
changes workload over time, so the longer-run average is not used to claim a
larger improvement against the twelve-window baseline.

Retained changes:

- Advection traces store eight offsets, eight X weights and eight Y weights
  together. RSP vector loads/stores replace scalar weight loading and trace
  interleaving. DMA batches grow from 24 to 48 cells; consumed trace storage
  doubles as the output buffer, keeping the full source grid inside DMEM.
  Trace values, interpolation, decay and rounding are unchanged.
- The pressure solver pipelines the same eight Gauss-Seidel passes over a
  wavefront of rows. Nine pressure rows and eight divergence rows remain in
  DMEM; divergence is read once and only final pressure is written to RDRAM.
  A pass processes row `tick - pass`, after its below-row dependency is ready.
  Top/bottom copied boundaries and the serial left dependency remain exact.
- Fixed-storage splats work in Q4 storage units, avoiding redundant float
  decode/encode and the wider storage clamp. Zero-weight cells use integer
  clamps and still apply the original pigment limits: simply skipping those
  cells would have changed the result.
- Tracer history stores adjacent X/Y pairs, with all tracers for a given time
  sample contiguous. This improves cache reuse in simulation and drawing.
  All 96 tracers, 17 history samples and five visible tail layers remain.
- Tail clipping compares integer pixel bounds, and tails guaranteed to lie
  entirely behind the side masks are rejected before preparation. A conservative
  25-unit halo retains the existing 24-unit maximum tail extent and rounding.

Matched eight-window diagnostic profiles show where the work was reduced:

| Profiled stage | Original, ms | Final, ms |
| --- | ---: | ---: |
| Velocity + dye advection | 4.319 | 3.682 |
| Pressure solve | 2.815 | 2.504 |
| CPU splats | 1.574 | 1.196 |
| Tail point preparation | 1.520 | 1.027 |

These stage averages include interrupt time and cache/queue/completion costs.
They are attribution measurements, not additions to the ordinary-build results.
Tail command emission did not improve (0.328 to 0.363 ms in these profiles).
A smaller, color-major tail loop was slower and was discarded. A separate
48-cell trace/output allocation exceeded DMEM; the retained batch implementation
reuses consumed trace space instead. Paired per-tracer history improved timing,
but the final time-major layout improved it further.

Validation completed:

- The full portable suite passes across float, fixed dye, fixed velocity and
  fixed confinement backends; all four 120-second physics trace hashes match
  the pre-change run. Both 30/60 FPS history and timing tests pass.
- All six RSP numerical suites pass, including **128 bit-exact pressure fields**
  with impulses at both side edges of every interior row, DMA guards and overlay
  switches. Trace/advection fixtures cover both frame rates and extreme inputs.
- **2,000 randomized full-grid splats** match the frozen original implementation.
  A portable golden-hash regression now retains that check in `tools/check.sh`.
- **32 complete SVG outputs** match the original byte-for-byte, covering menu,
  2P/3P/4P, particles/tails and multiple replay lengths.
- A 28-window four-player run cycles all four effects under RDP validation;
  a separate eight-window final-tail validation run covers the final cull.
  Neither reports assertions, RDP errors or DMA/cache warnings. Validation
  overhead causes missed refreshes and is not the performance acceptance path.
- The CPU fallback and default playable `plasmapong.z64` both build successfully.

Raw logs, experiment JSON, original source snapshots, hashes and final aggregate
results are retained in `build/optimise2/`; `results.json` includes the matched
first-twelve-window comparison. No grid cells, solver passes, gameplay mechanics
or visible effect density were removed. The timing sum remains an approximate
simulation/submission budget, not serialized GPU completion or a per-frame
worst-case bound. Hardware confirmation is still needed before claiming a locked
60 FPS on console.

```sh
./tools/check.sh
./tools/build-rom.sh -j4
./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_FPS=60 SMOKE_EFFECT=2 ROM=opt2_final4 BUILD_DIR=build/optimise2/final4
python3 tools/benchmark-ares.py opt2_final4.z64 build/optimise2/final4.log --windows 24 --timeout 360
```

### Low/high resolution selection (2026-10-03)

Options now selects **LOW RES** (320 × 240 progressive at 60 FPS) or
**HIGH RES** (640 × 480 interlaced at 30 FPS), replacing the frame-rate row.
The existing EEPROM values retain their format: 60 selects low res and 30
selects high res. The default is low res. Fonts, particles and geometry double
in high res; the 48 × 33 fluid grid and gameplay coordinates stay unchanged.
Three 16-bit high-res framebuffers occupy 1,843,200 bytes (1.76 MiB).

The portable suite passes. `just smoke-video` exercises real Options inputs,
switches both ways under RDP validation, writes EEPROM and returns to gameplay.
The replay also restored a saved high-res choice on its next boot. The corrected
font and high-res Options layout were visually checked in Ares. Logs and a screen
capture are retained in `build/video-switch.log` and `build/hires-options.png`.

A six-window ordinary four-player TAILS replay in high res averaged **11.107 ms
simulation**, **2.812 ms draw submission**, and **33.349 ms per frame**. Its last
presentation snapshot counted 892 new frames over 1,784 fields, with 18 fields
beyond the allowed two-field gap and a longest gap of three fields. This supports
the 30 FPS target but does not establish a perfectly paced or hardware-verified
frame rate. Final measurements are in `build/hires-final.{log,json}`.

### High-resolution work budget and Expansion Pak investigation (2026-10-03)

The high-resolution workload is comfortably below its 33.333 ms work budget in
Ares. The retained changes reduce average completed-frame work by **3.6%** and
the largest observed frame by **7.5%**, while preserving simulation results,
visual effect density and exact audio samples. The normal build enables them;
the Expansion Pak experiment remains opt-in.

Matched twelve-window four-player TAILS runs use the same 30 Hz replay on a
**4 MiB configuration**, audio/HUD enabled, without fluid profiling or RDP
validation. `FRAME_WORK_PROFILE=1` measures from input polling through simulation,
buffer acquisition and RSP/RDP completion, excluding intentional rate-limiter
sleep. Percentiles are conservative upper bounds from 250-us buckets. The
reported bounds below are the largest bounds across the twelve windows, not
averages of percentiles. The runs contain 1,800 measured PLAY frames each.

| Completed frame work | Before | After |
| --- | ---: | ---: |
| Average | 13.858 ms | **13.359 ms** |
| Largest window's p95 upper bound | 17.250 ms | **15.500 ms** |
| Largest window's p99 upper bound | 17.500 ms | **15.750 ms** |
| Largest observed individual frame | 17.616 ms | **16.299 ms** |

The final animated-menu run on 4 MiB averaged **13.827 ms** per frame. Its first
measurement window reached **19.284 ms**; later windows stayed below 14.3 ms.
A separate eight-window low-res
four-player TAILS replay retains 60 FPS presentation with no repeated refreshes
in its reported windows. Frame work and field pacing are different measurements:
occasional three-field gaps remain at 30 FPS because updates are clock-scheduled
rather than locked to alternate VI fields. More CPU headroom alone does not
establish perfect field pacing. Emulator RDP completion is also not a calibrated
measurement of real-console RDRAM contention.

The initial eight-window high-res diagnostic profile identified these costs:

| Work | Mean time |
| --- | ---: |
| Divergence + eight-pass pressure solve + gradient | 3.938 ms/step |
| Velocity + three-channel dye advection | 3.684 ms/step |
| CPU splats | 1.143 ms/step |
| Tail point preparation + command emission | 1.413 ms/frame |
| Dye-to-texture pixel generation | 0.632 ms/frame |
| Complete drawing, including the above drawing stages | 3.077 ms/frame |

Stage measurements include audio interrupts and must not be added to separate
audio costs. In matched profiled audio runs, mixing falls from 64.506 to
39.343 ms of CPU time per audio second, a **39.0% reduction**.

Retained changes:

- Queue curl and confinement together; queue divergence, pressure and gradient
  together. Intermediate DMA results stay under RSP ownership until their final
  consumer completes, avoiding redundant syncpoints and cache transfers.
- Select pigment destinations, weights and limits once per splat, keeping the
  original float operation order, rounding and zero-weight clamps.
- Use a steady-gain audio path with a register-held playback cursor and omit
  redundant ramp tests; centered channels avoid identity gain multiplications.
  The ramp path, truncation, loop endpoints, voice state and PCM output remain
  identical. This file also uses `-O3` in the ROM.
- Add opt-in completed-frame distribution probes and an explicit
  `--memory-mib 4|8` benchmark setting, without changing global Ares settings.

The full portable suite passes with the retained paths, including 2,000 golden
splats, unchanged physics trace hashes and 1,200 variable-size audio callbacks
compared with the frozen mixer. All RSP numerical fixtures pass. New chain tests
cover 64 exact projection fields and 64 exact curl/confinement fields with dirty
caches, boundary cells, overlay switches and DMA guards. A 4 MiB high-res replay
cycles all four effects under RDP validation without errors. The normal ROM and
CPU fallback both build.

An indexed CI4 high-res font was discarded: it increased completed drawing from
about 3.09 ms to 3.88 ms in the matched six-window diagnostic comparison, with
similar results on 4 and 8 MiB. RGBA16 remains the font default.

#### Expansion Pak

The Expansion Pak adds capacity, from 4 to 8 MiB. It does not change the CPU,
RSP's 4 KiB working memory, or the shared RDRAM channel's clock/bandwidth.
Nintendo documents independent active-page registers for 1 MiB banks and
recommends separating busy buffers to reduce page misses. See the
[RDRAM hardware description](https://ultra64.ca/files/documentation/online-manuals/man/kantan/step1/2-4.html),
[buffer-placement guidance](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro04/04-03.html),
and [memory-detection requirements](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/caution/caution/index12.htm).

`EXPANSION_BANKS=1` began as an optional experiment and is now enabled in the
playable build following the hardware validation recorded below.
When libdragon detects an Expansion Pak, a linker allocation wrapper aligns each
640 × 480 16-bit framebuffer to 1 MiB. Each 614,400-byte buffer then occupies one
distinct bank, keeping scanned and rendered buffers separate. The observed
addresses were 0x00100000, 0x00200000 and 0x00300000: this uses the extra memory
headroom for alignment rather than requiring the framebuffers themselves to be
above 4 MiB. Missing expansion memory or a failed aligned allocation falls back
to normal allocation. Framebuffer freeing and libdragon's swap logic are intact.

The same optional ROM succeeds with 4 MiB (fallback) and 8 MiB (aligned buffers).
An 8 MiB Options replay also switches high/low/high repeatedly under RDP
validation, confirming that aligned buffers can be freed and recreated safely.
Matched eight-window completed-work means are **13.450 ms and 13.456 ms**,
respectively: no meaningful emulator gain. Ares' RDRAM array implementation does
not model these bank/page penalties, so only a hardware A/B comparison can judge
this experiment. It is not enabled automatically in production. More buffering
could increase latency, and larger fluid grids would increase the main compute
cost; neither is justified as a performance improvement by these results.

Reproduction (use distinct build directories when changing flags):

```sh
just benchmark-hires
python3 tools/benchmark-ares.py plasmapong-hires-work.z64 build/hires-work.log --windows 12 --memory-mib 4
just benchmark-expansion
python3 tools/benchmark-ares.py plasmapong-expansion-work.z64 build/expansion-work.log --windows 8 --memory-mib 8
```

For the before comparison, set `CONFINEMENT_CHAIN=0 PROJECTION_CHAIN=0
SPLAT_PLAN=0 SOUND_STEADY=0` in a separate build. Raw profiles, comparisons and
source hashes are retained in `build/hires-opt/`; `results.json` contains the
matched results. This is emulator evidence; console work-budget, bank-placement
benefit and field pacing still need hardware confirmation.

### 60 Hz high-res optimisation (2026-10-04)

Both resolution options now use the same nominal 60 Hz physics, input, emission,
timers and tracer history. The saved value `30` still selects high resolution
for compatibility; it no longer selects a simulation rate. On NTSC, 640 × 480
remains interlaced: approximately 60 new fields per second, or 30 complete scans.
The scheduler starts one update after each actual VI field (about 59.94 Hz)
while each physics step remains 1/60 second. Direct field pacing avoids both
the periodic double-step spikes from a 60.000 Hz clock and the occasional
display repeat observed with an estimated VI period. PAL presentation has not been benchmarked.

The optimisation target is **at most 14 ms per update**, leaving about 2.67 ms
inside the nominal 16.67 ms budget. The first completed-work measurements in
Ares with 4 MiB, before the new pressure/audio paths, were:

| Replay | Updates measured | Mean active work | Worst active work |
| --- | ---: | ---: | ---: |
| High res, four players, tails | 1,800 | 12.685 ms | 16.086 ms |
| High res, animated menu | 1,200 | 12.796 ms | 13.999 ms |
| High res, continuous four-player powers | 1,800 | 14.729 ms | 19.332 ms |
| Low res, four players, tails | 1,800 | 12.774 ms | 16.255 ms |

Active work includes input, simulation, audio interrupts and completed RSP/RDP
drawing, but subtracts framebuffer acquisition waits and excludes intentional
limiter sleep. Buffer acquisition averaged about 7–8 us. All four replays
reported no repeated VI fields or presentation misses. Buffering absorbs short
spikes; that does not establish that each update meets the work budget. The
stress replay deliberately bypasses cooldowns and keeps all players alive.
It exposes remaining overload rather than representing ordinary gameplay.
The low-res comparison preceded the final startup clock adjustment. Later
measurements below include the additional pressure and audio optimisations.

With the final direct-field scheduler and both default optimisations enabled:

| High-res replay | Updates measured | Mean active work | Worst active work |
| --- | ---: | ---: | ---: |
| Ordinary four-player gameplay, tails | 1,800 | 11.660 ms | 12.645 ms |
| Animated menu | 1,200 | 12.124 ms | 13.403 ms |
| Continuous four-player powers, cycling all four effects | 7,200 | 12.083 ms | 13.736 ms |
| Arcade replay, starting at level 8 | 1,200 | 11.060 ms | 11.981 ms |

The worst observed update meets the 14 ms target and leaves **2.931 ms (17.6%)**
inside the nominal 16.667 ms budget. These final runs report no repeated fields,
presentation misses or audio underrun observations. The all-effects replay
runs for about two emulated minutes, including repeated effect transitions.
An earlier estimated-period scheduler produced one repeated field after about
72 seconds despite work fitting the budget; direct VI pacing removes that
failure in the longer replay. Reconfiguration, EEPROM writes and initial scene
setup remain outside the steady gameplay budget.

Retained improvements load RSP neighbour windows with vector loads, overlap
pixel generation with CPU drawing preparation, specialise tracer drawing loops,
precompute ring directions and share per-frame suction radius calculation.
Pump force and pigment preparation avoid redundant fixed-point conversions.
`PRESSURE_VECTOR_SUM=1` sums independent neighbours in the RSP accumulator,
while preserving the scalar left-neighbour recurrence and all eight passes.
Paired vector/scalar instructions and early scratch loads hide execution/load
latency. The isolated pressure fixtures improved from 2.443 ms to 1.863 ms per
solve, with every result still bit-exact.

`AUDIO_STREAM=1` generates bounded batches of at most 128 stereo samples on
the main thread. The batch allowance follows elapsed time and the actual AI
frequency, with at most 384 samples generated per update. Mixing overlaps
queued RSP projection; AI interrupts only submit completed buffers. Partial
buffers remain private until `audio_write_end`, and the original mixer and PCM
assets are unchanged. Playback starts after loading/first render, and mode
changes refill spare buffers around the deliberate reconfiguration stall.
EEPROM writes and verification reads service quiet buffers between pages,
preserving the intentional save mute and the journal's commit order. The arcade
replay covers a high-score save and resumes with no AI starvation observations.
Both optimisations are enabled by default and can be disabled independently
for comparisons. The first visible scene's command setup happens during startup,
outside the steady update budget. The 48 × 33 simulation grid, pressure passes, 96 tracers
and five tail layers are unchanged.

The original principal costs were pressure/projection (about 3.8 ms), advection
(about 3.6 ms combined), sustained pumps and audio interrupt bursts. An exact
speculative audio cache was discarded because it increased total work under
stress. Precomputed loop gain/pan tables saved audio arithmetic but also
increased total frame work; cached PCM output stores did not improve the
matched stress replay. These experiments are absent from the playable build.

The portable suite passes, including unchanged physics traces, exact audio
callbacks, partitioned-buffer PCM/voice-state comparisons and 2,000 golden pump
cases. RSP numerical fixtures pass, including
queued pixel production, overlay transitions and buffer guards. A validated
4 MiB replay switches low/high/low/high through Options and cycles all four
effects without RDP errors. A dedicated EEPROM test poisons the in-memory
scores/settings and recovers both journal slots and both resolution choices
from fresh chip reads. Thirty host drawing command streams also match the
pre-optimisation UI exactly. The normal playable ROM builds successfully.
These are emulator and host results; console timing remains unverified.

Build and run ordinary/stress benchmarks separately:

```sh
just benchmark-hires
python3 tools/benchmark-ares.py plasmapong-hires-work.z64 build/hires-work.log --windows 12 --memory-mib 4
just benchmark-hires-stress
python3 tools/benchmark-ares.py plasmapong-hires-stress.z64 build/hires-stress.log --windows 12 --memory-mib 4
just benchmark-hires-effects
python3 tools/benchmark-ares.py plasmapong-hires-effects.z64 build/hires-effects.log --windows 48 --memory-mib 4 --timeout 480
```

Raw logs, distributions, source hashes and a comparison manifest are retained
locally in `build/hires60/`. Percentiles are 250 us histogram upper bounds per
150-update window; averages and maxima retain microsecond precision.

### NTSC console follow-up: high-res 2P (2026-10-04)

The user tested commit `fae1c76` on an NTSC console and reported consistently
**49–51 presented FPS** in high-resolution 2P, with otherwise good gameplay.
A fresh Ares control still presents at 60 fields/s. Optimisation therefore uses
matched Ares work measurements to compare changes, while treating console frame
rate as a separate result requiring another playtest. In this Ares checkout,
RDP commands are rendered through the GPU and full sync raises completion without
modelling the console's pixel-by-pixel execution time. Its completed-work figures
cannot establish real-console RDP fill or shared-memory bandwidth headroom.

The ordinary replay uses two players, high resolution and particle tails, with
normal cooldowns. Both builds collect 12 windows of 150 updates on 4 MiB. A second replay uses
continuous 2P powers and cycles all four effects over 24 windows (3,600 updates):

| High-res 2P replay | Mean active work | Worst active work |
| --- | ---: | ---: |
| Unchanged `fae1c76` control | 11.157 ms | 11.873 ms |
| Retained optimisations | 10.303 ms | 10.945 ms |
| Unchanged control, continuous powers/all effects | 10.611 ms | 12.465 ms |
| Retained optimisations, continuous powers/all effects | 9.719 ms | 11.499 ms |

This is **7.7% less average work** and **7.8% less worst observed work**, with
no reported repeated fields, presentation misses or audio underrun observations.
The all-effects stress replay reduces mean work by **8.4%** and worst observed
work by **7.7%**, also with zero reported repeats, misses and audio starvation
observations. Detailed profiling is a separate diagnostic build because it adds overhead.
Combined velocity/dye advection falls from about **3.453 ms to 2.777 ms** per
step; chained divergence/pressure/gradient falls from **3.148 ms to 3.053 ms**.
Projection and advection remain the largest measured simulation costs. Tracer
preparation/emission remains around 1.5 ms, followed by confinement around 0.7 ms.

Retained changes:

- `ADVECTION_PIPELINE=1` alternates vector register banks so interpolation of
  one eight-cell batch overlaps the next batch's scalar-addressed gathers.
  Four independent addresses hide scalar load latency. Signed interpolation,
  zero/unity decay, rounding and all source/output layouts remain unchanged.
- `GRADIENT_PIPELINE=1` loads the independent vertical pressure source bank
  during horizontal gradient arithmetic, preserving carry/control ordering
  and exact rounded division and clamping.
- `TRACE_ROWS=3` batches three backtrace rows per DMA setup, reducing trace
  transfers from 99 to 33 per backtrace command (two commands per physics
  update). Commands share scratch layouts because
  they execute serially and reload their inputs; preparation scratch shrinks
  from 2,384 to 1,904 bytes. Initial x coordinates are cached within the command.
  `TRACE_ROWS=1` remains available for comparison.
- `CPU_LTO=1` enables cross-file CPU optimisation with the existing floating
  point rules. Default-option changes now rebuild cached objects through their
  Makefile dependency; use separate build directories when changing overrides.
- Tails already within the 24-unit cap skip multiplies by one. The court clear
  touches only border strips because the fluid blit opaquely replaces the court.
  At 640 × 480 this removes **228,096 redundant clear pixels** per frame:
  79,104 instead of 307,200 (**74.25% less clear area**). This directly reduces
  framebuffer writes; its console timing benefit is not measured by Ares.

The 48 × 33 grid, eight pressure passes, 96 tracers, five tail layers, bilinear
fluid rendering, audio mixer and video refresh configuration are retained.
Exact dye/velocity/backtrace/gradient/projection/confinement fixtures pass,
including overlay switches, source integrity and DMA guards. The portable suite
passes, and 36 host UI rasters covering 12 scenes and three effects are
pixel-identical to the control. A separate 1,800-update 4P continuous-powers/tails
regression averages 12.395 ms with a 12.956 ms maximum, with no repeated fields
or observed audio underruns. The normal playable ROM is `plasmapong.z64`;
the new console frame rate has not yet been measured.

Reproduce the matched 2P replays:

```sh
just benchmark-hires-2p
python3 tools/benchmark-ares.py plasmapong-hires-2p.z64 build/hires-2p.log --windows 12 --memory-mib 4
just benchmark-hires-2p-effects
python3 tools/benchmark-ares.py plasmapong-hires-2p-effects.z64 build/hires-2p-effects.log --windows 24 --memory-mib 4 --timeout 360
```

Raw controls, candidate measurements, validation logs and the normal ROM build
log are retained locally in `build/hires2p/`. For the next console comparison,
select high res and 2P, reset the overlay with R after entering PLAY, then exercise
jets, suction and each flow effect. Compare both the presented FPS and `MISS`/
`GAP`; the refresh number after the slash should remain approximately 60.

### SummerCart64 hardware benchmarks

`just benchmark-hardware` builds a separate USB-logging ROM that boots into
the 640×480 particles menu with a fixed emitter seed, regardless of saved
options. Controller input remains available; leave the menu idle for matched
baseline runs. This build preserves the ordinary asynchronous renderer and
reports simulation, draw submission, frame intervals, VI presentation counters
and audio debt every 150 rendered frames. USB logging adds some overhead.

Load the ROM into cartridge RAM and start capture. Let `sc64deployer` determine
whether the current console state permits the upload; switch off only if it
reports that this is required:

```sh
just benchmark-hardware
python3 tools/benchmark-hardware.py plasmapong-hardware.z64 build/hardware/menu-01.log --upload
```

Turn on the N64 after the debugger says it is listening. Capture ends after ten
windows (about 36 seconds at 42 rendered FPS). The SD-card ROM and EEPROM file
are not overwritten; the benchmark uses the cartridge's EEPROM and normal
save behavior if you change options. Use a new log filename for each run.
`--port serial:///dev/ttyUSB0` selects a device explicitly; `--timeout 600`
allows extra time for power-on. To attach without uploading, omit `--upload`.
Capture saves the raw log and a JSON summary including ROM SHA-256, device
information and completion status, including partial results on timeout.
The deployer uses `--no-writeback`, so capture does not write a host save file.

For stage timings and completed-frame work quantiles, use a separate run:

```sh
just benchmark-hardware-profile
python3 tools/benchmark-hardware.py plasmapong-hardware-profile.z64 build/hardware/menu-profile-01.log --upload
```

Power off before each upload. The diagnostic build adds fluid timers and an
RSP/RDP completion fence; its FPS cannot be treated as the ordinary renderer's
baseline. Menu simulation is included in the fluid profile for these menu
benchmark builds. `render_fps` in JSON measures render-loop cadence; actual
displayed frames are recorded by the cumulative VI `presentation` counters,
which reset on scene/video changes or controller R. Keep modes and scenes
fixed during each capture. Audio time within fluid stages remains included.
Replay a saved capture without connecting hardware using `--replay`.
Default builds do not enable USB logging or force benchmark menu options.

For fluid-stage timings while retaining ordinary asynchronous rendering, use
`just benchmark-hardware-stages` and capture `plasmapong-hardware-stages.z64`.
Compare this run against the baseline before interpreting individual stage
costs: RDP activity can contend with simulation, and the completed-frame fence
changes that overlap. Chained operations report inclusive times: for example,
`pressure_solve` includes the queued divergence and gradient work in the default
projection chain; zero separate call counts do not mean those operations are
absent. Likewise, the confinement chain includes curl preparation.

First real-hardware baseline (2026-10-04): SummerCart64 firmware v2.20.2,
NTSC console with 8 MiB RDRAM, high-res particles menu, ordinary renderer,
USB logging enabled. Ten 150-render windows measured **44.181 rendered FPS**
(22.634 ms mean frame interval), **15.753 ms simulation per step** across
13 simulation windows, and **1.140 ms draw submission per render**. The final
VI counters recorded 1,499 new framebuffers over 2,035 fields, 536 repeated
fields and a maximum gap of two fields. All ten audio windows reported zero
underrun observations. Raw evidence is `build/hardware/menu-baseline-01.log`
and its `.json` summary. These are measured hardware results; draw submission
does not include all asynchronous RSP/RDP completion time. Stage diagnosis
requires the separate diagnostic run before selecting an optimisation.

The first completed-frame diagnostic (`build/hardware/menu-profile-01.log`)
measured **28.308 rendered FPS**, **11.254 ms simulation per step** and
**11.102 ms completed drawing per render** over ten frame windows. Inclusive
fluid stage averages were projection **3.307 ms**, dye advection **1.782 ms**,
splats **1.596 ms**, velocity advection **1.436 ms**, and confinement **0.842 ms**.
The fence changes overlap and this run had 663 audio underrun observations;
these timings are not a clean attribution of the ordinary renderer's cost.
A stage-only run is needed to compare simulation with the normal overlap.
The initial capture omitted the final flow/audio timer lines (nine windows
for those trailing metrics); the capture tool now waits for the final timer
line, including when USB packets arrive separately.

The ordinary-overlap stage run (`build/hardware/menu-stages-01.log` and `.json`)
completed all ten frame and trailing diagnostic windows. It measured
**41.817 rendered FPS**, **15.631 ms simulation per step** across 14 simulation
windows, and **1.208 ms draw submission per render**. Its first audio window
had two underrun observations; the remaining nine had zero.

| Inclusive stage | Ordinary overlap (ms/step) | Completed-frame fence (ms/step) |
| --- | ---: | ---: |
| Velocity advection | 5.642 | 1.436 |
| Pressure/projection | 3.314 | 3.307 |
| Dye advection | 1.786 | 1.782 |
| Splats | 1.720 | 1.596 |
| Curl/confinement | 0.842 | 0.842 |

The velocity timer covers trace/advection queue submission and syncpoint
completion, so it includes waiting behind previously queued work. Approximately
**4.206 ms/step** of the overlap-dependent difference appears in that first RSP
stage; later RSP stages are nearly unchanged. This points to render/queue
interaction as the next investigation, but does not distinguish RSP queue
blocking from RDRAM contention. The stage build's added timing/logging overhead
also changes cadence relative to the 44.181 FPS unprofiled baseline. Preserve
that baseline for optimisation comparisons rather than summing these inclusive
stage measurements into an assumed standalone execution cost.

Next diagnostic: `just benchmark-hardware-queue` builds
`plasmapong-hardware-queue.z64`. `QUEUE_PROFILE=1` inserts and times an RSP
syncpoint before each velocity-advection call, attributing earlier queued work
to `queue_wait`. It does not request full RDP completion. The extra syncpoint
changes scheduling, so compare total simulation time and queue-wait plus
advection against the ordinary stage build. A large queue wait and advection
near the fenced 1.436 ms would support queue backlog as the main contributor;
continued slow advection after the queue split would require investigating
concurrent RDP/VI bandwidth pressure. Keep normal rendering in the eventual
unprofiled candidate and validate displayed FPS, repeated fields, and audio on
hardware before retaining any optimisation.

The queue-split run (`build/hardware/menu-queue-01.log` and `.json`) measured
**4.221 ms/step queue wait** and **1.436 ms/step velocity advection**, with
projection **3.310 ms/step**. This attributes the ordinary-overlap slowdown to
earlier queued commands rather than a slower advection kernel in this test.
The extra diagnostic synchronization changed cadence to **40.373 rendered
FPS** and produced 35 audio underrun observations, so it remains diagnostic.

First candidate: `just benchmark-hardware-batch` builds
`plasmapong-hardware-batch.z64` with `FRAME_BLOCK=1` and no fluid profiling or
completion fence. It records texture drawing, particles, static geometry and
text together each frame, bypassing the separate per-element block caches.
Particle rectangles use RDPQ while recording (raw `rdpq_exec` cannot run inside
a block). Pixel generation stays outside recording, and previous-frame
completion still protects shared texture and block memory. The candidate aims
to reduce command-buffer transitions behind which simulation waits. It adds
CPU recording/allocation cost and requires hardware comparison before becoming
the default. Its high-res particles menu completed two Ares measurement windows
with RDP validation and no command errors; validation timing is diagnostic only.

Hardware rejected the frame-recording candidate: the ten-window
`build/hardware/menu-batch-01.log` run averaged **29.703 FPS**, **14.118 ms
simulation per step**, **5.049 ms draw submission per render**, and 586 audio
underrun observations. CPU recording cost exceeded the saved queue time.
`FRAME_BLOCK` remains an opt-in experiment and is not enabled by default.

Next candidate: `just benchmark-hardware-highpri` builds
`plasmapong-hardware-highpri.z64`, keeping cached drawing while placing
simulation's RSP commands in the high-priority queue (`FLUID_HIGHPRI=1`).
All simulation overlays are registered before entering that queue. The ordinary
pixel-producer guard still releases fluid source data before simulation.
High-priority batches cannot use ordinary syncpoints, so each existing DMA
completion point closes/synchronizes the high-priority batch before CPU reads,
then resumes its queue context. Boot fixtures and pixel production keep normal
queue waits. This changes scheduling and still requires hardware validation;
the RSP can switch priority only after its current command finishes.
Empty simulation batches (for example lobby steps) also enqueue a no-op before
closing, so the pinned library's checked write path handles buffer rollover
before its directly appended high-priority epilogue. The first stress run
caught this assertion; the corrected candidate completed two 4P high-res tails
stress windows with RDP validation, no queue assertions or RDP errors, and zero
audio underrun observations (`build/hardware/highpri-4p-validate-02.log`).

Hardware did not retain the high-priority candidate: ten menu windows measured
**43.704 FPS**, **12.448 ms simulation per step**, and **5.690 ms draw submission
including the previous-frame completion wait**, with zero audio underrun
observations (`build/hardware/menu-highpri-01.log` and `.json`). Simulation
finished earlier, but the additional wait moved into drawing and total
presentation did not beat the 44.181 FPS baseline. This historical
`FLUID_HIGHPRI=1 FLUID_HIGHPRI_YIELD=0` variant remains rejected; the yielding
variant validated later is now the default.

`just benchmark-hardware-banks` builds `plasmapong-hardware-banks.z64` to test
the pre-existing `EXPANSION_BANKS=1` allocation wrapper on the detected 8 MiB
console. It uses the ordinary renderer and simulation queue, without frame
recording or high-priority work. Each 640×480 framebuffer is aligned to a
distinct 1 MiB bank; startup logs record the allocation addresses. Compare
against the original menu baseline; no change in resolution or pixels is
intended.

The ten-window bank-placement run (`build/hardware/menu-banks-01.log` and
`.json`) measured **45.589 FPS**, a **3.188% increase** over 44.181 FPS.
Simulation remained **15.755 ms/step**, draw submission averaged **1.110 ms/frame**,
and all ten audio windows reported zero underrun observations. The framebuffers
were allocated at physical addresses 0x00100000, 0x00200000 and 0x00300000.
This is a modest hardware improvement, not a 60 FPS result; repeat and gameplay
validation remain necessary before enabling the placement policy by default.

`just benchmark-hardware-ink16` adds `INK16=1` to that bank-placement control.
It keeps the established RGBA32 pixel producer, waits for it, then packs its
48×33 output to RGBA5551 on the CPU and uploads a 16-bit texture. The actual
3168-byte texture fits within TMEM, unlike the 6336-byte RGBA32 source. This
experiment measures the upload/tiling benefit before investing in RSP packing.
Display resolution stays 640×480; colors are quantized before bilinear filtering,
so subtle gradient differences need visual review. It adds CPU conversion and
producer synchronization cost and stays opt-in pending hardware results.

The CPU-packed run (`build/hardware/menu-ink16-01.log` and `.json`) regressed
to **37.840 FPS**, with **15.189 ms simulation per step**, **2.250 ms draw
submission per render** and 53 audio underrun observations. This rejects CPU
conversion as an optimisation; it does not isolate the direct-packed texture's
benefit from conversion and synchronization costs.

`just benchmark-hardware-ink16-rsp` builds the direct-packing version. New
preparation-overlay commands generate RGBA5551 dye pixels directly and fetch
SPEED colors from a once-packed palette. The established RGBA32 commands remain
available. Both formats are checked against CPU references across 80 fields,
including source integrity, padded destination rows, command interleaving and
DMA guards. The candidate keeps asynchronous pixel production and the ordinary
source-ownership syncpoint, avoiding CPU conversion and its early completion
wait. Exact pixel/velocity/gradient/projection fixtures and two 4P high-res
SPEED stress windows with RDP validation passed in Ares
(`build/hardware/ink16-rsp-validate.log`). Hardware timing is still required.

Direct RSP packing measured **46.031 FPS** with zero audio underrun observations
over ten hardware windows (`build/hardware/menu-ink16-rsp-01.log` and `.json`).
Simulation averaged **15.787 ms/step** and draw submission **1.054 ms/frame**.
This is only **0.969%** faster than the 45.589 FPS bank-placement result; verify
repeatability and the color tradeoff before retaining it as a default.

`just benchmark-hardware-menu-stamps` tests `MENU_STAMPS=1` against the
bank-placement control, using the original RGBA32 texture. The 26 stationary
label emitters cache their splat/gold grid footprints and weights, rebuilding
when their positions change. Application retains emitter order, fixed-storage
rounding, zero-weight velocity/blue clamps and gold limits. A 26,000-case
differential test matches the original scalar operations exactly, including
cache hits/invalidation and both source banks (`tests/menu_stamp_test.c`, also
run by `tools/check.sh`). Gameplay emitters retain their existing path. This
was initially opt-in pending repeat measurements and gameplay validation;
those checks passed later, and menu source caching is now the default.

The cached-menu candidate measured **46.561 FPS**, simulation **15.751 ms/step**
and draw submission **1.089 ms/frame**, with zero audio underrun observations
over ten hardware windows (`build/hardware/menu-stamps-01.log` and `.json`).
That is 2.13% above bank placement alone and 5.39% above the original baseline.

`just benchmark-hardware-vi-point` adds `HIRES_VI_POINT=1` to that candidate.
It disables VI resampling only at 640x480; 320x240 retains resampling because
libdragon prohibits unfiltered NTSC 16-bit scanout at widths at or below 320.
RDP texture filtering, color precision and simulation remain unchanged. This
may sharpen the scanout image and needs a visual check as well as hardware
timing before deciding whether to retain it.

The VI point-sampling candidate measured **46.103 FPS**, simulation
**15.762 ms/step** and draw submission **1.087 ms/frame**, with zero audio
underrun observations (`build/hardware/menu-vi-point-01.log` and `.json`).
It did not improve on the 46.561 FPS cached-menu control; retain VI resampling.

The cached-menu repeat measured **46.578 FPS**, simulation **15.751 ms/step**
and draw submission **1.087 ms/frame**, with zero audio underrun observations
(`build/hardware/menu-stamps-02.log` and `.json`). The two runs differ by only
0.036%, supporting the observed menu gain. Four-player hardware validation is
still required before enabling the candidate by default.

The retained candidate's scripted four-player 640x480 particle-tail stress run
measured **51.174 FPS**, simulation **14.562 ms/step** and draw submission
**2.277 ms/frame**, with zero audio underrun observations over ten hardware
windows (`build/hardware/gameplay-candidate-01.log` and `.json`). The longest
presentation gap was two VI refreshes. Ares also completed two matching
four-player windows with 4 MiB RAM, confirming the framebuffer allocation
fallback works without an Expansion Pak (`build/hardware/gameplay-4mib-ares.log`).
This is stability evidence; a matched hardware control is needed to establish
the gameplay performance effect.

The matched four-player hardware control measured **50.477 FPS**, simulation
**14.558 ms/step** and draw submission **2.300 ms/frame**, with zero audio
underrun observations (`build/hardware/gameplay-baseline-01.log` and `.json`).
The candidate's 51.174 FPS is a 1.38% gain in this replay; menu caching does not
change the PLAY path, so the gameplay difference tests framebuffer placement.

`just benchmark-hardware-menu-labels` adds `MENU_LABEL_BLOCK=1`. Instead of
switching between cached title and individual shadow/text buffers, it records
the entire menu foreground once and replays it as one block. The cache key
covers selection, player count and connected-controller count; changing video
mode frees the cache. Drawing order and glyphs remain unchanged. This is a
separate experiment from recording every frame: steady-state frames do not
rebuild the block. Hardware measurement is required to judge the benefit.

The menu-foreground refactor preserves byte-identical host draw output across
15 scenes. Its validation ROM exercised low/high/low/high video changes through
Options, returned to the menu, selected four players and entered gameplay, with
RDP validation enabled and no reported warnings/errors
(`build/hardware/menu-labels-validate.log`).

The single menu-foreground buffer measured **47.142 FPS** with zero audio
underrun observations (`build/hardware/menu-labels-01.log` and `.json`). Draw
submission averaged **0.837 ms/frame**, down from 1.087 ms in the retained
control, while simulation averaged **15.939 ms/step**. It gains 1.21% over that
control, about 6.70% over the original baseline; this still misses 60 FPS.
`just benchmark-hardware-retained-stages` profiles this version, separating
earlier RSP queue waits from actual velocity advection without a full RDP fence.
As with the earlier queue diagnostic, the extra syncpoint can affect scheduling;
use it to attribute cost, not as the uninstrumented FPS result.

The retained stage ROM in Ares measured **10.918 ms/step**, with queue wait
**0.034 ms**, velocity advection **1.232 ms**, confinement **0.692 ms**,
projection **3.049 ms** and dye advection **1.532 ms**
(`build/hardware/menu-retained-stages-ares.log`). The earlier hardware queue
diagnostic waited 4.221 ms before advection; the retained hardware comparison
is pending because the first capture received no ROM output. Rendering lateness
also causes additional fixed 60 Hz simulation steps per render: the best menu
run averages 21.212 ms per rendered frame even though its individual simulation
steps average 15.939 ms. Do not add per-step timings to per-frame timings
without accounting for the number of simulation steps.

`just benchmark-hardware-flat-fluid` is a diagnostic, requiring `BENCH_MENU=1`.
It retains simulation, particle drawing and asynchronous texture generation,
but replaces the fluid texture blit with an opaque flat fill. Compare it with
the unprofiled menu-labels ROM to isolate the textured backdrop's RDP cost.
Its missing fluid backdrop is intentional and is not a proposed visual change.

An exact-ROM comparison of the original queue diagnostic now isolates the
dominant emulator/hardware difference (`build/hardware/menu-queue-ares-01.log`
versus `menu-queue-01.log`): Ares simulation **11.377 ms/step**, hardware
**15.630 ms/step**; Ares pre-advection queue wait **0.030 ms**, hardware
**4.221 ms**. The additional queue wait accounts for about 98.5% of the net
simulation-time difference in these profiled runs. Projection is 3.057 vs
3.310 ms, velocity advection 1.258 vs 1.436 ms, confinement 0.691 vs 0.844 ms,
and dye advection 1.586 vs 1.784 ms. This points to earlier queued rendering
blocking simulation as the main difference; the flat-backdrop diagnostic is
intended to distinguish the textured draw's cost. These instrumented runs are
attribution evidence, not the ordinary renderer's FPS measurements.

The fresh retained-stage hardware capture completed
(`build/hardware/menu-retained-stages-03.log` and `.json`): simulation
**15.890 ms/step**, pre-advection queue wait **4.977 ms**, velocity advection
**1.422 ms**, confinement **0.842 ms**, projection **3.310 ms**, dye advection
**1.766 ms**, splats **1.375 ms**, gold injection **0.236 ms** and particle
sampling **0.443 ms**. Its exact-ROM Ares comparison waits only 0.034 ms and
simulates in 10.918 ms. About 99.4% of the net simulation-time difference is
additional queue wait in this diagnostic. Two audio underrun observations
occurred in the first window, zero thereafter. The profile renders at 42.510
FPS; retain the ordinary unprofiled 47.142 FPS result as the candidate's
performance number, because the added syncpoint changes scheduling.

The flat-backdrop diagnostic reached **59.938 FPS on hardware**, simulation
**10.621 ms/step** and draw submission **0.787 ms/frame**, with zero audio
underrun observations (`build/hardware/menu-flat-fluid-01.log` and `.json`).
There was one startup repeat; the cumulative missed count stayed at one through
all ten windows. Retaining simulation, particles and texture generation while
replacing the textured draw restores the target cadence. This isolates the
textured backdrop and resulting queue backpressure rather than proving a
production fix; retain the fluid image in any final candidate.

`just benchmark-hardware-draw-stream` tests `DRAW_STREAM=1` with that image
retained. It generates texture blit and particle rectangle commands in the
ordinary stream, bypassing the cached texture block and raw particle buffer,
while retaining cached menu foreground commands. The aim is fewer RDP buffer
switches; repeated CPU command preparation is the tradeoff. Ares RDP validation
reported no warnings/errors (`build/hardware/menu-draw-stream-validate.log`).

Streaming alone measured **46.875 FPS**, simulation **15.781 ms/step** and
draw submission **1.033 ms/frame**, with zero audio underrun observations
(`build/hardware/menu-draw-stream-01.log` and `.json`). It did not beat the
47.142 FPS cached-background control. Buffer switches within the foreground
remain a possible source of backpressure.

`just benchmark-hardware-menu-buffer` adds `MENU_BUFFER_KIB=32` to streaming.
This isolated experiment includes the pinned libdragon internal header and
sets the menu block's initial allocation capacity after `rspq_block_begin`.
It changes neither global library defaults nor other blocks. The measured
buffer-chain count is logged whenever the foreground is rebuilt. Ares confirms
one RDP static buffer per foreground and reports no RDP warnings/errors
(`build/hardware/menu-buffer-validate.log`). This depends on the pinned library's
internal API; keep opt-in pending hardware benefit and broader validation.

The one-buffer foreground measured **46.894 FPS**, simulation **15.779 ms/step**
and draw submission **1.036 ms/frame**, with zero audio underrun observations
(`build/hardware/menu-buffer-01.log` and `.json`). Hardware confirms one buffer,
but this does not improve on the 47.142 FPS control. Do not retain the larger
allocation or streaming by default based on these results.

`just benchmark-hardware-tail-sync` restores the best cached renderer and tests
`RDP_TAIL_SYNC=1`. The pinned library normally schedules SYNC_FULL and then
queues detach target cleanup. Its hardware workaround blocks subsequent RDP
transfers until SYNC_FULL completes, which can stall the shared RSP command
stream. The opt-in attachment implementation queues cleanup first, then
SYNC_FULL and flushes; the display callback still runs only after drawing
completes. The other attachment operations are copied unchanged from pinned
libdragon e356bf3, with its license alongside `src/rdpq_tail_sync.c`. This
experiment relies on that library's internal header and needs hardware and
mode-switch/gameplay validation. Ares RDP validation reported no warnings or
errors (`build/hardware/tail-sync-validate.log`).

The reordered completion candidate measured **46.998 FPS**, simulation
**15.948 ms/step** and draw submission **0.827 ms/frame**, with zero audio
underrun observations (`build/hardware/menu-tail-sync-01.log` and `.json`).
It does not improve on the control, so keep this library override disabled.
The proposed completion barrier does not explain the measured stall by itself.

`just benchmark-hardware-queue-pc` now samples only SP halt
status, and RDP command-busy/DMA-busy/end-valid status while waiting at the
pre-advection syncpoint. Interrupts stay enabled during the 100-microsecond
interval between samples, and the wait is bounded to two seconds. Reports
contain status counts per 150 simulation steps; the capture JSON retains the
sampling mode and counts. The legacy recipe/flag name remains for compatibility.
This diagnostic intentionally changes polling and logging overhead.

The initial live-PC capture (`build/hardware/menu-queue-pc-01.log` and `.json`)
sampled 706,673 times. RDP command-busy was set in **99.914%** of samples,
DMA-busy in **99.879%**, and end-valid in **99.223%**; the RSP was already
halted in only **0.090%**. Its instruction addresses are unreliable because
PC reads require the RSP to be halted; do not map those hotspots to code.
The capture parser labels older unmarked PC reports as unreliable. Status flags
support RDP backpressure; the instrumented 40.514 FPS is not a candidate
performance result. The retained uninstrumented control remains 47.142 FPS.

The halted-PC ROM passed two Ares windows with zero audio underruns
(`build/hardware/menu-queue-pc-halt-ares.log`). Ares completed the queued work
before the polling loop, so it collected zero PC samples; halt/read/resume
was not exercised. On hardware it triggered libdragon's `Unexpected RDP
interrupt` assertion before any timing window (`menu-queue-pc-halt-01.log`).
The screenshot backtrace locates it in `queue_sample_wait`, with the RDP
full-sync signal absent in the interrupt handler. The exact hardware interaction
is unresolved. This diagnostic was rejected: the sampler no longer halts the
RSP or reads PC, and the recovery ROM uses the previously measured cached
renderer without queue instrumentation. Future instruction tracing must be
implemented inside the RSP queue rather than halting it externally.

The recovery control completed ten hardware windows at **47.195 FPS**, with
simulation **15.941 ms/step**, draw submission **0.835 ms/frame**, and zero
audio underrun observations (`build/hardware/menu-recovery-01.log` and `.json`).
It retains the fluid image, bank-aligned framebuffers, cached menu splats and
foreground. Removing the halted-PC sampler restored the prior behavior.

`just benchmark-hardware-rdp-trace` builds an isolated tracing toolchain image
through `tools/Dockerfile.trace`. Its asserted patch to the pinned
`rsp_queue.inc` adds two resident DMEM words after the internal command table,
outside the queue's C state and DMA input buffer. The RSP stores the caller
and requested wait mask on entering `RSPQ_RdpWait`, and clears the mask on
return. The original busy-poll loop is unchanged; the patch adds two entry
instructions and replaces the return delay-slot NOP with a store. All library
and game overlays are rebuilt against the same shared queue layout. Normal
builds keep the original toolchain image.

The build generates CPU marker addresses from the queue ELF and checks their
DMEM range/alignment. The CPU reads these words during the existing 100-us
status polling interval, without halting the RSP or masking interrupts.
Reports distinguish mask `0` (outside this wait) from `0x040` (command busy),
`0x100` (DMA busy), `0x200` (end valid), or combinations, and retain sampled
callers. These are sampled wait occupancy, not instruction cycle counts;
the caller is read separately and may race a transition. The matching queue
disassembly is retained at `build/hardware/rsp-queue-trace-disassembly.txt`.

The tracing ROM passed two Ares windows with zero audio underruns and no
assertions (`build/hardware/menu-rdp-trace-ares.log`). The modified RSP queue
executed rendering, but Ares again completed queued work before CPU polling,
so hardware is needed to collect nonzero wait samples.

The passive trace completed ten hardware windows without an assertion
(`build/hardware/menu-rdp-trace-01.log` and `.json`). Of 100,990 queue-wait
samples, **92,869 (91.959%)** were inside `RSPQ_RdpWait` with mask **0x200
(END_VALID)**; every recorded caller was **0x194**, the return address within
`RSPQCmd_RdpSetBuffer`. The remaining 8,121 samples had marker zero. No nonzero
mask included command-busy or DMA-busy. Thus the observed wait is submission
backpressure while switching command buffers, not that routine's explicit
wait for DMA completion or pending full-sync completion. It does not prove
that buffer switches alone cause the full rendering cost. The diagnostic
averaged **4.958 ms/step** queue wait and **42.326 FPS**, with two startup audio
underrun observations and zero thereafter; use the 47.195 FPS recovery control
for performance comparison.

`just benchmark-hardware-menu-template` tests `MENU_TEMPLATE=1`, a benchmark-only
candidate that records the fluid blit, particle fill mode, and menu foreground
into one reusable block with a 32-KiB initial RDP allocation. It reserves 520
raw particle command slots aligned to complete 16-byte CPU cache lines.
Each frame rebuilds the existing particle commands and copies them into those
slots, padding the unused tail with RDP NOPs; the original previous-frame
completion wait protects reuse. Title/labels rebuild on selection, player
count, connected-controller count or flow-effect changes, and video changes
release the template. The live performance overlay remains outside the block.
Fluid RGBA32 generation, bilinear sampling, coordinates, particle colors/order
and simulation are retained. This avoids the rejected per-frame recording
cost of `FRAME_BLOCK=1`, while depending on the pinned RDP block internal API.

Active-template RDP validation passed two Ares windows with one RDP buffer
and no warnings, assertions or audio underruns
(`build/hardware/menu-template-validate-ares-02.log`). The earlier unsuffixed
validation log exercised the fallback because the initial guard excluded the
visible performance overlay; it is not template validation evidence.

The template capture listener expired before boot (`menu-template-01.log`);
reconnecting without re-upload captured ten complete windows from the running
ROM (`build/hardware/menu-template-02.log` and `.json`). It measured **44.998
FPS**, simulation **15.843 ms/step**, draw submission **0.970 ms/frame**, and
zero audio underrun observations. Audio debt remained roughly 2,200 samples.
The cumulative seven-VI maximum gap predates this reconnected capture and
cannot be attributed to a steady captured frame. This does not beat the
47.195 FPS recovery control; keep the template disabled.

`just benchmark-hardware-yield` tests `FLUID_HIGHPRI_YIELD=1` alongside
`FLUID_HIGHPRI=1` on the retained cached renderer. The earlier high-priority
experiment reopened its queue immediately after every completed batch and
kept it open throughout CPU-only simulation. The pinned library requests RSP
high priority at `rspq_highpri_begin`; an empty open batch prevents normal
queued rendering from progressing until its epilogue is submitted. That can
defer drawing instead of overlapping it with CPU work.

The yielding candidate opens high priority immediately before enqueueing the
first RSP job in a batch and closes/synchronizes at the existing output wait,
without reopening. Chained jobs share a batch. CPU cache maintenance before
the first job, splats and other CPU work leave normal rendering eligible.
Step completion retires a remaining open batch; paused/lobby steps with no
RSP work open none. Texture production after simulation remains on the
ordinary queue. This changes scheduling rather than simulation or rendering
content. Native high priority still switches only at command boundaries; it
does not interrupt an RDP buffer wait already executing.

Two Ares RDP-validation windows passed with no warnings/assertions or audio
underruns (`build/hardware/menu-yield-validate-ares.log`). The unvalidated
candidate held the expected 60-FPS cadence with zero missed fields and zero
audio underruns over two windows (`build/hardware/menu-yield-ares.log`), at
**10.164 ms/step** simulation and **0.720 ms/frame** draw submission. Hardware
benefit remains to be measured.

The yielding candidate reached **59.941 FPS on real NTSC hardware** across
ten complete windows (`build/hardware/menu-yield-01.log` and `.json`), versus
47.195 FPS for the recovery control. Simulation fell to **10.467 ms/step**
from 15.941 ms/step, and draw submission was **0.782 ms/frame**. All ten
windows contained exactly 150 steps per 150 frames. Audio underrun observations
and sample debt stayed zero. There was one startup repeated field; cumulative
misses remained one through the final 1,498 new frames / 1,499 VI. The full
RGBA32 bilinear fluid image and particles are retained. This demonstrates the
menu target cadence after changing queue scheduling; broader hardware gameplay
still needs the matched capture below before treating it as a general fix.

`just benchmark-hardware-yield-gameplay` applies the same scheduling to the
existing deterministic four-player high-resolution tails/stress scenario,
retaining bank placement and menu source stamps. Its matched prior hardware
control measured 51.174 FPS (`gameplay-candidate-01.log`). The host suite
(`./tools/check.sh`) passed after the scheduling change. Four-player Ares
RDP-validation runs passed two windows with 8 MiB and 4 MiB RDRAM, zero
warnings/assertions, zero missed fields in PLAY, and zero audio underruns
(`build/hardware/gameplay-yield-validate-ares.log` and
`build/hardware/gameplay-yield-4m-validate-ares.log`). The 4-MiB run confirms
the non-bank-aligned allocation fallback. Validation adds CPU overhead; its
draw timing is not the uninstrumented candidate's hardware timing.

The matched four-player high-resolution tails/stress hardware capture reached
**59.942 FPS** across ten render/presentation/audio windows
(`build/hardware/gameplay-yield-01.log` and `.json`), versus 51.174 FPS for the
prior bank-aligned control. Every window contained 150 simulation steps per
150 rendered frames. PLAY presentation reached **1,490 new frames / 1,490 VI**,
with zero repeats, zero misses and a maximum one-VI gap throughout. All audio
windows reported zero underruns and zero sample debt. Simulation averaged
**10.000 ms/step** across nine reported timing windows (the scene timing window
starts later than the render counter); draw submission averaged **2.287 ms/frame**.

The normal playable build now enables yielding high-priority scheduling for
the complete RSP backend, bank-aligned high-resolution framebuffers when an
Expansion Pak is available, exact menu source caching on the fixed backend,
and cached menu foreground commands. Queue-wait profiling defaults to the
ordinary queue, and CPU/comparison backends disable high priority automatically.
`FLUID_HIGHPRI=0`, `EXPANSION_BANKS=0`, `MENU_STAMPS=0` and
`MENU_LABEL_BLOCK=0` independently disable the retained changes. Archived
hardware benchmark recipes specify the historical settings explicitly so
promoting defaults does not silently change their controls. The rejected
frame recording/template, streaming, 16-bit ink, VI point-sampling and library
tail-sync overrides remain disabled.

The playable `plasmapong.z64` was built with the new defaults and USB logging
for manual console play. Default resolution switching passed low/high/low/high
through Options with RDP validation on both 8 MiB and 4 MiB systems
(`build/hardware/default-video-8m-validate-ares.log` and
`build/hardware/default-video-4m-validate-ares.log`), without warnings/assertions
or audio underrun observations. Mode transitions deliberately interrupt cadence;
these are safety/behavior checks rather than steady hardware performance tests.
The CPU comparison ROM also builds with the new conditional defaults.
