#!/usr/bin/env python3
"""Build a native-frame capture adapter without modifying the Ares checkout.

Reuse the configured CMake objects/libraries, replace only the desktop Program
translation unit, and write all generated sources, objects and binaries under
this repository's ignored build directory. No desktop input/screenshot API.
"""
import argparse
import hashlib
import json
import pathlib
import re
import shlex
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ares-source', type=pathlib.Path, default=root.parent / 'ares')
args = parser.parse_args()
source = args.ares_source.resolve()
build = source / 'build/desktop-ui'
cmake = build / 'CMakeFiles/desktop-ui.dir'
out = root / 'build/ares-native-capture'
out.mkdir(parents=True, exist_ok=True)
original = source / 'desktop-ui/program'
platform = (original / 'platform.cpp').read_text()
needle = '''auto Program::video(ares::Node::Video::Screen node, const u32* data, u32 pitch, u32 width, u32 height) -> void {
  if(screens.empty()) return;'''
replacement = needle + '''
  // Native screenshot request supplied by the development runner, not desktop input.
  static u64 captureField = 0;
  if(const char* requestedField = std::getenv("ARES_CAPTURE_FRAME")) {
    if(++captureField == std::strtoull(requestedField, nullptr, 10)) {
      if(const char* filename = std::getenv("ARES_CAPTURE_PATH")) {
        bool ok = Encode::PNG::RGB8(filename, data, pitch, width, height);
        std::fprintf(stderr, "Native Ares capture %s: %s\\n", ok ? "saved" : "failed", filename);
      }
    }
  }'''
if platform.count(needle) != 1:
    raise SystemExit('Ares video callback changed; review the native capture adapter')
platform_path = out / 'platform-capture.cpp'
platform_path.write_text(platform.replace(needle, replacement))
program = (original / 'program.cpp').read_text()
def include(match):
    name = match.group(1)
    path = platform_path if name == 'platform.cpp' else (original / name).resolve()
    return '#include "' + str(path) + '"'
program_path = out / 'program-capture.cpp'
program_path.write_text('#include <cstdlib>\n#include <cstdio>\n' + re.sub(r'#include "([^"]+)"', include, program))
settings = {}
for line in (cmake / 'flags.make').read_text().splitlines():
    if line.startswith(('CXX_DEFINES = ', 'CXX_INCLUDES = ', 'CXX_FLAGS = ')):
        name, value = line.split(' = ', 1)
        settings[name] = shlex.split(value)
obj = out / 'program-capture.cpp.o'
compile_command = ['/usr/bin/c++'] + settings['CXX_DEFINES'] + settings['CXX_INCLUDES'] + settings['CXX_FLAGS'] + ['-c', str(program_path), '-o', str(obj)]
subprocess.run(compile_command, cwd=build, check=True)
link_command = shlex.split((cmake / 'link.txt').read_text())
object_name = 'CMakeFiles/desktop-ui.dir/program/program.cpp.o'
if link_command.count(object_name) != 1:
    raise SystemExit('Ares link layout changed; review the native capture adapter')
link_command[link_command.index(object_name)] = str(obj)
binary = out / 'ares'
link_command[link_command.index('-o')+1] = str(binary)
link_command = ['-Wl,--dependency-file=' + str(out / 'link.d') if arg.startswith('-Wl,--dependency-file=') else
                '-flto=2' if arg == '-flto=auto' else arg for arg in link_command]
subprocess.run(link_command, cwd=build, check=True)
(out / 'manifest.json').write_text(json.dumps({
    'source': str(source), 'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip(),
    'source_status': subprocess.check_output(['git', 'status', '--short'], cwd=source, text=True),
    'platform_sha256': hashlib.sha256(platform.encode()).hexdigest(),
    'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    'compile': compile_command, 'link': link_command,
}, indent=2) + '\n')
print(binary)
