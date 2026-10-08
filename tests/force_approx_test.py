"""Compare experimental integer forces with float kernels and radial behavior."""
import ctypes
import pathlib
import random
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
flags = ['-std=c11', '-O3', '-shared', '-fPIC', '-DPLASMAPONG_SPLAT_PLAN',
         '-DPLASMAPONG_DYE_FIXED', '-DPLASMAPONG_VELOCITY_FIXED', '-Isrc']
sources = ['src/fluid.c', 'src/fluid_advection.c', 'src/fluid_dye_fixed.c', 'src/fluid_velocity_fixed.c']
libraries = []
for name, extra in [('baseline', []), ('candidate', ['-DPLASMAPONG_FORCE_FIXED'])]:
    path = root / ('build/force-' + name + '.so')
    subprocess.run(['cc'] + flags + extra + sources + ['-lm', '-o', str(path)], cwd=root, check=True)
    library = ctypes.CDLL(str(path))
    library.fluid_init.argtypes = [ctypes.c_void_p]
    library.fluid_splat.argtypes = [ctypes.c_void_p] + [ctypes.c_float]*6 + [ctypes.c_int]
    library.fluid_pump.argtypes = [ctypes.c_void_p] + [ctypes.c_float]*5 + [ctypes.c_int]
    libraries.append(library)

# Align overallocated state buffers; the shipping fixed grid has 1584 cells.
owners = [ctypes.create_string_buffer(100000) for _ in libraries]
addresses = [(ctypes.addressof(owner)+15)&~15 for owner in owners]
fields = [(ctypes.c_int16 * (10*1584)).from_address(address) for address in addresses]
rng = random.Random(731)
velocity_max = dye_max = total_error = compared = 0
for trial in range(600):
    for library, address in zip(libraries, addresses):
        library.fluid_init(address)
    initial = [rng.randrange(-4000, 4001) for _ in range(2*1584)]
    for field in fields:
        field[:2*1584] = initial
    x, y, radius = rng.uniform(-20,308), rng.uniform(-20,218), rng.uniform(12,50)
    if trial % 2:
        parameters = (x,y,radius,rng.uniform(-240,240),rng.uniform(-240,240),rng.uniform(0,1),trial%4)
        function = 'fluid_splat'
    else:
        parameters = (x,y,radius,rng.choice((-1150,2800)),rng.uniform(0,.13),trial%4)
        function = 'fluid_pump'
    for library, address in zip(libraries, addresses):
        getattr(library,function)(address,*parameters)
    for k in range(2*1584):
        error = abs(fields[0][k]-fields[1][k])
        velocity_max = max(velocity_max,error)
        total_error += error
        compared += 1
        assert abs(fields[1][k]) <= 6720
    for k in range(4*1584,7*1584):
        dye_max = max(dye_max,abs(fields[0][k]-fields[1][k]))
        assert 0 <= fields[1][k] <= 24576

# From rest, suction points toward the source and creates no new pigment.
candidate = libraries[1]
for sign in (-1,1):
    candidate.fluid_init(addresses[1])
    candidate.fluid_pump(addresses[1],147,99,35,sign*1150,1/60,0)
    for iy in range(33):
        for ix in range(48):
            k = iy*48+ix
            assert fields[1][k]*(ix*6+3-147)*sign >= 0
            assert fields[1][1584+k]*(iy*6+3-99)*sign >= 0
    if sign < 0:
        assert not any(fields[1][4*1584:7*1584])

print(f'Force differential: max velocity error {velocity_max/16:.4f} px/s, '
      f'mean {total_error/compared/16:.6f}, max dye error {dye_max/8192:.6f}')
assert velocity_max/16 <= 3, 'single-injection velocity approximation exceeded budget'
assert dye_max/8192 <= .006, 'single-injection dye approximation exceeded budget'
print('PASS: 600 force comparisons, bounded fields, inward suction/outward jets, no suction pigment')
