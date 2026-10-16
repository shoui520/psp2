"""Compile one source with the compiler Sony used for its module.

Sony built the firmware with two compilers: GCC 4.8, best reproduced by Linaro GCC
4.8-2014.04 with the project's corrections (toolchains/build-strd-variant.sh; kernel,
LibKernel, some libraries), and SNC 3.57 from the SDK (most user libraries and apps).
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
GCC48 = TOOLCHAINS / os.environ.get('GCC48_TOOLCHAIN', 'linaro-4.8-2014.04-strong-strd') / \
	'bin' / ('arm-none-eabi-' + os.environ.get('GCC48_DRIVER', 'gcc'))
COMPAT = ROOT / 'cmake/compat'

# compile_one's own Sony flags; the module's other -f options (MODULE_COMPILE_OPTIONS:
# how Sony built that module) come from the module's build
DEFAULT_F = ('-fstack-protector-strong', '-ffreestanding', '-fno-builtin', '-fno-optimize-sibling-calls',
	     '-fno-delete-null-pointer-checks', '-fno-section-anchors', '-fno-shrink-wrap')

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
	includes = [COMPAT / 'snc', ROOT / 'common/include', ROOT,
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
		cmd = host.linux_command([str(gcc or GCC48), '-m' + mode, '-mcpu=cortex-a9', '-mfloat-abi=softfp', '-mfpu=neon', opt, '-std=gnu11', '-fstack-protector-strong' if gcc is None else '-fstack-protector', '-fno-optimize-sibling-calls', '-fno-delete-null-pointer-checks', '-fno-section-anchors', '-fno-shrink-wrap', *extra,
		       # arm-none-eabi defaults to short enums; Sony's (and vitasdk's) ABI has int-sized enums
		       '-fno-short-enums', '-ffreestanding', '-fno-builtin', '-w', name, *defines,
		       f'-I{COMPAT / "gcc48"}', *includes, '-isystem', str(host.vitasdk_include()),
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


if __name__ == '__main__':
	# sonycc.py gcc48 ARGS...: the project's GCC 4.8 with ARGS, as this host runs it
	# (gcc48.sh, for Makefiles that want a compiler command)
	if sys.argv[1:2] != ['gcc48']:
		sys.exit('usage: sonycc.py gcc48 ARGS...')
	try:
		sys.exit(subprocess.run(host.linux_command([GCC48, *sys.argv[2:]])).returncode)
	except host.HostError as e:
		sys.exit(f'sonycc.py: {e}')
