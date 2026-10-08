#!/usr/bin/env python3
"""Build, validate in Ares, and measure an immutable ROM on the NTSC-J N64."""
import argparse
import datetime
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import zipfile
from n64_power import configured_power_url

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name', help='unique lowercase experiment name')
    parser.add_argument('--define', action='append', default=[], metavar='KEY=VALUE', help='candidate build setting; repeat as needed')
    parser.add_argument('--workload', choices=('menu', '2p', '4p', 'effects'), default='4p')
    parser.add_argument('--profile', action='store_true', help='diagnostic stage timings; compare separately from ordinary captures')
    parser.add_argument('--kernel-checks', action='store_true', help='run exact RSP numerical fixtures in validation ROMs only')
    parser.add_argument('--windows', type=int, default=10)
    parser.add_argument('--ares-windows', type=int, default=2)
    parser.add_argument('--ares', default='../ares/build/rundir/bin/ares')
    parser.add_argument('--power-url', default=configured_power_url(), help='defaults to N64_POWER_URL in environment or .env')
    parser.add_argument('--port', default='serial:///dev/ttyUSB0')
    parser.add_argument('--emulator-only', action='store_true')
    parser.add_argument('--skip-check', action='store_true', help='only when the current source has already passed portable checks')
    args = parser.parse_args()
    if not args.emulator_only and not args.power_url:
        parser.error('set N64_POWER_URL in .env or the environment, or provide --power-url')
    if not re.fullmatch(r'[a-z][a-z0-9-]{0,47}', args.name):
        parser.error('name must start with a letter and contain lowercase letters, digits or hyphens')
    if min(args.windows, args.ares_windows) < 1:
        parser.error('window counts must be positive')
    reserved = {'ROM', 'BUILD_DIR', 'USB_LOG', 'RDP_VALIDATE', 'FLUID_PROFILE', 'QUEUE_PROFILE', 'FRAME_WORK_PROFILE', 'SMOKE', 'BENCH_MENU', 'SMOKE_PLAYERS', 'SMOKE_HIGH_RES', 'SMOKE_EFFECT', 'SMOKE_STRESS', 'SMOKE_FLOW'}
    for setting in args.define:
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*=[A-Za-z0-9_.+-]+', setting) or setting.split('=')[0] in reserved:
            parser.error('invalid or loop-owned build setting: ' + setting)
    keys = [setting.split('=')[0] for setting in args.define]
    if len(set(keys)) != len(keys):
        parser.error('duplicate build setting')
    run = ROOT / 'build/dev-loop' / args.name
    run.mkdir(parents=True, exist_ok=False)
    manifest = {'name': args.name, 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'console': 'NTSC-J N64 / SummerCart64', 'arguments': vars(args), 'commands': [], 'complete': False}
    manifest['revision'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    (run / 'source.patch').write_bytes(subprocess.check_output(['git', 'diff', 'HEAD'], cwd=ROOT))
    manifest['git_status'] = subprocess.check_output(['git', 'status', '--short'], cwd=ROOT, text=True)
    # Retain contents as well as hashes: untracked prototypes cannot be recovered
    # from git diff when an unsuccessful experiment is removed.
    paths = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=ROOT).decode().split('\0')
    manifest['source_sha256'] = {}
    archive = run / 'source.zip'
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as snapshot:
        for path in paths:
            if not path or not (ROOT / path).is_file():
                continue
            content = (ROOT / path).read_bytes()
            manifest['source_sha256'][path] = hashlib.sha256(content).hexdigest()
            snapshot.writestr(path, content)
    manifest['source_archive_sha256'] = hashlib.sha256(archive.read_bytes()).hexdigest()

    def execute(command, log):
        manifest['commands'].append(command)
        print('Running: ' + ' '.join(command), flush=True)
        with (run / log).open('x') as output:
            subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)

    common = ['USB_LOG=1'] + args.define
    if args.workload == 'menu':
        common += ['BENCH_MENU=1']
    else:
        common += ['SMOKE=1', 'SMOKE_PLAYERS=' + ('2' if args.workload == '2p' else '4'),
                   'SMOKE_HIGH_RES=1', 'SMOKE_STRESS=1']
        common += ['SMOKE_FLOW=1'] if args.workload == 'effects' else ['SMOKE_EFFECT=2']
    if args.profile:
        common += ['FLUID_PROFILE=1']
    name = 'plasmapong-dev-' + args.name
    build = 'build/dev_loop_' + args.name.replace('-', '_')
    try:
        execute([sys.executable, 'tests/dev_loop_test.py'], 'loop-tests.log')
        if not args.skip_check:
            execute(['./tools/check.sh'], 'check.log')
        fixtures = ['RSP_TEST=1', 'UPWIND_TEST=1', 'PREPARE_TEST=1'] if args.kernel_checks else []
        execute(['./tools/build-rom.sh', '-j4'] + common + fixtures + ['RDP_VALIDATE=1', 'ROM=' + name + '-validate', 'BUILD_DIR=' + build + '_validate'], 'build-validate.log')
        for memory in (8, 4):
            command = [sys.executable, 'tools/benchmark-ares.py', name + '-validate.z64', str(run / f'ares-{memory}m.log'),
                       '--ares', args.ares, '--memory-mib', str(memory), '--windows', str(args.ares_windows), '--timeout', '240']
            if args.workload == 'menu':
                command += ['--draw-only']
            execute(command, f'ares-{memory}m-runner.log')
        execute(['./tools/build-rom.sh', '-j4'] + common + ['ROM=' + name, 'BUILD_DIR=' + build], 'build-hardware.log')
        rom = run / (name + '.z64')
        rom.write_bytes((ROOT / (name + '.z64')).read_bytes())
        manifest['rom_sha256'] = hashlib.sha256(rom.read_bytes()).hexdigest()
        # Run the exact hardware ROM in Ares as well; validation overhead stays separate.
        command = [sys.executable, 'tools/benchmark-ares.py', str(rom), str(run / 'ares-exact.log'), '--ares', args.ares,
                   '--windows', str(args.ares_windows), '--timeout', '240']
        if args.workload == 'menu':
            command += ['--draw-only']
        execute(command, 'ares-exact-runner.log')
        if not args.emulator_only:
            execute([sys.executable, 'tools/benchmark-hardware.py', str(rom), str(run / 'hardware.log'),
                     '--upload', '--port', args.port, '--power-url', args.power_url,
                     '--windows', str(args.windows), '--timeout', '300'], 'hardware-runner.log')
            result = json.loads((run / 'hardware.json').read_text())
            manifest['hardware_measurements'] = result['measurements']
            timings = result['measurements']
            print(f"Console: {timings['render_fps']:.3f} FPS, "
                  f"{timings['simulation average']['mean_us']/1000:.3f} ms/simulation update", flush=True)
        manifest['complete'] = True
    except (subprocess.CalledProcessError, OSError, KeyboardInterrupt) as exc:
        manifest['error'] = str(exc) or 'Interrupted'
        raise
    finally:
        (run / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print('Evidence: ' + str(run), flush=True)


if __name__ == '__main__':
    main()
