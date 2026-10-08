#!/usr/bin/env python3
"""Run a scripted ROM until complete emulated-time measurement windows exist."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import selectors
import shutil
import subprocess
import struct
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('rom', type=pathlib.Path)
parser.add_argument('log', type=pathlib.Path)
parser.add_argument('--ares', default='../ares/build/rundir/bin/ares')
parser.add_argument('--windows', type=int, default=10)
parser.add_argument('--timeout', type=float, default=240)
parser.add_argument('--screenshot', type=pathlib.Path, help='capture the Ares rendering window (X11/ImageMagick)')
parser.add_argument('--screenshot-window', type=int, default=2, help='timing window to capture (PLAY, or draw with --draw-only), default 2')
parser.add_argument('--native-screenshot', type=pathlib.Path, help='capture through the native Ares adapter; no desktop screenshot/input API')
parser.add_argument('--native-frame', type=int, default=300, help='video callback to capture with the native adapter')
parser.add_argument('--draw-only', action='store_true', help='measure menus without waiting for PLAY simulation logs')
parser.add_argument('--memory-mib', type=int, choices=(4, 8), help='test with or without the Expansion Pak; changes only this run')
args = parser.parse_args()
if args.windows < 1 or args.timeout <= 0 or args.screenshot_window < 1:
    parser.error('windows, screenshot-window and timeout must be positive')
if args.screenshot and args.screenshot_window > args.windows:
    parser.error('screenshot-window must not exceed windows')
if args.native_screenshot and (args.screenshot or args.native_frame<1):
    parser.error('native capture needs a positive frame and cannot be combined with --screenshot')
if args.native_screenshot and args.native_screenshot.exists():
    parser.error('native capture already exists; choose a new file')
if not args.rom.is_file():
    parser.error(f'ROM does not exist: {args.rom}')
args.log.parent.mkdir(parents=True, exist_ok=True)
settings = args.log.with_suffix('.settings.bml')
user_settings = pathlib.Path.home() / '.local/share/ares/settings.bml'
if not settings.exists() and user_settings.exists():
    shutil.copyfile(user_settings, settings)
command = [args.ares, '--settings-file', str(settings.resolve()), '--setting', 'Developer/HomebrewMode=true', '--setting', 'Input/Defocus=Block', '--system',
           'Nintendo 64', '--no-file-prompt', str(args.rom.resolve())]
if args.memory_mib is not None:
    command[1:1] = ['--setting', f'Nintendo64/ExpansionPak={"true" if args.memory_mib == 8 else "false"}']
measurements = {}
presentation = []
audio_stream = []
start = time.monotonic()
with args.log.open('w') as log:
    environment = os.environ.copy()
    if args.screenshot:
        environment['GDK_BACKEND'] = 'x11'
    if args.native_screenshot:
        args.native_screenshot.parent.mkdir(parents=True, exist_ok=True)
        environment['ARES_CAPTURE_FRAME'] = str(args.native_frame)
        environment['ARES_CAPTURE_PATH'] = str(args.native_screenshot.resolve())
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment)
    captured = False
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    pending = b''
    try:
        while time.monotonic() - start < args.timeout:
            for key, _ in selector.select(1):
                chunk = key.fileobj.read1(65536)
                if not chunk:
                    raise RuntimeError(f'ares exited early ({process.poll()})')
                pending += chunk
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    line = raw.decode(errors='replace')
                    log.write(line + '\n')
                    log.flush()
                    match = re.search(r'(simulation average|draw average|frame interval) (\d+) us', line)
                    if not match:
                        match = re.search(r'Fluid profile: (\w+) (\d+) us', line)
                    if match:
                        measurements.setdefault(match[1], []).append(int(match[2]))
                    draw = re.search(r'Draw profile: pixels (\d+) us, texture (\d+) us, wait (\d+) us, other (\d+) us/frame', line)
                    if draw:
                        for name, value in zip(('draw_pixels', 'draw_texture', 'draw_wait', 'draw_other'), draw.groups()):
                            measurements.setdefault(name, []).append(int(value))
                    flow = re.search(r'Flow draw: prepare (\d+) us, emit (\d+) us/frame', line)
                    if flow:
                        for name, value in zip(('flow_prepare', 'flow_emit'), flow.groups()):
                            measurements.setdefault(name, []).append(int(value))
                    shown = re.search(r'Presentation: (\d+) new / (\d+) VI, (\d+) repeats, longest (\d+) VI gap', line)
                    if shown:
                        presentation.append(dict(zip(('frames', 'refreshes', 'repeats', 'longest_gap_vi'), map(int, shown.groups()))))
                        target = re.search(r'\((\d+) missed, target (\d+) FPS\)', line)
                        if target:
                            presentation[-1].update(misses=int(target[1]), target_fps=int(target[2]))
                    if 'PASS:' in line:
                        print(line, flush=True)
                    audio = re.search(r'Audio stream: (\d+) samples, (\d+) underrun observations, (\d+) samples owed', line)
                    if audio:
                        audio_stream.append(dict(zip(('samples', 'underrun_observations', 'samples_owed'), map(int, audio.groups()))))
                    work = re.search(r'Frame (work|active|busy): average (\d+) us, p50 <= (\d+) us, p95 <= (\d+) us, p99 <= (\d+) us, max (\d+) us', line)
                    if work:
                        for name, value in zip(('average', 'p50_bound', 'p95_bound', 'p99_bound', 'max'), work.groups()[1:]):
                            measurements.setdefault(work[1]+'_'+name, []).append(int(value))
                    buffer_wait = re.search(r'Buffer wait: average (\d+) us, max (\d+) us', line)
                    if buffer_wait:
                        for name,value in zip(('buffer_wait_average','buffer_wait_max'),buffer_wait.groups()):
                            measurements.setdefault(name, []).append(int(value))
                    if re.search(r'assertion failed|mismatch trial|RDPQ.*(?:ERROR|WARNING)|missing cache (?:invalidation|writeback)|DMA.*(?:cached|dirty)', line, re.I):
                        raise RuntimeError(line)
            if args.screenshot and not captured and len(measurements.get('draw average' if args.draw_only else 'simulation average', [])) >= args.screenshot_window:
                tree = subprocess.check_output(['xwininfo', '-root', '-tree'], text=True)
                window = re.search(r'(0x[0-9a-f]+) "' + re.escape(args.rom.stem) + r'":', tree)
                if not window:
                    raise RuntimeError('Cannot find the Ares game window for screenshot')
                args.screenshot.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(['import', '-window', window[1], str(args.screenshot)], check=True)
                captured = True
            required = ['draw average', 'frame interval']
            if not args.draw_only:
                required.append('simulation average')
            if 'velocity_advection' in measurements and not args.draw_only:
                required.append('sample')
                required.append('flow_emit')
            if 'work_average' in measurements:
                required.append('work_average')
            if 'active_average' in measurements:
                required.extend(('active_average', 'buffer_wait_average'))
            if 'busy_average' in measurements:
                required.append('busy_average')
            if (all(len(measurements.get(k, [])) >= args.windows for k in required)
                    and (not presentation or len(presentation) >= args.windows)
                    and (not audio_stream or len(audio_stream) >= args.windows)):
                if args.screenshot and not captured:
                    raise RuntimeError('Requested screenshot window exceeds completed PLAY windows')
                if args.native_screenshot and not args.native_screenshot.exists():
                    # Native fields need not align with logging windows; keep
                    # running the same confirmed-live emulator until capture.
                    continue
                break
        else:
            raise RuntimeError('Timed out before the requested complete measurement windows')
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        selector.close()
result = {key: {'mean_us': sum(values[:args.windows])/len(values[:args.windows]),
                'min_us': min(values[:args.windows]), 'max_us': max(values[:args.windows]),
                'windows': len(values[:args.windows])} for key, values in measurements.items()}
if args.native_screenshot:
    data = args.native_screenshot.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise RuntimeError('Native capture did not produce a PNG')
    width,height = struct.unpack('>II',data[16:24])
    result['native_capture'] = dict(path=str(args.native_screenshot.resolve()),field=args.native_frame,
                                  width=width,height=height,sha256=hashlib.sha256(data).hexdigest())
if presentation:
    # Counters restart on scene changes or the console's R reset button.
    result['presentation'] = presentation
if audio_stream:
    result['audio_stream'] = audio_stream
args.log.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
