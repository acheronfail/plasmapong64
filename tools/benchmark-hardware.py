#!/usr/bin/env python3
"""Capture SummerCart64 USB timings; optionally upload a ROM before power-on."""
import argparse
import errno
import hashlib
import json
import os
import pathlib
import pty
import re
import selectors
import subprocess
import time
from n64_power import PowerSwitch, configured_power_url


def summarize(lines):
    measurements, presentation, audio, queue_pc = {}, [], [], []
    queue_pc_mode = 'live reads (unreliable PC)'
    for line in lines:
        if 'Queue PC sampler: halted reads, period 100 us' in line:
            queue_pc_mode = 'halted reads, period 100 us'
        if 'Queue PC sampler: status only, period 100 us' in line:
            queue_pc_mode = 'status only, period 100 us'
        match = re.search(r'Queue PC: total (\d+), halted (\d+), DP busy (\d+), DMA busy (\d+), end valid (\d+)', line)
        if match:
            queue_pc.append(dict(zip(('samples','halted','dp_busy','dma_busy','end_valid'), map(int,match.groups()))))
            queue_pc[-1]['hotspots']=[]
            queue_pc[-1]['mode']=queue_pc_mode
        match = re.search(r'RSP wait (mask|caller): (0x[0-9a-f]+) samples (\d+)', line)
        if match and queue_pc:
            queue_pc[-1].setdefault('wait_' + match[1], []).append(
                dict(value=int(match[2],16),samples=int(match[3])))
        match = re.search(r'Queue PC hot: pc (0x[0-9a-f]+) samples (\d+)', line)
        if match and queue_pc:
            queue_pc[-1]['hotspots'].append(dict(pc=int(match[1],16),samples=int(match[2])))
        match = re.search(r'(simulation average|draw average|frame interval) (\d+) us', line)
        if not match:
            match = re.search(r'Fluid profile: (\w+) (\d+) us', line)
        if match:
            measurements.setdefault(match[1], []).append(int(match[2]))
        work = re.search(r'Frame (work|active): average (\d+) us, p50 <= (\d+) us, p95 <= (\d+) us, p99 <= (\d+) us, max (\d+) us', line)
        if work:
            for name, value in zip(('average', 'p50_bound', 'p95_bound', 'p99_bound', 'max'), work.groups()[1:]):
                measurements.setdefault(work[1] + '_' + name, []).append(int(value))
        for pattern, names in (
            (r'Draw profile: pixels (\d+) us, texture (\d+) us, wait (\d+) us, other (\d+) us/frame', ('draw_pixels', 'draw_texture', 'draw_wait', 'draw_other')),
            (r'Flow draw: prepare (\d+) us, emit (\d+) us/frame', ('flow_prepare', 'flow_emit')),
            (r'Buffer wait: average (\d+) us, max (\d+) us', ('buffer_wait_average', 'buffer_wait_max')),
        ):
            match = re.search(pattern, line)
            if match:
                for name, value in zip(names, match.groups()):
                    measurements.setdefault(name, []).append(int(value))
        match = re.search(r'Presentation: (\d+) new / (\d+) VI, (\d+) repeats, longest (\d+) VI gap \((\d+) missed, target (\d+) FPS\)', line)
        if match:
            presentation.append(dict(zip(('frames', 'refreshes', 'repeats', 'longest_gap_vi', 'misses', 'target_fps'), map(int, match.groups()))))
        match = re.search(r'Audio stream: (\d+) samples, (\d+) underrun observations, (\d+) samples owed', line)
        if match:
            audio.append(dict(zip(('samples', 'underrun_observations', 'samples_owed'), map(int, match.groups()))))
    result = {key: dict(mean_us=sum(values)/len(values), min_us=min(values), max_us=max(values), windows=len(values)) for key, values in measurements.items()}
    if 'frame interval' in result:
        result['render_fps'] = 1_000_000 / result['frame interval']['mean_us']
    # Presentation counters are cumulative since scene/mode changes or R reset.
    # Preserve them verbatim; render_fps measures submissions, not VI swaps.
    result.update(presentation=presentation, audio_stream=audio)
    if queue_pc:
        result['queue_pc']=queue_pc
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=pathlib.Path)
    parser.add_argument('log', type=pathlib.Path)
    parser.add_argument('--port', help='SC64 port, e.g. serial:///dev/ttyUSB0')
    parser.add_argument('--upload', action='store_true', help='upload to cartridge RAM; deployer checks console state')
    parser.add_argument('--power-url', help='ESPHome outlet URL: power off, upload, listen, power on, capture, power off')
    parser.add_argument('--power-control', action='store_true', help='automatically control outlet using N64_POWER_URL from environment or .env')
    parser.add_argument('--power-entity', default='switch')
    parser.add_argument('--leave-on', action='store_true', help='leave console on after a successful automatic capture')
    parser.add_argument('--windows', type=int, default=10)
    parser.add_argument('--timeout', type=float, default=300, help='includes waiting for power-on')
    parser.add_argument('--replay', action='store_true', help='summarize an existing capture without hardware')
    args = parser.parse_args()
    if args.power_control and not args.power_url:
        args.power_url = configured_power_url()
        if not args.power_url:
            parser.error('set N64_POWER_URL in .env or the environment, or provide --power-url')
    if args.windows < 1 or args.timeout <= 0 or not args.rom.is_file():
        parser.error('provide an existing ROM and positive windows/timeout')
    if args.power_url and (not args.upload or args.replay):
        parser.error('--power-url requires --upload and cannot be used with --replay')
    if args.leave_on and not args.power_url:
        parser.error('--leave-on requires --power-url')
    args.log.parent.mkdir(parents=True, exist_ok=True)
    command = ['sc64deployer'] + (['--port', args.port] if args.port else [])
    metadata = dict(rom=str(args.rom.resolve()), sha256=hashlib.sha256(args.rom.read_bytes()).hexdigest(), requested_windows=args.windows)
    lines = []
    error = None
    if args.replay:
        lines = args.log.read_text().splitlines()
    else:
        # Never overwrite an earlier run; raw evidence should remain available.
        if args.log.exists() or args.log.with_suffix('.json').exists():
            parser.error('capture already exists; choose a new log path')
        power = PowerSwitch(args.power_url, args.power_entity) if args.power_url else None
        if power:
            metadata['power_url'] = args.power_url
            metadata['initial_power'] = power.state()
            power.set(False)
            time.sleep(2)  # Release console reset and the cartridge's PI/SD lock.
        info = subprocess.run(command + ['info'], capture_output=True, text=True, check=True, timeout=15)
        metadata['device_info'] = info.stdout + info.stderr
        if 'Firmware version:' not in metadata['device_info']:
            parser.error('SC64 did not return device information: ' + metadata['device_info'])
        if args.upload:
            # Let the deployer decide whether the console state permits upload.
            uploaded = subprocess.run(command + ['upload', '--direct', str(args.rom)], capture_output=True, text=True, check=True, timeout=60)
            metadata['upload'] = uploaded.stdout + uploaded.stderr
            if re.search(r'\berror\b', metadata['upload'], re.I):
                parser.error('upload failed: ' + metadata['upload'])
            print(metadata['upload'], flush=True)
        # The debugger is interactive, so give it a terminal even in CI/shell pipes.
        master, slave = pty.openpty()
        process = subprocess.Popen(command + ['debug', '--no-writeback'], stdin=slave, stdout=slave, stderr=slave, start_new_session=True)
        os.close(slave)
        selector = selectors.DefaultSelector()
        selector.register(master, selectors.EVENT_READ)
        pending = b''
        started = time.monotonic()
        print('Debugger listening.' + (' Automatic power-on follows.' if power else ' Turn on the N64 to start the ROM.'), flush=True)
        try:
            with args.log.open('x') as log:
                if power:
                    power.set(True)
                    print('N64 powered on; capturing scripted ROM.', flush=True)
                while time.monotonic() - started < args.timeout:
                    for _, _ in selector.select(1):
                        try:
                            chunk = os.read(master, 65536)
                        except OSError as exc:
                            if exc.errno != errno.EIO:
                                raise
                            chunk = b''
                        if not chunk:
                            raise RuntimeError('SC64 debugger exited before capture completed')
                        pending += chunk
                        while b'\n' in pending:
                            raw, pending = pending.split(b'\n', 1)
                            line = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', raw.decode(errors='replace')).rstrip('\r')
                            log.write(line + '\n'); log.flush()
                            lines.append(line)
                            print(line, flush=True)
                            if re.search(r'assertion failed|mismatch trial|RDPQ.*(?:ERROR|WARNING)|IO error', line, re.I):
                                raise RuntimeError(line)
                    profiled = any('Fluid profile:' in line for line in lines)
                    # Profiled batches end after draw/flow/audio diagnostics,
                    # not at Presentation. USB may split each line/packet.
                    if (sum('Presentation:' in line for line in lines) >= args.windows
                            and sum('simulation average' in line for line in lines) >= args.windows
                            and sum('draw average' in line for line in lines) >= args.windows
                            and sum('Audio stream:' in line for line in lines) >= args.windows
                            and (not profiled or sum('Plasma Pong 64: audio ' in line for line in lines) >= args.windows)):
                        break
                else:
                    raise RuntimeError('Timed out waiting for hardware measurement windows')
        except (RuntimeError, OSError, ValueError, KeyboardInterrupt) as exc:
            error = str(exc) or 'Capture interrupted'
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
            selector.close(); os.close(master)
            if power and (error or not args.leave_on):
                try:
                    power.set(False)
                    metadata['final_power'] = False
                except (OSError, RuntimeError, ValueError) as exc:
                    error = (error + '; ' if error else '') + 'Could not verify console power-off: ' + str(exc)
            elif power:
                metadata['final_power'] = True
    result = dict(metadata=metadata, complete=error is None, error=error, measurements=summarize(lines))
    args.log.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['measurements'], indent=2))
    if error:
        raise SystemExit(error)


if __name__ == '__main__':
    main()
