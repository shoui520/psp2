"""The link of a syscon program (ernie), which CA78K0R's free-edition linker cannot do (it stops
at 64 KiB): every object compiled or taken as it is, its segments placed by the program's link
file, each assembled at its final addresses (tools/ca78k0r.py), and the bytes written out.

    python3 tools/lk78k0r.py -c CHIP LINKFILE -o OUT.hex [--map OUT.map] [--work DIR]
	[-I DIR] [-D NAME[=VALUE]] [--flags "OPTIONS"]

The link file (ernie/<program>/<program>.lk), one directive a line, `#` to the end of a line
a comment; paths are relative to the link file's folder:

  OBJECT path		an object, in link order: a C source (compiled with -I, -D and
			--flags) or an assembly source
  SYMBOL name address	a symbol the linker defines (the compiler's runtime registers)
  DROP name		every object's segment of that name is left out
  AT address		the location the next segments go to
  SEG name [ALIGN n] [path...]
			every object's segment of that name not placed yet, in link order
			(or only those of the objects named, in that order), one after the other from the
			location on, each at a multiple of n (default: 2 for a segment the
			assembler aligns, a P attribute, else 1); the location moves past them
  RESERVE address size	memory no segment may take (what code uses by absolute address)
  FILL byte		the byte the gaps between a SEG's segments and an AT's are filled with
			(default none: only what is assembled is written)

Any directive may end in `IF NAME==VALUE` or `IF NAME!=VALUE`: it applies only when the -D
definition NAME has (or has not) that value (numbers compared as numbers), as a board's own
objects and places.

A segment with a 64K attribute (UNIT64KP, PAGE64KP) stays in one 64 KiB page: one that would
cross into the next page starts there, and the segments after it that fit fill the rest of
the page left behind, as CA78K0R's linker does.

A segment with an AT attribute of its own goes there, and an object's segment no SEG names is
an error, as are a symbol no object or SYMBOL defines and segments that overlap each other
or a RESERVE. The output is Intel HEX of every byte
the program places; --map lists every segment and public symbol with its address.
"""
from pathlib import Path
import argparse
import os
import re
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ca78k0r  # noqa: E402

ATTR_AT = re.compile(r'\bAT\s*\(?\s*0*([0-9A-F]+)H\s*\)?', re.I)
SADDR = re.compile(r'\bSADDRP?\b', re.I)
NEAR = re.compile(r'\bBASEP?\b|\bCALLT0\b|\bSADDRP?\b', re.I)


class LinkError(Exception):
	"""The link cannot be made."""


def number(text):
	return int(text, 0)


def same(value, text):
	"""a -D definition's value is text (as numbers when both are)"""
	try:
		return value is not None and number(value) == number(text)
	except ValueError:
		return value == text


class LinkFile:
	"""a link file's directives"""

	def __init__(self, path, defines=()):
		self.path, self.objects, self.symbols, self.drop, self.steps = Path(path), [], {}, set(), []
		self.reserved = []
		values = dict(d.split('=', 1) if '=' in d else (d, '1') for d in defines)
		for no, line in enumerate(Path(path).read_text().splitlines(), 1):
			words = line.split('#', 1)[0].split()
			if len(words) > 2 and words[-2].upper() == 'IF':
				m = re.fullmatch(r'(\w+)(==|!=)(\S+)', words[-1])
				if not m:
					raise LinkError(f'{path}:{no}: {line.strip()}')
				if (same(values.get(m.group(1)), m.group(3))) != (m.group(2) == '=='):
					continue
				words = words[:-2]
			if not words:
				continue
			op, args = words[0].upper(), words[1:]
			try:
				if op == 'OBJECT' and len(args) == 1:
					self.objects.append(args[0])
				elif op == 'SYMBOL' and len(args) == 2:
					self.symbols[args[0]] = number(args[1])
				elif op == 'DROP' and len(args) == 1:
					self.drop.add(args[0].upper())
				elif op == 'RESERVE' and len(args) == 2:
					self.reserved.append((number(args[0]), number(args[1])))
				elif op == 'AT' and len(args) == 1:
					self.steps.append(('AT', number(args[0])))
				elif op == 'FILL' and len(args) == 1:
					self.steps.append(('FILL', number(args[0])))
				elif op == 'SEG' and args:
					align = None
					if len(args) > 2 and args[1].upper() == 'ALIGN':
						align = number(args[2])
						del args[1:3]
					self.steps.append(('SEG', args[0].upper(), align, args[1:]))
				else:
					raise ValueError
			except ValueError:
				raise LinkError(f'{path}:{no}: {line.strip()}')
		unknown = {p for step in self.steps if step[0] == 'SEG' for p in step[3]} - set(self.objects)
		if unknown:
			raise LinkError(f'{path}: SEG names objects it does not list: {", ".join(sorted(unknown))}')


