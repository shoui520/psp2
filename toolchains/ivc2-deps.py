#!/usr/bin/env python3
"""
Lets GCC 4.5.4 schedule IVC2 intrinsics: patches gcc/config/mep/intrinsics.md.

cgen marks most IVC2 instructions VOLATILE because its descriptions name the
control registers an instruction writes (as dummy sets) but not the ones it
reads (accumulators, CSAR0/1, CC, overflow and permute registers).  GCC turns
VOLATILE into unspec_volatile, a full scheduling barrier, so MAC code comes
out one coprocessor operation per bundle.

Toshiba's IVC2 User's Manual (MEPUM08H04-E12, 5.4 Table 8) lists every
instruction's read and write resources.  For each volatile intrinsic whose
effects are all registers, this adds the control registers it reads as
operands of its unspecs (and any write cgen left out as a set), and makes
the unspecs plain.  Overflow flags are sticky, so their writes also read
them.  Loads, stores, branches, cmovc (its register is an operand) and
anything the table does not list stay volatile.

usage: ivc2-deps.py <IVC2 User Manual as pdftotext -layout> <intrinsics.md> <out>
"""
import re
import sys

FIRST_CCR = 80
RESOURCES = {
	'CSAR0': [0], 'CC': [1], 'COFR0': [4], 'COFR1': [5], 'COFA0': [6], 'COFA1': [7],
	'CPERM0': [8], 'CPERM1': [9], 'CPERM2': [10], 'CPERM3': [11], 'CSAR1': [15],
	'ACC0': list(range(16, 24)), 'ACC1': list(range(24, 32)),
}
STICKY = {'COFR0', 'COFR1', 'COFA0', 'COFA1'}
GP = {'CRo', 'CRq', 'CRp', 'CRn'}


def table(text):
	lines = text.split('\n')
	start = next(i for i, l in enumerate(lines) if 'Table 8. Read Resources' in l and i > 1000)
	rows = {}
	for line in lines[start:]:
		if line.lstrip().startswith('Other than the Instructions shown'):
			break
		m = re.match(r'^\s*((?:c3|p0s|p0|p1)(?:/(?:c3|p0s|p0|p1))*)\s+([A-Z][A-Z0-9.]+)(.*)$', line)
		if not m:
			continue
		cols = [c for c in re.split(r'\s{2,}|(?<=[^,\s]) (?=\S)', m.group(3).strip()) if c]
		if len(cols) == 2:
			cols = [''] + cols
		if len(cols) == 3:
			rows.setdefault(m.group(2), []).append(cols)
	return rows


def resources(column):
	"""Control registers named in a column; None if it names anything else."""
	regs = set()
	for item in filter(None, (c.strip() for c in column.split(','))):
		name = re.sub(r'\(X\d\)$', '', item)
		if name in GP:
			continue
		if name not in RESOURCES:
			return None, None
		regs.update(RESOURCES[name])
	names = {re.sub(r'\(X\d\)$', '', c.strip()) for c in column.split(',') if c.strip()}
	return regs, names


def duplicate(vector):
	"""The vector again, its operands as match_dups."""
	return re.sub(r'\(match_operand:\w+ (\d+) "[^"]*" "[^"]*"\)', r'(match_dup \1)', vector)


def patch(block, rows, stats):
	m = re.search(r'^  "([a-z0-9.]+)', block, re.M)
	if 'unspec_volatile' not in block or not m:
		return block
	entries = rows.get(m.group(1).upper())
	if not entries:
		stats['unlisted'].add(m.group(1))
		return block
	# rows that differ by slot: their union
	read = ','.join(e[1] for e in entries)
	write = ','.join(e[2] for e in entries)
	reads, _ = resources(read)
	writes, written = resources(write)
	if reads is None or writes is None:
		stats['other'].add(m.group(1))
		return block
	for name in written & STICKY:
		reads.update(RESOURCES[name])

	sets = {int(r) - FIRST_CCR for r in re.findall(r'\(set \(reg:SI (\d+)\)', block)}
	uses = ''.join('\n          (reg:SI %d)' % (FIRST_CCR + r) for r in sorted(reads))
	missing = sorted(writes - sets)
	bare = re.search(r'\[\(unspec_volatile \[(.*?)\] (\d+)\)\]', block, re.S)
	if bare:
		# no output at all: the effect is the registers it writes
		if not missing:
			stats['other'].add(m.group(1))
			return block
		vector = bare.group(1).rstrip() + uses
		block = block.replace(bare.group(0), '[' + '\n   '.join(
			'(set (reg:SI %d)\n        (unspec:SI [%s\n        ] %s))'
			% (FIRST_CCR + r, vector if i == 0 else duplicate(vector), bare.group(2))
			for i, r in enumerate(missing)) + ']')
	else:
		block = re.sub(r'(unspec_volatile:(\w+) \[)(\s*\(const_int 0\))?',
			lambda u: u.group(1) + (uses if uses else (u.group(3) or '')), block)
		if missing:
			vector = re.search(r'unspec_volatile:\w+ \[(.*?)\] (\d+)\)', block, re.S)
			extra = ''.join('\n   (set (reg:SI %d)\n        (unspec:SI [%s] %s))'
				% (FIRST_CCR + r, duplicate(vector.group(1)), vector.group(2)) for r in missing)
			block = re.sub(r'\)\]\n(  "CGEN_ENABLE)', ')' + extra + r']\n\1', block, count=1)
	stats['patched'].add(m.group(1))
	return block.replace('unspec_volatile', 'unspec')


def main():
	rows = table(open(sys.argv[1]).read())
	source = open(sys.argv[2]).read()
	stats = {'patched': set(), 'unlisted': set(), 'other': set()}
	parts = re.split(r'(?=^\(define_)', source, flags=re.M)
	out = ''.join(patch(p, rows, stats) if p.startswith('(define_insn') else p for p in parts)
	open(sys.argv[3], 'w').write(out)
	for key in ('patched', 'unlisted', 'other'):
		print('%s: %d %s' % (key, len(stats[key]), ' '.join(sorted(stats[key])) if key != 'patched' else ''))


main()
