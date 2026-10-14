"""How this host runs the project's compilers, decided from the operating system.

The project's GCC 4.8 and MeP toolchains are Linux programs, and Sony's SNC (psp2snc.exe)
is a Windows program:

  host      Linux toolchains                      psp2snc.exe
  Linux     run natively                          through Wine (required)
  macOS     in a Lima VM (limactl required)       through Wine (required)
  Windows   through WSL2's bash (required)        run natively

A missing requirement stops with an error that says what to install; nothing falls back
silently. On macOS the VM must mount the project at the same path (Lima's default for
the home folder); on Windows, paths are translated to WSL's /mnt/<drive>/ form.

Locations, from the environment:
  PSP2_TOOLCHAINS   built toolchains (the recipes in toolchains/ install there); default
                    toolchains/install
  PSP2_SDK          the Sony PSP2 SDK's "SDK Software/PSVITA/sdk" folder (required for SNC)
  VITASDK           vitasdk, for its headers when the pinned copy is not installed
  PSP2_LIMA         the Lima instance on macOS (default psp2)
  WINE              the Wine program (default: wine64 or wine on PATH)

  python3 host.py check                 report what this host will use
  python3 host.py linux -- PROG ARGS    run a Linux program (used by the shell wrappers)
"""
from pathlib import Path, PureWindowsPath
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SYSTEM = {'linux': 'linux', 'darwin': 'macos', 'win32': 'windows', 'cygwin': 'windows',
	  'msys': 'windows'}.get(sys.platform, sys.platform)


class HostError(SystemExit):
	"""A requirement of this host is missing."""


def toolchains():
	if os.environ.get('PSP2_TOOLCHAINS'):
		return Path(os.environ['PSP2_TOOLCHAINS'])
	return ROOT / 'toolchains' / 'install'


def sdk():
	"""The SDK's PSVITA/sdk folder (host_tools/, target/)."""
	if os.environ.get('PSP2_SDK'):
		return Path(os.environ['PSP2_SDK'])
	raise HostError('the Sony PSP2 SDK is not found: set PSP2_SDK to its '
			'"SDK Software/PSVITA/sdk" folder')


def vitasdk_include():
	"""vitasdk headers: the pinned copy all results were measured with, else $VITASDK's."""
	pinned = toolchains() / 'vitasdk-include-2026-05-24'
	if pinned.is_dir():
		return pinned
	if os.environ.get('VITASDK'):
		return Path(os.environ['VITASDK']) / 'arm-vita-eabi' / 'include'
	raise HostError('vitasdk headers are not found: set VITASDK')


# --- Linux programs -------------------------------------------------------------------

_checked = {}


def _lima_instance():
	if 'lima' in _checked:
		return _checked['lima']
	if not shutil.which('limactl'):
		raise HostError('macOS runs the project\'s Linux toolchains in a Lima VM, and limactl '
				'is not installed (https://lima-vm.io)')
	name = os.environ.get('PSP2_LIMA', 'psp2')
	status = subprocess.run(['limactl', 'list', name, '--format', '{{.Status}}'],
				capture_output=True, text=True).stdout.strip()
	if not status:
		raise HostError(f'the Lima instance "{name}" does not exist: create one that mounts '
				f'{ROOT} at the same path (or set PSP2_LIMA)')
	if status != 'Running':
		raise HostError(f'the Lima instance "{name}" is {status.lower()}: limactl start {name}')
	_checked['lima'] = name
	return name


def _wsl():
	if 'wsl' in _checked:
		return _checked['wsl']
	wsl = shutil.which('wsl.exe') or shutil.which('wsl')
	if not wsl or subprocess.run([wsl, '-e', 'bash', '-c', 'true'],
				     capture_output=True).returncode:
		raise HostError('Windows runs the project\'s Linux toolchains in WSL2, and no WSL2 '
				'distribution with bash was found (wsl --install)')
	_checked['wsl'] = wsl
	return wsl


_DRIVE = re.compile(r'(?<![A-Za-z0-9])([A-Za-z]):[\\/]([^\s"\';]*)')


def wsl_path(text):
	"""C:\\x\\y (or C:/x/y) anywhere in an argument -> /mnt/c/x/y."""
	return _DRIVE.sub(lambda m: f'/mnt/{m.group(1).lower()}/' + m.group(2).replace('\\', '/'),
			  str(text))


def linux_command(argv, cwd=None):
	"""The command that runs the Linux program argv on this host."""
	argv = [str(a) for a in argv]
	cwd = str(cwd or os.getcwd())
	if SYSTEM == 'linux':
		return argv
	if SYSTEM == 'macos':
		return ['limactl', 'shell', '--workdir', cwd, _lima_instance(), '--', *argv]
	if SYSTEM == 'windows':
		return [_wsl(), '--cd', wsl_path(cwd), '-e', *map(wsl_path, argv)]
	raise HostError(f'unsupported host system: {sys.platform}')


# --- psp2snc.exe ----------------------------------------------------------------------

def _wine():
	if 'wine' in _checked:
		return _checked['wine']
	wine = os.environ.get('WINE') or shutil.which('wine64') or shutil.which('wine')
	if not wine or not Path(wine).exists():
		raise HostError('psp2snc.exe is a Windows program, and Wine is not installed '
				'(or set WINE to the wine program)')
	# Keep a wineserver and the booted prefix alive between compiles (a cold prefix costs
	# seconds per compile); both start detached from our stdio, or the caller would wait
	# for Wine to exit.
	server = Path(wine).with_name('wineserver')
	if server.exists() and subprocess.run([str(server), '-p'], stdin=subprocess.DEVNULL,
					      capture_output=True).returncode == 0:
		subprocess.run([wine, 'wineboot'], stdin=subprocess.DEVNULL, capture_output=True)
	os.environ.setdefault('WINEDEBUG', '-all')
	_checked['wine'] = wine
	return wine


def windows_path(p):
	"""A path as psp2snc.exe sees it: native on Windows, Wine's Z: drive (= /) elsewhere."""
	if SYSTEM == 'windows':
		return str(PureWindowsPath(p))
	return 'Z:' + str(Path(p).resolve()).replace('/', '\\')


def windows_command(argv):
	"""The command that runs the Windows program argv (psp2snc.exe) on this host."""
	argv = [str(a) for a in argv]
	if SYSTEM == 'windows':
		return argv
	if SYSTEM in ('linux', 'macos'):
		return [_wine(), *argv]
	raise HostError(f'unsupported host system: {sys.platform}')


# --- command line ---------------------------------------------------------------------

def main():
	args = sys.argv[1:]
	if args[:1] == ['linux'] and '--' in args:
		argv = args[args.index('--') + 1:]
		try:
			return subprocess.run(linux_command(argv)).returncode
		except HostError as e:
			sys.stderr.write(f'host.py: {e}\n')
			return 1
	if args == ['check']:
		print('system:     ', SYSTEM)
		print('toolchains: ', toolchains())
		for name, probe in (('linux', lambda: linux_command(['true'])[:-1] or 'native'),
				    ('psp2snc', lambda: windows_command(['psp2snc.exe'])[:-1] or 'native'),
				    ('sdk', sdk), ('vitasdk', vitasdk_include)):
			try:
				print(f'{name + ":":12}', probe())
			except HostError as e:
				print(f'{name + ":":12} MISSING: {e}')
		return 0
	sys.stderr.write(__doc__)
	return 2


if __name__ == '__main__':
	sys.exit(main())