class Object:
	"""an object: its module, and each segment's size and labels' offsets"""

	def __init__(self, name, path, module):
		self.name, self.path, self.module = name, path, module
		self.size, self.offsets, self.base = {}, {}, {}

	def segments(self):
		return [seg for seg in self.module.segments]


def segment_at(seg):
	m = ATTR_AT.search(seg.attr)
	return int(m.group(1), 16) if m else None


def in_page(seg, start, size):
	"""whether the segment fits at start: a 64K attribute (UNIT64KP, PAGE64KP) keeps it in
	one 64 KiB page"""
	return '64K' not in seg.attr.upper() or not size or start >> 16 == start + size - 1 >> 16


def aligned(seg):
	"""2 for a segment the assembler wants at an even address (a P attribute), else 1"""
	words = seg.attr.upper().split()
	return 2 if words and words[0].endswith('P') else 1


DS = re.compile(r'^(?:[A-Za-z_?@][\w?@]*:)?\s*DS\s+\(?\s*(\w+)\s*\)?\s*(?:;.*)?$', re.I)


def reserved(seg):
	"""the bytes a data segment's DS lines reserve"""
	total = 0
	for line in seg.lines:
		m = DS.match(line)
		if m:
			v = m.group(1)
			total += int(v[:-1], 16) if v.upper().endswith('H') else int(v, 0)
	return total


def dummy_bases(obj):
	"""a base for each of the object's segments, apart from each other, in a region of the
	same kind, for measuring them"""
	out, code, near, ram, saddr = {}, 0x40000, 0x02000, 0xfcf00, 0xffe20
	for seg in obj.segments():
		if seg.kind == 'BSEG':
			raise LinkError(f'{obj.name}: bit segments not supported yet ({seg.name})')
		at = segment_at(seg)
		if at is not None:
			out[id(seg)] = at
		elif seg.kind == 'CSEG' and NEAR.search(seg.attr):
			out[id(seg)], near = near, near + 0x1000
		elif seg.kind == 'CSEG':
			out[id(seg)], code = code, code + 0x10000
		elif SADDR.search(seg.attr):
			out[id(seg)], saddr = saddr, saddr + reserved(seg) + 2
		else:
			out[id(seg)], ram = ram, ram + reserved(seg) + 2
	return out


def assemble(obj, place, extern, chip, work, tag):
	"""assemble the object with each segment at place[id(seg)]; its listing and each
	segment's listing name"""
	names, n = {}, [len(obj.module.extrn)]

	def at(seg, label):
		if label is not None:
			return None
		names[id(seg)] = f'@@A{n[0]}'
		n[0] += 1
		return place[id(seg)]

	asm = Path(work) / f'{obj.name.replace("/", "__")}.{tag}.asm'
	asm.write_text(obj.module.absolute(at, extern), encoding='latin-1')
	return ca78k0r.assemble(asm, chip), names


