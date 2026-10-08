#!/usr/bin/env python3
"""Convert supplied MP3 loops to cartridge-native stereo PCM (requires ffmpeg).

Normal ROM builds copy the checked-in results and do not require ffmpeg or MP3s.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
DEST = ROOT / 'assets/audio/music'
DEST.mkdir(parents=True, exist_ok=True)
for name in ('intro_loop', 'battle_loop'):
    # Preserve full duration/stereo; small edge fades avoid a click at the wrap.
    raw = subprocess.check_output([
        'ffmpeg', '-v', 'error', '-i', str(ROOT / f'{name}.mp3'),
        '-ac', '2', '-ar', '16000', '-af',
        'afade=t=in:d=0.005,areverse,afade=t=in:d=0.005,areverse',
        '-f', 's16be', '-'])
    if not raw or len(raw) % 4:
        raise ValueError(f'{name}: invalid stereo PCM')
    (DEST / f'{name}.pcm').write_bytes(raw)
    print(f'{name}: {len(raw)//4} frames, {len(raw)/64000:.3f}s, {len(raw)} bytes')
