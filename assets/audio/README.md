# Sound sources and processing

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

Retrieved 2026-10-02 from Kenney's own downloads. Packs are Creative Commons Zero
(CC0 1.0): see `LICENSE-impact.txt`, `LICENSE-digital.txt`, and `LICENSE-scifi.txt`.

`python3 tools/prepare-audio.py` (run from the repository) regenerates the mono
16 kHz WAV auditions here and `src/sound_bank.inc`. It requires ffmpeg and Python,
uses no network, and performs trimming, DC removal, peak normalization, filtering,
loop crossfades, onset/tail fades, and a short delayed repeat for the victory cue.
The checked-in PCM bank is about 171 KB; ordinary ROM builds need no audio tools.

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
12s victory, 14s menu confirm, 15s menu back. Source/audition WAVs are normalized assets; the mix demo reflects the
much quieter in-game levels of the sustained effects.

For historical context, Graeme Norgate describes trimming effects and reducing
sample rates for GoldenEye's cartridge budget in this
[2010 interview](https://designingsound.org/2010/11/03/from-n64-to-wii-re-imagining-goldeneye-007-exclusive-interview-with-graeme-norgate-and-steve-duckworth/).
That informed the treatment here; it does not establish that all GoldenEye sounds
were stock samples.
