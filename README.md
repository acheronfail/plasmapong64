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
| Release A | Burst fluid outwards and launch a caught ball; holding A for up to one second charges the release |
| START | Confirm menu selection, start, pause/resume, or multiplayer rematch |
| Up/down, then A on the main menu | Choose a mode or view high scores |
| B in the lobby, pause, or winner screen | Return to the main menu |

Suction reaches about 35 arena pixels; actual capture requires the ball to be
within 18 pixels on the playing side of the bat. Fast shots and strong currents
can beat a grab. You can move while holding the ball and aim a release with the
bat's vertical motion. Z and A can be used together. Unplugging a required
controller pauses the match; reconnect it and press START to resume. Arcade mode
ignores controller 2 during a run.

## Endless arcade

Start with **3 lives**. Score **3 goals** to advance a level; conceding costs one
life without removing your goals. Clear every fifth level for an extra life,
up to five. Levels continue until you run out of lives. Transitions freeze the
playfield briefly, followed by the usual serve countdown.

Each goal earns **100 × level**, each level clear earns **500 × level**, and
clearing in under 60 seconds of active play adds up to **600 points** (10 per
remaining second). Pauses, serves and transitions do not consume that bonus time.
Holding or returning the ball gives no points. A caught ball automatically
releases after two seconds; release A before catching again.

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
just build     # produces plasmapong.z64
just check     # portable physics tests and SVG previews in build/
just deploy    # build, then upload to /CUSTOM/plasmapong.z64
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

- `src/fluid.c`: 48 × 30 Eulerian grid covering a 288 × 180 arena. Velocity and
  three dye concentrations use bilinear semi-Lagrangian advection. An 8-iteration
  Gauss–Seidel pressure solve reduces divergence; curl confinement preserves
  small swirls. Boundary cells enforce zero wall-normal velocity. Momentum and
  dye decay gradually so old currents dissipate.
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
  synchronized before reusing texture memory. Game state occupies about 64 KB;
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
fluid correctly. After the fluid optimizations, its measured simulation cost is
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
separate testing artifact: `just build` and `just deploy` always use the normal
human-controlled `plasmapong.z64`. `just smoke-arcade` builds
`plasmapong-arcade-smoke.z64`, which navigates to single player and exercises the
real AI using one scripted human controller. Host previews also include
`preview-{arcade,gameover,scores,level}.svg`. `just smoke-save` builds a separate
EEPROM fixture ROM: the first boot saves an ACE score, and a second boot asserts
that it survived. This fixture never runs in the playable ROM.

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
