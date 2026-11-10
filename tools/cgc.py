#!/usr/bin/env python3
"""cgc.py SOURCE OUTPUT: a Cg program compiled by the SDK's psp2cgc into a GXP program.

The profile comes from the source's suffix, as Sony's paths name them: .vp is a vertex program
(sce_vp_psp2), .fp a fragment program (sce_fp_psp2). psp2cgc pads its output file to a multiple
of four bytes; the program is the size its header gives (the word at +8), and that is what is
written, as the firmware's modules carry it."""
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host  # noqa: E402

PROFILES = {'.vp': 'sce_vp_psp2', '.fp': 'sce_fp_psp2'}


def compile_program(src, out):
	src, out = Path(src), Path(out)
	profile = PROFILES.get(src.suffix)
	if profile is None:
		raise host.HostError(f'{src}: not a .vp or .fp program')
	out.parent.mkdir(parents=True, exist_ok=True)
	cgc = host.sdk() / 'host_tools/bin/psp2cgc.exe'
	result = subprocess.run(host.windows_command([cgc, '-profile', profile, host.windows_path(src),
						      '-o', host.windows_path(out)]),
				capture_output=True, text=True)
	if result.returncode or not out.exists():
		sys.stderr.write(result.stdout + result.stderr)
		return 1
	data = out.read_bytes()
	if data[:4] != b'GXP\0':
		sys.stderr.write(f'{out}: not a GXP program\n')
		return 1
	size, = struct.unpack_from('<I', data, 8)
	out.write_bytes(data[:size])
	return 0


if __name__ == '__main__':
	if len(sys.argv) != 3:
		sys.exit('usage: cgc.py SOURCE OUTPUT')
	try:
		sys.exit(compile_program(sys.argv[1], sys.argv[2]))
	except host.HostError as e:
		sys.exit(f'cgc.py: {e}')
