# Sound sources and processing

## Background music

`music/intro_loop.pcm` and `music/battle_loop.pcm` are the user-supplied music
tracks, converted from MP3 to raw big-endian signed 16-bit stereo at 16 kHz.
They retain the full decoded tracks (154.723 s and 220.291 s), with 5 ms edge
fades to avoid clicks at the repeat boundary. These are separate from the CC0
sound effects below; no third-party license is asserted for the supplied music.

`python3 tools/prepare-music.py` regenerates them when the original MP3 files
are supplied in the repository root. The MP3s are no longer needed after
conversion. Normal builds copy the checked-in PCM into DragonFS without any
audio conversion tools. Together the tracks use 24,000,888 cartridge bytes;
playback uses one 4 KiB RAM buffer and CPU mixing, leaving the RSP for gameplay.

The intro loops across Menu, Options and High Scores. Battle loops across Lobby,
Play (including serve countdowns and arcade level cards), Pause and Finished.
Changing between these groups fades the old track out and starts the new track
from the beginning with a short fade-in. Music plays at one-quarter PCM gain
under the existing sound effects, with saturation on the combined stereo output.
Playback follows the actual audio hardware sample rate using a fractional cursor.

Integration checks (2026-10-08): portable tests and Ares RDP validation pass with
4/8 MiB RDRAM. Ordinary Ares menu/game captures measured 11.241/7.413 ms per
simulation update, with 1/0 presentation misses and zero audio underrun
observations across two timing windows each. The slower menu validation capture
also has zero audio underruns after replacing the per-frame 384-sample generation
cap with the four-buffer capacity. These are emulator diagnostics, not hardware
performance measurements. Retained evidence is under
`build/dev-loop/music-integration-{menu,play}-02/`. Console checks were unavailable
because `/dev/ttyUSB0` was absent in this session.

## Sound effects

All six source recordings are from Kenney's CC0 stock libraries. These are modern
stock assets, edited for a compact, low-bandwidth cartridge-era sound; they are
not extracted from GoldenEye 007 and are not claimed to be its original samples.
The original OGG files and each pack's license are included here.

| Game effect | Original sample | Pack |
| --- | --- | --- |
| Ball / bat | `impactGeneric_light_000.ogg` | [Impact Sounds](https://kenney.nl/assets/impact-sounds) |
| Ball / boundary | `impactMetal_light_002.ogg` | Impact Sounds |
| Goal | `lowDown.ogg` | [Digital Audio](https://kenney.nl/assets/digital-audio) |
| Victory | `threeTone2.ogg` | Digital Audio |
| Suction attack / texture | `forceField_000.ogg` | [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) |
| Menu confirm / back | `threeTone2.ogg` / `lowDown.ogg` | Digital Audio |
| Jet | `thrusterFire_000.ogg` | Sci-fi Sounds |
| Suction break | `impactMetal_light_002.ogg` / `lowDown.ogg` | Impact Sounds / Digital Audio |

Retrieved 2026-10-02 from Kenney's own downloads. Packs are Creative Commons Zero
(CC0 1.0): see `LICENSE-impact.txt`, `LICENSE-digital.txt`, and `LICENSE-scifi.txt`.

`python3 tools/prepare-audio.py` (run from the repository) regenerates the mono
16 kHz WAV auditions here and `src/sound_bank.inc`. It requires ffmpeg and Python,
uses no network, and performs trimming, DC removal, peak normalization, filtering,
loop crossfades, onset/tail fades, and a short delayed repeat for the victory cue.
The checked-in PCM bank is about 180 KiB; ordinary ROM builds need no audio tools.
The break cue combines a metallic crack with two fading, lower-pitched sputters,
panned toward the affected bat.

Suction plays a one-time force-field attack, morphs for 1.2 seconds into a quiet
synthesized hum with a filtered stock texture, and loops only that steady sustain.
A fresh press restarts the attack; holding never does.

The runtime integer mixer supplies stereo 16-bit audio with four transient voices
and four quiet, independently panned continuous voices (suction and jet for each
player). Both players can use both powers simultaneously. Loops ramp up/down over
about 25 ms; they fade out on release, pause, disconnect, goal-to-win, or menu
entry. Ordinary goals retain active powers during the next serve countdown.
Victory has a short lead-in so the final goal can be heard first.

`just check` creates `build/sound-demo.wav` using the actual runtime mixer:
0s left bat, 1s right bat, 2s boundary, 3s goal, 4–8s suction, 9–11s jet,
12s victory, 14s menu confirm, 15s menu back, 16s left suction break, 17s right
suction break. Source/audition WAVs are normalized assets; the mix demo reflects the
much quieter in-game levels of the sustained effects.

For historical context, Graeme Norgate describes trimming effects and reducing
sample rates for GoldenEye's cartridge budget in this
[2010 interview](https://designingsound.org/2010/11/03/from-n64-to-wii-re-imagining-goldeneye-007-exclusive-interview-with-graeme-norgate-and-steve-duckworth/).
That informed the treatment here; it does not establish that all GoldenEye sounds
were stock samples.