class Linker:
	def __init__(self, link, chip, work, includes, defines, flags):
		self.link, self.chip, self.work = link, chip, Path(work)
		self.includes, self.defines, self.flags = includes, defines, flags
		self.work.mkdir(parents=True, exist_ok=True)
		self.objects = []

	def load(self):
		root = self.link.path.parent
		for name in self.link.objects:
			path = root / name
			if path.suffix == '.c':
				asm = self.work / (name.replace('/', '__') + '.cc.asm')
				messages = ca78k0r.cc(path, asm, self.chip, self.flags, self.includes, self.defines)
				if messages:
					print(messages, file=sys.stderr)
				text = asm.read_text(encoding='latin-1')
			else:
				text = path.read_text(encoding='latin-1')
			module = ca78k0r.Module(text)
			dropped = [s for s in module.segments if self.dropped(s.name)]
			module.segments = [s for s in module.segments if s not in dropped]
			gone = {label for s in dropped for label in s.labels}
			module.public = [p for p in module.public if p not in gone]
			self.objects.append(Object(name, path, module))
		# who defines each public symbol, and in what kind of segment
		self.defined = {}
		for obj in self.objects:
			for seg in obj.segments():
				for label in seg.labels:
					if label in obj.module.public:
						if label in self.defined:
							raise LinkError(f'{label} is defined by {self.defined[label][0].name} and {obj.name}')
						self.defined[label] = (obj, seg)

	def dropped(self, name):
		return any(re.fullmatch(re.escape(d).replace('\\?', '.'), name.upper()) for d in self.link.drop)

	def dummy_extern(self, name):
		"""an address of the right kind for an external symbol, while measuring"""
		if name in self.link.symbols:
			return self.link.symbols[name]
		if name not in self.defined:
			raise LinkError(f'{name} is not defined by any object or SYMBOL')
		seg = self.defined[name][1]
		if seg.kind == 'CSEG':
			return 0x01000 if NEAR.search(seg.attr) or segment_at(seg) is not None else 0x30000
		return 0xffe20 if SADDR.search(seg.attr) else 0xfe000

	def measure(self):
		for obj in self.objects:
			place = dummy_bases(obj)
			listing, names = assemble(obj, place, self.dummy_extern, self.chip, self.work, 'size')
			sizes = {name: size for name, start, size in
				 ((n, s, z) for n, s, z in listing.segments)}
			for seg in obj.segments():
				obj.size[id(seg)] = sizes.get(names.get(id(seg)), 0)
				base = place[id(seg)]
			for seg in obj.segments():
				for label in seg.labels:
					if label in listing.labels:
						obj.offsets[label] = (seg, listing.labels[label] - place[id(seg)])

	def place(self):
		location, fill, self.fills, holes = None, None, [], {}
		# an empty segment is placed only for its labels (a startup's bounds)
		pending = {(obj.name, id(seg)): (obj, seg) for obj in self.objects for seg in obj.segments()
			   if obj.size[id(seg)] or seg.labels}
		for obj in self.objects:
			for seg in obj.segments():
				at = segment_at(seg)
				if at is not None:
					obj.base[id(seg)] = at
					pending.pop((obj.name, id(seg)), None)
		for step in self.link.steps:
			if step[0] == 'AT':
				if fill is not None and location is not None and step[1] > location:
					self.fills.append((location, step[1], fill))
				location = step[1]
			elif step[0] == 'FILL':
				fill = step[1]
			else:
				_, name, align, only = step
				if location is None:
					raise LinkError(f'SEG {name} before any AT')
				named = {obj.name: obj for obj in self.objects}
				for obj in [named[n] for n in only] if only else self.objects:
					for seg in obj.segments():
						if seg.name.upper() != name or (obj.name, id(seg)) not in pending:
							continue
						step_align = align or aligned(seg)
						size = obj.size[id(seg)]
						start = self.hole(holes.setdefault(name, []), size, step_align, seg)
						if start is None:
							start = -(-location // step_align) * step_align
							if not in_page(seg, start, size):
								# a segment kept in a 64 KiB page starts the next one, and
								# what is left of this one takes the segments that fit
								holes[name].append([location, start + 0xffff & ~0xffff])
								start = holes[name][-1][1]
							if fill is not None and start > location:
								self.fills.append((location, start, fill))
							location = start + size
						obj.base[id(seg)] = start
						del pending[(obj.name, id(seg))]
		if pending:
			left = sorted(f'{obj.name} {seg.name}' for obj, seg in pending.values())
			raise LinkError('segments no SEG places: ' + ', '.join(left))
		for obj in self.objects:
			for seg in obj.segments():
				obj.base.setdefault(id(seg), None)
		used = sorted([(start, size, 'RESERVE') for start, size in self.link.reserved] +
			      [(obj.base[id(seg)], obj.size[id(seg)], f'{obj.name} {seg.name}')
			       for obj in self.objects for seg in obj.segments()
			       if obj.base[id(seg)] is not None and obj.size[id(seg)]])
		for (a, n, what), (b, _, other) in zip(used, used[1:]):
			if a + n > b:
				raise LinkError(f'{what} ({a:05x}-{a + n - 1:05x}) overlaps {other} at {b:05x}')

	@staticmethod
	def hole(holes, size, align, seg):
		"""the start of the first hole the segment fits in, which it then fills from there"""
		for h in holes:
			start = -(-h[0] // align) * align
			if start + size <= h[1] and in_page(seg, start, size):
				h[0] = start + size
				return start
		return None

	def address(self, name):
		if name in self.link.symbols:
			return self.link.symbols[name]
		obj = self.defined[name][0]
		seg, offset = obj.offsets[name]
		return obj.base[id(seg)] + offset

	def build(self):
		image = {}
		for start, end, byte in self.fills:
			for a in range(start, end):
				image[a] = byte
		for obj in self.objects:
			place = {id(seg): obj.base[id(seg)] if obj.base[id(seg)] is not None else 0x40000
				 for seg in obj.segments()}
			listing, names = assemble(obj, place, self.address, self.chip, self.work, 'link')
			for seg in obj.segments():
				if obj.base[id(seg)] is None or seg.kind != 'CSEG':
					continue
			for st in listing.statements:
				if st.address is None or not st.data or st.address >= 0xf0000:
					continue
				for i, b in enumerate(st.data):
					a = st.address + i
					if a in image and image[a] != b and a not in self.filled(a):
						raise LinkError(f'{obj.name}: {a:05x} is written twice')
					image[a] = b
		return image

	def filled(self, a):
		return {a} if any(s <= a < e for s, e, _ in self.fills) else set()

	def map(self):
		lines = ['# segments: address, size, object, name']
		rows = []
		for obj in self.objects:
			for seg in obj.segments():
				if obj.base[id(seg)] is not None:
					rows.append((obj.base[id(seg)], obj.size[id(seg)], obj.name, seg.name))
		lines += [f'{a:05x}\t{s:05x}\t{o}\t{n}' for a, s, o, n in sorted(rows)]
		lines.append('# public symbols')
		syms = sorted((self.address(name), name) for name in self.defined)
		lines += [f'{a:05x}\t{n}' for a, n in syms]
		return '\n'.join(lines) + '\n'


def intel_hex(image):
	out, addresses = [], sorted(image)
	upper = None
	i = 0
	while i < len(addresses):
		start = addresses[i]
		run = [image[start]]
		while i + len(run) < len(addresses) and addresses[i + len(run)] == start + len(run) and \
		      len(run) < 16 and (start + len(run)) & 0xffff:
			run.append(image[start + len(run)])
		if start >> 16 != upper:
			upper = start >> 16
			rec = bytes([2, 0, 0, 4, upper >> 8, upper & 0xff])
			out.append(':' + rec.hex().upper() + f'{-sum(rec) & 0xff:02X}')
		rec = bytes([len(run), (start >> 8) & 0xff, start & 0xff, 0]) + bytes(run)
		out.append(':' + rec.hex().upper() + f'{-sum(rec) & 0xff:02X}')
		i += len(run)
	out.append(':00000001FF')
	return '\n'.join(out) + '\n'


def main():
	ap = argparse.ArgumentParser(description='link a syscon program (CA78K0R objects)')
	ap.add_argument('link')
	ap.add_argument('-c', dest='chip', required=True)
	ap.add_argument('-o', dest='out', required=True)
	ap.add_argument('--map')
	ap.add_argument('--work')
	ap.add_argument('-I', dest='includes', action='append', default=[])
	ap.add_argument('-D', dest='defines', action='append', default=[])
	ap.add_argument('--flags', default='')
	args = ap.parse_args()
	work = args.work or tempfile.mkdtemp(prefix='lk78k0r-')
	try:
		linker = Linker(LinkFile(args.link, args.defines), args.chip, work, args.includes, args.defines,
				args.flags.split())
		linker.load()
		linker.measure()
		linker.place()
		image = linker.build()
	except (LinkError, ca78k0r.CAError) as e:
		sys.exit(f'lk78k0r: {e}')
	Path(args.out).write_text(intel_hex(image))
	if args.map:
		Path(args.map).write_text(linker.map())


if __name__ == '__main__':
	main()
