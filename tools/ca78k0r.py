"""Renesas CA78K0R, the C compiler and assembler of the syscon (ernie, a NEC 78K0R), used
without its linker.

The linker of CA78K0R's free edition (lk78k0r) stops at 64 KiB, and the syscon's programs
are larger. Its assembler resolves every reference itself once every segment is absolute
(CSEG/DSEG AT(address)), so the segments the compiler writes are placed here, by segment
and label, and each source is assembled into its final bytes:

  cc(src, asm, chip)              C to assembly (cc78k0r)
  Module(text)                    an assembly source's segments, labels, EXTRN and PUBLIC
  Module.absolute(place, extern)  the source with every segment at an address
  assemble(asm, chip)             the bytes at each address (ra78k0r's listing)

  python3 tools/ca78k0r.py cc -c CHIP SRC.c -o OUT.asm [-I DIR] [-D NAME[=VALUE]] [options]
  python3 tools/ca78k0r.py as -c CHIP SRC.asm -o OUT.prn

CA78K0R is a Windows program (host.py runs it); it is found in $CA78K0R, else in
<PSP2_TOOLCHAINS>/ca78k0r-1.72 (unpacked by toolchains/unpack-ca78k0r.py).
"""
from pathlib import Path
import os
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import host  # noqa: E402

VERSION = '1.72'


class CAError(Exception):
	"""CA78K0R reported errors."""


def toolchain():
	if os.environ.get('CA78K0R'):
		return Path(os.environ['CA78K0R'])
	return host.toolchains() / f'ca78k0r-{VERSION}'


def win(p):
	return host.windows_path(p)


BANNER = re.compile(r'^\s*$|Copyright|for RL78|Microcontroller|C Compiler V|Assembler V|'
		    r'Pass\d Start|Target chip|Device file|Compilation complete|Assembly complete')


def run(prog, args, cwd):
	"""Run a CA78K0R program; its messages (the banner left out), or CAError."""
	exe = toolchain() / 'Bin' / f'{prog}.exe'
	if not exe.exists():
		raise host.HostError(f'CA78K0R is not found: {exe} (toolchains/unpack-ca78k0r.py)')
	p = subprocess.run(host.windows_command([exe, *args]), cwd=cwd, stdin=subprocess.DEVNULL,
			   capture_output=True, env=dict(os.environ, WINEDEBUG='-all'))
	out = (p.stdout + p.stderr).decode('latin-1').replace('\r', '')
	errors = re.search(r'complete,\s*(\d+) error', out)
	messages = '\n'.join(line for line in out.splitlines() if not BANNER.search(line))
	if p.returncode or not errors or errors.group(1) != '0':
		raise CAError(f'{prog} {args[-1]}:\n{messages or out}')
	return messages


def cc(src, asm, chip, options=(), includes=(), defines=()):
	"""Compile src to the assembly source asm (no object, no debug information)."""
	tc, asm = toolchain(), Path(asm)
	asm.parent.mkdir(parents=True, exist_ok=True)
	args = [f'-c{chip}', f'-y{win(tc / "Dev78k0r")}', f'-i{win(tc / "Inc78k0r")}',
		*(f'-i{win(i)}' for i in includes), *(f'-d{d}' for d in defines),
		'-no', '-ng', f'-a{win(asm)}', f'-t{win(asm.parent)}', *options, win(src)]
	return run('cc78k0r', args, asm.parent)


SEGMENT = re.compile(r'^([A-Za-z_?@][\w?@]*)\s+(CSEG|DSEG|BSEG)\b\s*([^;]*?)\s*(;.*)?$', re.I)
LABEL = re.compile(r'^([A-Za-z_?@][\w?@]*):')
SYMBOLS = re.compile(r'^\s+(EXTRN|EXTBIT|PUBLIC)\s+([^;]*?)\s*(;.*)?$', re.I)
END = re.compile(r'^\s+END\b', re.I)


class Segment:
	def __init__(self, name, kind, attr):
		self.name, self.kind, self.attr = name, kind.upper(), attr
		self.lines = []

	@property
	def labels(self):
		return [m.group(1) for m in map(LABEL.match, self.lines) if m]

	def __repr__(self):
		return f'<{self.name} {self.kind} {self.attr}: {len(self.lines)} lines>'


