#!/usr/bin/env python3
"""An assembler for the SrcMix processor, the audio DSP of Kermit's SRC bus.

    srcmixasm.py OUTPUT SOURCE

OUTPUT gets the program as the SrcMix loads it: 32-bit little-endian words, each holding
two instruction halfwords, the lower address in the low half.

No document of Sony's describes the processor, so the syntax is this project's. What each
instruction does was read from the programs SceAudio uploads; NOTES on the decoding are in
the source's header. The encoding:

  16-bit instructions (top nibble 0-d, and e4, ea, ef):
    0000                nop
    0001-5fff           mu.XX OPND     multiplier: XX = bits 14:7, OPND = bits 6:0
                                       (mpy = mu.80, mac = mu.88)
    6000-9fff           ALU: bits 15:12 class 6-9, 11:10 op, 9:8 A, 7:6 B, 5:0 immediate
                                       A: 1 r08 r02 r03, B: 0 r08 r02 r03; the result is r08
    a000-dfff           move DST, SRC  DST = bits 13:7 (from a000), SRC = bits 6:0
    e400                jmp (jt)
    eaXX                reti           (ea00; others reti.XX)
    efXX                ef.XX

  32-bit instructions (top nibble e or f, other than e4, ea, ef), the second halfword IMM:
    e0CC IMM            b.CC LABEL     (e000: jmp LABEL)
    e1CC IMM            bd.CC LABEL    a branch with one delay slot (e100: jd LABEL)
    e2CC IMM            call LABEL     (e200; others call.CC)
    e9CC MOVE           ret | MOVE     (e900; others ret.CC)
    ecNN IMM            rep NN, LABEL  repeat up to LABEL NN times
    edCC IMM            repr.CC LABEL  repeat up to LABEL a register-held number of times
    eeCC MOVE           ee.CC | MOVE
    f0RR IMM            ldi R, IMM
    f1RR IMM            bset R, IMM    (RR bit 7 set: bclr R, IMM)
    f2RR IMM            ld R, x:IMM    (RR bit 7 set: y memory)
    f3RR IMM            st x:IMM, R    (RR bit 7 set: y memory)
    f4-f6 HI IMM        ldi24 r02|r03|r08, HI << 16 | IMM
    f8-fd AAAB BB       ldi2 P, 0xAAA, Q, 0xBBB   two address registers at once:
                                       f8 r12/r1a, f9 r13/r1b, fa r14/r1c, fb r15/r1d,
                                       fc r16/r1e, fd r17/r1f
    fe AAAB BB          ld2 r02, x:0xAAA, r03, y:0xBBB

  Operands (7 bits): r00-r1f registers; 0x20-0x3f memory through a pointer, written [rP]
  with an update suffix (+ increment, - decrement, none, or mN for update N); 0x40-0x7f the
  special registers, written rXX or by name (cmd, port, sr, jt).

A source line is a label (NAME:), an instruction, or a directive: .equ NAME, VALUE. A
comment runs from ';' to the end of the line. Numbers are decimal or 0x hex; a NAME
defined by .equ or as a label may stand for any number.
"""
import re
import struct
import sys
from pathlib import Path

SPECIAL = {0x64: 'cmd', 0x65: 'port', 0x66: 'sr', 0x5b: 'jt'}
SPECIAL_BY_NAME = {v: k for k, v in SPECIAL.items()}
UPDATES = {'': 1, '+': 2, '-': 3}
MU_ALIASES = {'mpy': 0x80, 'mac': 0x88}
ALU_NAMES = {(6, 0): 'add', (6, 2): 'sub', (7, 0): 'cmp', (8, 2): 'and'}
ALU_BY_NAME = {v: k for k, v in ALU_NAMES.items()}
ALU_A = ['1', 'r08', 'r02', 'r03']
ALU_B = ['0', 'r08', 'r02', 'r03']
LDI24 = ['r02', 'r03', 'r08']
LDI2 = [('r12', 'r1a'), ('r13', 'r1b'), ('r14', 'r1c'), ('r15', 'r1d'), ('r16', 'r1e'),
	('r17', 'r1f')]


class AsmError(Exception):
	pass


def is_long(h):
	"""Two halfwords: the first's top nibble is e or f, other than e4, ea and ef."""
	return h >> 12 in (0xe, 0xf) and h >> 8 not in (0xe4, 0xea, 0xef)


