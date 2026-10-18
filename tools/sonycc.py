"""Compile one source with the compiler Sony used for its module.

Sony built the firmware with two compilers: GCC 4.8.3 20140320 (SCEI rev. 5.3.3), Sony's
build of CodeSourcery's GCC, reproduced by that CodeSourcery release with the project's
corrections (toolchains/build-csl.sh; kernel, LibKernel, some libraries), and SNC 3.57 from
the SDK (most user libraries and apps).
compile_one() runs either with Sony's flags; build_sony.py builds whole modules with it.

How each compiler runs on this host (natively, in Lima, in WSL2, through Wine) and where
the toolchains and the SDK are is decided by host.py.
"""
from pathlib import Path
import os
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host  # noqa: E402

ROOT = host.ROOT
HERE = ROOT / 'tools'
TOOLCHAINS = host.toolchains()
# the project's GCC 4.8 (a Linux program); GCC48_TOOLCHAIN picks another build there,
# GCC48_DRIVER=g++ the C++ driver
GCC48 = TOOLCHAINS / os.environ.get('GCC48_TOOLCHAIN', 'csl-4.8-2014.05-strong-strd') / \
	'bin' / ('arm-none-eabi-' + os.environ.get('GCC48_DRIVER', 'gcc'))
COMPAT = ROOT / 'cmake/compat'

# Sony's GCC 4.8 flags (compile_one, and the CMake build: cmake/toolchain.cmake): the target,
# the code generation, and the freestanding kernel (arm-none-eabi defaults to short enums;
# Sony's ABI has int-sized enums)
GCC48_TARGET = ('-mcpu=cortex-a9', '-mfloat-abi=softfp', '-mfpu=neon')
GCC48_CODE = ('-fno-optimize-sibling-calls', '-fno-delete-null-pointer-checks', '-fno-section-anchors',
	      '-fno-shrink-wrap')
GCC48_FREESTANDING = ('-fno-short-enums', '-ffreestanding', '-fno-builtin')

# compile_one's own Sony flags; the module's other -f options (MODULE_COMPILE_OPTIONS:
# how Sony built that module) come from the module's build
DEFAULT_F = ('-fstack-protector-strong', *GCC48_CODE, *GCC48_FREESTANDING)

# SNC: Sony's macro spellings, then the project's include order: Sony's standard headers
# before vitasdk's psp2kern headers
SNC_DEFINES = ('-DSCE_DEPRECATED(n)=n', '-DSCE_DEPRECATED_EX(n,c)=n',
	       '-DSCE_ALIGN(x)=__attribute__((aligned(x)))',
	       '-D__builtin_return_address(x)=__builtin_return_address()')


def win(p):
	"""A path as psp2snc.exe sees it on this host."""
	return host.windows_path(p)


def __getattr__(name):
	# resolved on first use, so importing sonycc needs no vitasdk
	if name == 'VITASDK_INCLUDE':
		return host.vitasdk_include()
	raise AttributeError(name)


def snc_command(opt, args):
	sdk = host.sdk()
	includes = [ROOT / 'sys/internal/include', ROOT / 'sys/sdk/target/include',
		    ROOT / 'cmake/include', COMPAT / 'vitasdk', ROOT,
		    sdk / 'target/include', sdk / 'target/include_common', host.vitasdk_include()]
	return host.windows_command([sdk / 'host_tools/build/bin/psp2snc.exe', *SNC_DEFINES, opt,
				     *args, *(f'-I{win(i)}' for i in includes)])


def file_name(src):
	"""__FILE_NAME__ of a source, as Sony's debug records name it: the path from its
	component's folder (the nearest folder above it with a CMakeLists.txt); the bare file name
	for a source outside a component (common/, contrib/, generated in a build folder)."""
	src = Path(src).resolve()
	for d in src.parents:
		if d == ROOT or ROOT not in d.parents:
			break
		if (d / 'CMakeLists.txt').exists():
			return src.relative_to(d).as_posix()
	return src.name


