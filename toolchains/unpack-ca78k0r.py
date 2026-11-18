#!/usr/bin/env python3
"""Unpack Renesas CA78K0R, the syscon's (ernie's) C compiler, from Renesas' CS+ for CA,CX
package, without running any installer:

	python3 toolchains/unpack-ca78k0r.py CSPlus_CACX_Package_V41000.EXE

The package (from renesas.com, which asks for a My Renesas login) is a ZIP self-extractor
holding one InstallShield setup per product; CA78K0R's and the 78K device information's
are unpacked here (their overlay: files obfuscated with the file name as the key, then
zlib; the RL78 device information's setup is the newer ISSetupStream layout, UTF-16 names,
the same scrambling), and the MSI inside each is extracted with msitools' msiextract (on Windows:
msiexec /a, an administrative image, which installs nothing). The result, in
<PSP2_TOOLCHAINS>/ca78k0r-<version>: Bin/, Inc78k0r/, Lib78k0r/, Src/ as CA78K0R ships
them, and Dev78k0r/ with the 78K0R device files and the RL78 ones (DR5F*.DVF: the slim
models' RL78/G13, which CA78K0R compiles for with -c, e.g. -cf100sk).

The slim models' chip, Sony's R5F1ZCRK, is an RL78/G13 with the RL78-S3 core (MULHU, MULH,
DIVHU, DIVWU: Sony's code multiplies and divides with them), which no R5F100 device file has:
CA78K0R then calls its runtime instead (@@iumul, @@ludiv). DR5F1ZCRK.DVF is made here from the
R5F100SK's with the core byte (0x32: 2 for S2, 3 for S3, as the RL78/G14 files have it) set to
3 and the names changed, so -cf1zcrk compiles and assembles for an S3 R5F100SK. Run on an
unpacked toolchain (python3 toolchains/unpack-ca78k0r.py <PSP2_TOOLCHAINS>/ca78k0r-1.72) it
only writes that file.

Needs 7-Zip (7zz or 7z) and, except on Windows, msiextract (msitools).
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b'\x13\x35\x86\x07'
ROL4 = bytes((b << 4 | b >> 4) & 0xff for b in range(256))
CA78K0R = re.compile(r'CA78K0R__V(\d)(\d+)\.exe$', re.I)
DEVINFO = re.compile(r'DevInfo_78K__V\d+\.exe$', re.I)
DEVINFO_RL78 = re.compile(r'DevInfo_RL78__V\d+\.exe$', re.I)


def isx_files(exe):
	"""(name, data) of the files an InstallShield setup.exe carries after its PE image:
	"InstallShield\\0", a u16 count, 30 bytes, then per file a 312-byte header (name[260],
	flags u32, u32, length u32, 8 bytes, u16, 30 bytes) and the data. Flag 2: each byte is
	~(key ^ rol4(byte)), the key the file name ^ 13 35 86 07, restarted every 1024 bytes
	with flag 4; then zlib, when the data starts with a zlib header."""
	d = exe.read_bytes()
	pe = struct.unpack_from('<I', d, 0x3c)[0]
	nsec, opt = struct.unpack_from('<HH', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
	end = max(sum(struct.unpack_from('<II', d, pe + 24 + opt + 40 * i + 16)) for i in range(nsec))
	at = d.find(b'InstallShield\0', end)
	if d.startswith(b'ISSetupStream\0', end):
		yield from isx_stream(d, end)
		return
	if at < 0:
		sys.exit(f'{exe.name}: not an InstallShield setup this recipe knows')
	p = at + 14 + 2 + 30
	for _ in range(struct.unpack_from('<H', d, at + 14)[0]):
		name = d[p:p + 260].split(b'\0')[0]
		flags, _, length = struct.unpack_from('<III', d, p + 260)
		data = d[p + 312:p + 312 + length]
		p += 312 + length
		yield name.decode('latin-1'), descramble(name, flags, data)


def descramble(name, flags, data):
	"""flag 2: each byte ~(key ^ rol4(byte)), the key the file name ^ 13 35 86 07, restarted
	every 1024 bytes with flag 4; then zlib, when the data starts with a zlib header"""
	if flags & 2 and data:
		key = bytes(~(c ^ MAGIC[i % 4]) & 0xff for i, c in enumerate(name))
		step = 1024 if flags & 4 else len(data)
		block = (key * (step // len(key) + 1))[:step]
		stream = (block * (len(data) // step + 1))[:len(data)]
		data = (int.from_bytes(data.translate(ROL4), 'little') ^
			int.from_bytes(stream, 'little')).to_bytes(len(data), 'little')
	if flags & 4 and data[:1] == b'\x78':
		data = zlib.decompress(data)
	return data


def isx_stream(d, at):
	"""The newer layout: "ISSetupStream\\0", a u16 count, 30 bytes; per file a u32 name
	length, flags u32, 2 bytes, length u32, 8 bytes, u16, 24 bytes, the UTF-16 name and the
	data, scrambled as above with the name's UTF-8 bytes as the key"""
	p = at + 14 + 2 + 30
	for _ in range(struct.unpack_from('<H', d, at + 14)[0]):
		size, flags = struct.unpack_from('<II', d, p)
		length = struct.unpack_from('<I', d, p + 10)[0]
		name = d[p + 48:p + 48 + size].decode('utf-16-le')
		data = d[p + 48 + size:p + 48 + size + length]
		p += 48 + size + length
		yield name, descramble(name.encode('utf-8'), flags, data)