class Assembler:
	def __init__(self):
		self.symbols = {}

	def number(self, text):
		text = text.strip()
		if text in self.symbols:
			return self.symbols[text]
		try:
			return int(text, 0)
		except ValueError:
			raise AsmError(f'not a number or symbol: {text!r}')

	def register(self, text):
		"""A register of the 7-bit space by name (rXX or a special's name)."""
		text = text.strip()
		if text in SPECIAL_BY_NAME:
			return SPECIAL_BY_NAME[text]
		m = re.fullmatch(r'r([0-9a-f]{2})', text)
		if not m or int(m.group(1), 16) >= 0x80:
			raise AsmError(f'not a register: {text!r}')
		return int(m.group(1), 16)

	def operand(self, text):
		"""A 7-bit operand: a register, or memory through a pointer with its update."""
		text = text.strip()
		m = re.fullmatch(r'\[r(1[23ab])\](\+|-|m[0-7]|)', text)
		if not m:
			return self.register(text)
		pointer = int(m.group(1), 16)
		update = int(m.group(2)[1]) if m.group(2).startswith('m') else UPDATES[m.group(2)]
		return 0x20 | (pointer >= 0x1a) << 4 | (pointer & 1) << 3 | update

	def memory(self, text):
		"""x:ADDR or y:ADDR: the y flag and the address."""
		m = re.fullmatch(r'([xy]):(.+)', text.strip())
		if not m:
			raise AsmError(f'not a memory address: {text!r}')
		return m.group(1) == 'y', self.number(m.group(2))

	def move(self, text):
		m = re.fullmatch(r'move\s+(.+?),\s*(.+)', text.strip())
		if not m:
			raise AsmError(f'not a move: {text!r}')
		return 0xa000 + (self.operand(m.group(1)) << 7) + self.operand(m.group(2))

	def encode(self, mnemonic, args, text):
		"""The halfwords of one instruction."""
		def field(v, bits, what):
			if not 0 <= v < 1 << bits:
				raise AsmError(f'{what} out of range: {v:#x}')
			return v

		def cc(base):
			return base | field(int(suffix, 16), 8, 'condition') if suffix else base

		name, _, suffix = mnemonic.partition('.')
		n = len(args)
		if mnemonic == 'nop' and n == 0:
			return [0x0000]
		if name in MU_ALIASES and not suffix and n == 1:
			return [MU_ALIASES[name] << 7 | self.operand(args[0])]
		if name == 'mu' and n == 1:
			op = field(int(suffix, 16), 8, 'multiplier op')
			h = op << 7 | self.operand(args[0])
			if not 0 < h < 0x6000:
				raise AsmError(f'not a multiplier encoding: {text!r}')
			return [h]
		alu = ALU_BY_NAME.get(mnemonic)
		if alu is None:
			m = re.fullmatch(r'alu([6-9])\.([0-3])', mnemonic)
			alu = (int(m.group(1)), int(m.group(2))) if m else None
		if alu is not None and n in (2, 3):
			a, b = (args[1], args[0]) if mnemonic == 'sub' else (args[0], args[1])
			if a.strip() not in ALU_A or b.strip() not in ALU_B:
				raise AsmError(f'ALU operands: {text!r}')
			imm = field(self.number(args[2].lstrip(' #')), 6, 'ALU immediate') if n == 3 else 0
			return [alu[0] << 12 | alu[1] << 10 | ALU_A.index(a.strip()) << 8 |
				ALU_B.index(b.strip()) << 6 | imm]
		if mnemonic == 'move' and n == 2:
			return [self.move(text)]
		if mnemonic == 'jmp' and n == 1 and args[0].strip() == '(jt)':
			return [0xe400]
		if name == 'reti' and n == 0:
			return [cc(0xea00)]
		if name == 'ef' and n == 0:
			return [cc(0xef00)]
		if mnemonic == 'jmp' and n == 1:
			return [0xe000, self.target(args[0])]
		if mnemonic == 'jd' and n == 1:
			return [0xe100, self.target(args[0])]
		if name in ('b', 'bd') and suffix and n == 1:
			return [cc(0xe000 if name == 'b' else 0xe100), self.target(args[0])]
		if name == 'call' and n == 1:
			return [cc(0xe200), self.target(args[0])]
		if name in ('ret', 'ee') and '|' in text:
			return [cc(0xe900 if name == 'ret' else 0xee00), self.move(text.split('|', 1)[1])]
		if mnemonic == 'rep' and n == 2:
			return [0xec00 | field(self.number(args[0]), 8, 'repeat count'), self.target(args[1])]
		if name == 'repr' and suffix and n == 1:
			return [cc(0xed00), self.target(args[0])]
		if mnemonic == 'ldi' and n == 2:
			return [0xf000 | self.register(args[0]), field(self.number(args[1]), 16, 'immediate')]
		if mnemonic in ('bset', 'bclr') and n == 2:
			r = self.register(args[0]) | (mnemonic == 'bclr') << 7
			return [0xf100 | r, field(self.number(args[1]), 16, 'immediate')]
		if mnemonic == 'ld' and n == 2:
			y, addr = self.memory(args[1])
			return [0xf200 | self.register(args[0]) | y << 7, field(addr, 16, 'address')]
		if mnemonic == 'st' and n == 2:
			y, addr = self.memory(args[0])
			return [0xf300 | self.register(args[1]) | y << 7, field(addr, 16, 'address')]
		if mnemonic == 'ldi24' and n == 2:
			if args[0].strip() not in LDI24:
				raise AsmError(f'ldi24 register: {text!r}')
			v = field(self.number(args[1]) & 0xffffff, 24, 'immediate')
			return [(0xf4 + LDI24.index(args[0].strip())) << 8 | v >> 16, v & 0xffff]
		if mnemonic == 'ldi2' and n == 4:
			pair = (args[0].strip(), args[2].strip())
			if pair not in LDI2:
				raise AsmError(f'ldi2 registers: {text!r}')
			return self.pair(0xf8 + LDI2.index(pair), self.number(args[1]), self.number(args[3]))
		if mnemonic == 'ld2' and n == 4:
			if (args[0].strip(), args[2].strip()) != ('r02', 'r03'):
				raise AsmError(f'ld2 registers: {text!r}')
			return self.pair(0xfe, self.xy(args[1], False), self.xy(args[3], True))
		raise AsmError(f'unknown instruction: {text!r}')

	def xy(self, text, want_y):
		y, addr = self.memory(text)
		if y != want_y:
			raise AsmError(f'expected {"y" if want_y else "x"} memory: {text!r}')
		return addr

	@staticmethod
	def pair(op, a, b):
		if not (0 <= a < 0x1000 and 0 <= b < 0x1000):
			raise AsmError(f'12-bit addresses out of range: {a:#x}, {b:#x}')
		v = a << 12 | b
		return [op << 8 | v >> 16, v & 0xffff]

	def target(self, text):
		if self.sizing:
			return 0
		return self.number(text)

	def statements(self, path):
		"""(line number, label or None, mnemonic, arguments, text) for each line."""
		for number, line in enumerate(Path(path).read_text().splitlines(), 1):
			line = line.split(';', 1)[0].strip()
			while line:
				m = re.match(r'([A-Za-z_][\w.]*):\s*', line)
				if not m:
					break
				yield number, m.group(1), None, [], ''
				line = line[m.end():]
			if not line:
				continue
			mnemonic, _, rest = line.partition(' ')
			main = rest.split('|', 1)[0]
			args = [a for a in re.split(r',(?![^\[]*\])', main) if a.strip()] if main.strip() else []
			yield number, None, mnemonic, args, line

	def assemble(self, path):
		lines = list(self.statements(path))
		for self.sizing in (True, False):
			pc, out = 0, []
			for number, label, mnemonic, args, text in lines:
				try:
					if label:
						if self.sizing:
							if label in self.symbols:
								raise AsmError(f'{label} defined twice')
							self.symbols[label] = pc
						continue
					if mnemonic == '.equ':
						if self.sizing:
							self.symbols[args[0].strip()] = self.number(args[1])
						continue
					halfwords = self.encode(mnemonic, args, text)
					if (len(halfwords) == 2) != is_long(halfwords[0]):
						raise AsmError(f'length does not match the encoding: {text!r}')
					out += halfwords
					pc += len(halfwords)
				except AsmError as e:
					sys.exit(f'{path}:{number}: {e}')
		return out


def main():
	if len(sys.argv) != 3:
		sys.exit(__doc__)
	halfwords = Assembler().assemble(sys.argv[2])
	if len(halfwords) % 2:
		halfwords.append(0x0000)
	words = [halfwords[i] | halfwords[i + 1] << 16 for i in range(0, len(halfwords), 2)]
	Path(sys.argv[1]).write_bytes(struct.pack(f'<{len(words)}I', *words))


if __name__ == '__main__':
	main()