def compile_one(src, out, kind, defines, includes, opt, gcc=None, extra=(), mode='thumb', stage='object'):
	if stage not in ('object', 'assembly') or (stage == 'assembly' and kind != 'gcc48'):
		raise ValueError('unsupported compiler output stage')
	if mode not in ('arm', 'thumb'):
		raise ValueError(f'unsupported instruction mode: {mode}')
	if kind != 'gcc48' and mode != 'thumb':
		raise ValueError('ARM source mode is not implemented for SNC')
	out.parent.mkdir(parents=True, exist_ok=True)
	name = f'-D__FILE_NAME__="{file_name(src)}"'
	if kind == 'gcc48':
		cmd = host.linux_command([str(gcc or GCC48), '-m' + mode, *GCC48_TARGET, opt, '-std=gnu11',
					  '-fstack-protector-strong' if gcc is None else '-fstack-protector',
					  *GCC48_CODE, *extra, *GCC48_FREESTANDING, '-w', name, *defines,
					  *includes,
					  '-S' if stage == 'assembly' else '-c', str(src), '-o', str(out)], cwd=ROOT)
	else:
		incs = [f'-I{win(i[2:])}' if i.startswith('-I') else i for i in includes]
		extra = [f'-I{win(i[2:])}' if i.startswith('-I') and len(i) > 2 else i for i in extra]
		# asm is a C++ keyword (the SDK's xkeycheck.h rejects redefining it)
		asm = [] if src.suffix == '.cpp' else ['-Dasm=__asm__']
		cmd = snc_command(opt, ['-Xthumb', *extra, *asm, name, *defines, *incs,
					'-c', win(src), '-o', win(out)])
	result = subprocess.run(cmd, capture_output=True, text=True)
	if result.returncode:
		sys.stderr.write(result.stdout + result.stderr)
		return False
	return True


# the programs of the GCC 4.8 toolchain the CMake build runs (toolchain())
PROGRAMS = ('gcc', 'g++', 'ar', 'ranlib', 'objcopy', 'nm', 'readelf', 'strip')


def toolchain(folder):
	"""cmake/toolchain.cmake's half: a command per GCC 4.8 program in folder that runs it on this
	host, and folder/flags.cmake naming them, with Sony's flags and the system headers."""
	folder = Path(folder)
	folder.mkdir(parents=True, exist_ok=True)
	bindir = GCC48.parent
	if not (bindir / 'arm-none-eabi-gcc').exists():
		raise host.HostError(f'GCC 4.8 is not built in {bindir.parent}: run toolchains/build-binutils.sh '
				     'and toolchains/build-csl.sh')
	lines = ['# generated by tools/sonycc.py toolchain (cmake/toolchain.cmake)']
	for program in PROGRAMS:
		path = host.linux_wrapper(bindir / f'arm-none-eabi-{program}', folder / f'arm-none-eabi-{program}')
		lines.append(f'set(PSP2_GCC48_{program.upper().replace("+", "X")} "{path.as_posix()}")')
	lines += [f'set(PSP2_GCC48_TARGET "{" ".join(GCC48_TARGET)}")',
		  f'set(PSP2_GCC48_FLAGS "{" ".join(("-fstack-protector-strong", *GCC48_CODE, *GCC48_FREESTANDING))}")',
		  f'set(PSP2_SYSTEM_INCLUDE "{host.vitasdk_include().as_posix()}")']
	(folder / 'flags.cmake').write_text('\n'.join(lines) + '\n')


def snc(argv):
	"""The compiler launcher of a module Sony built with SNC (psp2_module COMPILER snc): argv is
	the GCC command CMake made; SNC compiles its source with its defines, includes and options,
	as build_sony.py does. GNU ld reads REL objects, so SNC's RELA object is converted."""
	import buildrecipe
	import elfsections
	options = buildrecipe.compile_options(argv)
	src = Path(argv[argv.index('-c') + 1])
	out = Path(argv[argv.index('-o') + 1])
	if not compile_one(src, out, 'snc', options['defines'], options['includes'], options['opt'],
			   extra=options['extra']):
		return 1
	data = out.read_bytes()
	if any(h['type'] == 4 for h in elfsections.Sections(data).headers):
		out.write_bytes(elfsections.object_rela_to_rel(data))
	if '-MF' in argv:
		# SNC's own dependency output is not used: the object depends on its source
		Path(argv[argv.index('-MF') + 1]).write_text(f'{out}: {src}\n')
	return 0


if __name__ == '__main__':
	# sonycc.py gcc48 ARGS...: the project's GCC 4.8 with ARGS, as this host runs it
	# (gcc48.sh, for Makefiles that want a compiler command)
	# sonycc.py toolchain FOLDER: the CMake build's commands and flags (cmake/toolchain.cmake)
	# sonycc.py snc COMPILER ARGS...: SNC in place of the GCC command (CMake's compiler launcher)
	command = sys.argv[1:2]
	try:
		if command == ['gcc48']:
			sys.exit(subprocess.run(host.linux_command([GCC48, *sys.argv[2:]])).returncode)
		if command == ['toolchain'] and len(sys.argv) == 3:
			sys.exit(toolchain(sys.argv[2]))
		if command == ['snc'] and len(sys.argv) > 3:
			sys.exit(snc(sys.argv[2:]))
	except host.HostError as e:
		sys.exit(f'sonycc.py: {e}')
	sys.exit('usage: sonycc.py gcc48 ARGS... | toolchain FOLDER | snc COMPILER ARGS...')
