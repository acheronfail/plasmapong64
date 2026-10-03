#!/usr/bin/env python3
"""Run a scripted ROM until complete emulated-time measurement windows exist."""
import argparse
import json
import pathlib
import re
import selectors
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('rom', type=pathlib.Path)
parser.add_argument('log', type=pathlib.Path)
parser.add_argument('--ares', default='../ares/build/rundir/bin/ares')
parser.add_argument('--windows', type=int, default=10)
parser.add_argument('--timeout', type=float, default=240)
parser.add_argument('--draw-only', action='store_true', help='measure menus without waiting for PLAY simulation logs')
parser.add_argument('--memory-mib', type=int, choices=(4, 8), help='test with or without the Expansion Pak; changes only this run')
args = parser.parse_args()
if args.windows < 1 or args.timeout <= 0:
    parser.error('windows and timeout must be positive')
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
start = time.monotonic()
with args.log.open('w') as log:
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
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
                    work = re.search(r'Frame work: average (\d+) us, p50 <= (\d+) us, p95 <= (\d+) us, p99 <= (\d+) us, max (\d+) us', line)
                    if work:
                        for name, value in zip(('work_average', 'work_p50_bound', 'work_p95_bound', 'work_p99_bound', 'work_max'), work.groups()):
                            measurements.setdefault(name, []).append(int(value))
                    if re.search(r'assertion failed|mismatch trial|RDPQ.*(?:ERROR|WARNING)|missing cache (?:invalidation|writeback)|DMA.*(?:cached|dirty)', line, re.I):
                        raise RuntimeError(line)
            required = ['draw average', 'frame interval']
            if not args.draw_only:
                required.append('simulation average')
            if 'velocity_advection' in measurements and not args.draw_only:
                required.append('sample')
                required.append('flow_emit')
            if 'work_average' in measurements:
                required.append('work_average')
            if (all(len(measurements.get(k, [])) >= args.windows for k in required)
                    and (not presentation or len(presentation) >= args.windows)):
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
if presentation:
    # Counters restart on scene changes or the console's R reset button.
    result['presentation'] = presentation
args.log.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