class Module:
	"""An assembly source as the compiler writes it: header (NAME, $ controls), symbol
	declarations, then segments, each a directive followed by its lines."""

	def __init__(self, text):
		self.header, self.extrn, self.extbit, self.public, self.segments = [], [], [], [], []
		seg = None
		for line in text.replace('\r', '').splitlines():
			if END.match(line):
				break
			m = SEGMENT.match(line)
			if m:
				seg = Segment(*m.groups()[:3])
				self.segments.append(seg)
				continue
			m = SYMBOLS.match(line)
			if m:
				names = [n.strip() for n in m.group(2).split(',') if n.strip()]
				getattr(self, m.group(1).lower()).extend(names)
				continue
			(seg.lines if seg else self.header).append(line)

	def defined(self):
		return {label for seg in self.segments for label in seg.labels}

	def absolute(self, place, extern):
		"""The source with every segment absolute. place(segment, label) is the address of
		segment's contents from label on, or None to continue the code before it;
		place(segment, None) the address of a segment's contents before any placed label.
		extern(name) is the address of an EXTRN symbol, defined here as a label."""
		out, n = list(self.header), 0
		out += [f'\tPUBLIC\t{p}' for p in self.public]
		data = []
		for name in self.extrn:
			address = extern(name)
			kind = 'DSEG' if address >= 0xf0000 else 'CSEG'
			out += [f'@@E{n}\t{kind}\tAT(0{address:X}H)', f'{name}:']
			n += 1
			if kind == 'DSEG':
				data.append(re.escape(name))
		# the compiler takes a RAM symbol's word as an immediate (movw de,#_@SEGAX), which
		# the linker resolves to its low word; placed here, the assembler wants loww
		near = re.compile(r'#(%s)(?![\w?@])' % '|'.join(data)) if data else None
		if self.extbit:
			raise CAError(f'EXTBIT not supported yet: {", ".join(self.extbit)}')
		for seg in self.segments:
			opened = False
			for line in seg.lines:
				m = LABEL.match(line)
				address = place(seg, m.group(1)) if m else None
				if address is None and not opened and line.strip() and \
				   not line.lstrip().startswith(';'):
					address = place(seg, None)
					if address is None:
						raise CAError(f'no address for segment {seg.name}')
				if address is not None:
					out.append(f'@@A{n}\t{seg.kind}\tAT(0{address:X}H)')
					n, opened = n + 1, True
				if near and seg.kind == 'CSEG':
					line = near.sub(r'#loww (\1)', line)
				out.append(line)
		out.append('\tEND')
		return '\n'.join(out) + '\n'


class Statement:
	"""a source line: its address (None: none), bytes and text; segment: a segment directive"""

	def __init__(self, line, address, data, source, segment=False):
		self.line, self.address, self.data, self.source = line, address, data, source
		self.segment = segment

	def __repr__(self):
		address = '-----' if self.address is None else f'{self.address:05X}'
		return f'<{self.line}: {address} {self.data.hex()} {self.source.strip()}>'


class Listing:
	"""ra78k0r's assemble list: every statement with its address and bytes, the labels'
	addresses and the segments."""

	MAIN = re.compile(r'^ *\d+ +(\d+) (.{5})  (.{8})(.{6})(.*)$')
	MORE = re.compile(r'^ {12}([0-9A-F]{5})  ([0-9A-F]+)(?:\s.*)?$')
	SEG = re.compile(r'^([0-9A-F]{5}) ([0-9A-F]{5})H +(\S+)\s*$')

	def __init__(self, text):
		self.statements, self.labels, self.segments = [], {}, []
		st = None
		for line in text.replace('\r', '').splitlines():
			# a statement's further bytes, with the rest of its source when that wrapped
			m = self.MORE.match(line)
			if m and st:
				st.data += bytes.fromhex(m.group(2))
				continue
			m = self.MAIN.match(line.ljust(33))
			if m:
				no, adrs, obj, _, source = m.groups()
				segment = adrs == '-----'
				address = int(adrs, 16) if adrs.strip() and not segment else None
				# an EQU shows its value, (VALUE), where the bytes go
				data = b'' if obj.lstrip().startswith('(') else bytes.fromhex(obj.strip())
				st = Statement(int(no), address, data, source, segment)
				self.statements.append(st)
				label = LABEL.match(source)
				if label and address is not None:
					self.labels[label.group(1)] = address
				continue
			m = self.SEG.match(line)
			if m:
				self.segments.append((m.group(3), int(m.group(1), 16), int(m.group(2), 16)))

	def image(self):
		"""{address: byte} of everything assembled"""
		out = {}
		for st in self.statements:
			for i, b in enumerate(st.data):
				out[st.address + i] = b
		return out


def assemble(asm, chip, prn=None):
	"""Assemble asm (no object); its Listing."""
	tc, asm = toolchain(), Path(asm)
	prn = Path(prn) if prn else asm.with_suffix('.prn')
	run('ra78k0r', [f'-c{chip}', f'-y{win(tc / "Dev78k0r")}', '-no', f'-p{win(prn)}',
			f'-t{win(prn.parent)}', win(asm)], asm.parent)
	return Listing(prn.read_bytes().decode('latin-1'))


def main():
	import argparse
	ap = argparse.ArgumentParser(description='CA78K0R without its linker')
	ap.add_argument('command', choices=('cc', 'as'))
	ap.add_argument('source')
	ap.add_argument('-c', dest='chip', required=True, help='chip (f1030a0)')
	ap.add_argument('-o', dest='out', required=True)
	ap.add_argument('-I', dest='includes', action='append', default=[])
	ap.add_argument('-D', dest='defines', action='append', default=[])
	args, options = ap.parse_known_args()
	try:
		if args.command == 'cc':
			messages = cc(args.source, args.out, args.chip, options, args.includes, args.defines)
		else:
			assemble(args.source, args.chip, args.out)
			messages = ''
	except CAError as e:
		sys.exit(str(e))
	if messages:
		print(messages, file=sys.stderr)


if __name__ == '__main__':
	main()
