#!/usr/bin/env python3
"""Instrument pinned libdragon in an isolated diagnostic Docker image."""
import pathlib
import subprocess

source = pathlib.Path('/libdragon/include/rsp_queue.inc')
text = source.read_text()
data_anchor = '    .align 4\n# Overlay data will be loaded at this address'
wait_anchor = '''RSPQ_RdpWait:
    mfc0 t2, COP0_DP_STATUS
1:
    # Wait for selected RDP status bits to become 0.
    and t1, t2, t3
    bnez t1, 1b
    mfc0 t2, COP0_DP_STATUS
    jr ra
    nop'''
assert text.count(data_anchor) == text.count(wait_anchor) == 1
text = text.replace(data_anchor, '''    .align 2
PLASMAPONG_RDP_WAIT_MASK:      .word 0
PLASMAPONG_RDP_WAIT_CALLER:    .word 0

''' + data_anchor)
text = text.replace(wait_anchor, '''RSPQ_RdpWait:
    sw ra, %lo(PLASMAPONG_RDP_WAIT_CALLER)
    sw t3, %lo(PLASMAPONG_RDP_WAIT_MASK)
    mfc0 t2, COP0_DP_STATUS
1:
    # Keep the original polling loop unchanged.
    and t1, t2, t3
    bnez t1, 1b
    mfc0 t2, COP0_DP_STATUS
    jr ra
    sw zero, %lo(PLASMAPONG_RDP_WAIT_MASK)''')
source.write_text(text)
subprocess.run(['make', '-C', '/libdragon', '-j4', 'libdragon'], check=True)
subprocess.run(['make', '-C', '/libdragon', 'install'], check=True)
symbols = subprocess.check_output([
    '/n64_toolchain/bin/mips64-elf-nm', '-n',
    '/libdragon/build/rspq/rsp_queue.elf'], text=True)
defines = []
for name in ('PLASMAPONG_RDP_WAIT_MASK', 'PLASMAPONG_RDP_WAIT_CALLER'):
    rows = [line.split() for line in symbols.splitlines() if line.endswith(' ' + name)]
    assert len(rows) == 1
    address = int(rows[0][0], 16)
    assert 0xa4000000 <= address < 0xa4001000 and address % 4 == 0
    defines.append(f'#define {name}_ADDRESS 0x{address:08x}u')
pathlib.Path('/n64_toolchain/mips64-elf/include/plasmapong_rdp_trace.h').write_text(
    '/* Generated from the isolated tracing queue ELF. */\n' + '\n'.join(defines) + '\n')