def msi_extract(msi, out):
	if sys.platform == 'win32':
		subprocess.run(['msiexec', '/a', str(msi), '/qn', f'TARGETDIR={out}'], check=True)
		return
	tool = shutil.which('msiextract')
	if not tool:
		sys.exit('msiextract is not installed (msitools: brew install msitools, apt install msitools)')
	subprocess.run([tool, '-C', str(out), str(msi)], check=True, stdout=subprocess.DEVNULL,
		       stderr=subprocess.DEVNULL)


def find(tree, *parts):
	"""the folder below tree whose path ends in parts (CS+/CACX/...)"""
	for path, dirs, _ in os.walk(tree):
		if Path(path).parts[-len(parts):] == parts:
			return Path(path)
	sys.exit(f'{"/".join(parts)} not found in the package')


def derive_s3(devices):
	"""Write DR5F1ZCRK.DVF: the R5F100SK's device file for the RL78-S3 core."""
	data = bytearray((devices / 'DR5F100SK.DVF').read_bytes())
	if data[0x34:0x3c] != b'R5F100SK' or data[0x44:0x4a] != b'f100sk' or data[0x32] != 2:
		sys.exit(f'{devices}/DR5F100SK.DVF: not the R5F100SK device file this expects')
	data[0x32] = 3
	data[0x34:0x3c], data[0x44:0x4a] = b'R5F1ZCRK', b'f1zcrk'
	(devices / 'DR5F1ZCRK.DVF').write_bytes(data)


def main():
	if len(sys.argv) != 2:
		sys.exit(__doc__)
	package = Path(sys.argv[1]).resolve()
	if package.is_dir():
		derive_s3(package / 'Dev78k0r')
		return
	seven = shutil.which('7zz') or shutil.which('7z')
	if not seven:
		sys.exit('7-Zip is not installed (7zz or 7z)')
	toolchains = Path(os.environ.get('PSP2_TOOLCHAINS') or ROOT / 'toolchains' / 'install')
	with tempfile.TemporaryDirectory() as tmp:
		tmp = Path(tmp)
		names = subprocess.run([seven, 'l', '-slt', str(package)], capture_output=True,
				       text=True, check=True).stdout
		setups = [n[7:] for n in names.splitlines() if n.startswith('Path = ') and
			  (CA78K0R.search(n) or DEVINFO.search(n) or DEVINFO_RL78.search(n))]
		ca = [s for s in setups if CA78K0R.search(s)]
		if not ca or not any(DEVINFO.search(s) for s in setups):
			sys.exit(f'{package.name}: no CA78K0R or 78K device information setup in it')
		major, minor = CA78K0R.search(ca[0]).groups()
		version = f'{major}.{minor}'
		subprocess.run([seven, 'x', '-y', f'-o{tmp / "pkg"}', str(package), *setups],
			       check=True, stdout=subprocess.DEVNULL)
		for setup in setups:
			unpacked = tmp / Path(setup).stem
			unpacked.mkdir()
			for name, data in isx_files(tmp / 'pkg' / setup):
				(unpacked / name).write_bytes(data)
			for msi in unpacked.glob('*.msi'):
				msi_extract(msi, tmp / 'files')
		out = toolchains / f'ca78k0r-{version}'
		shutil.rmtree(out, ignore_errors=True)
		shutil.copytree(find(tmp / 'files', 'CA78K0R', f'V{version}'), out)
		devices = find(tmp / 'files', 'Device', '78K0R', 'Devicefile')
		shutil.copytree(devices, out / 'Dev78k0r')
		if any(DEVINFO_RL78.search(s) for s in setups):
			for dvf in find(tmp / 'files', 'Device', 'RL78', 'Devicefile').glob('*.DVF'):
				shutil.copy(dvf, out / 'Dev78k0r')
			derive_s3(out / 'Dev78k0r')
	print(f'CA78K0R V{version} in {out}')


if __name__ == '__main__':
	main()
