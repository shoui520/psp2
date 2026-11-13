#!/usr/bin/env python3
"""An assembler for the MCUs inside Realtek's RTL8152/RTL8153 USB Ethernet controllers.

    rtlmcuasm.py OUTPUT NAME=SOURCE [NAME=SOURCE ...]

Each SOURCE is assembled into a program, and OUTPUT gets a C header with one array per
program, `static const unsigned char NAME[]`, holding the program as the driver uploads
it: 16-bit little-endian words from the patch RAM's start (0xf800).

The controller has two such MCUs, one behind the PLA (packet) register space and one
behind the USB register space. Realtek publishes no document of the processor, so the
syntax is this project's. What each instruction does was read from the programs the
driver uploads; the reading and what is still open are in the NOTES section of each
source. The encoding (one 16-bit word per instruction; PC counts words; a displacement is
counted from the instruction's own word):

    0Rii  0..7  addi rR, IMM        rR += IMM (8 bits)
    0Rii  8..f  subi rR, IMM        rR -= IMM
    1Rii  0..7  cmpi rR, IMM        compare rR with IMM
    1Rii  8..f  movi rR, IMM        rR = IMM (8 bits, zero-extended)
    2Hxx        op.2H F, rR, N      register and 4-bit immediate (H = 0..7);
                                    xx = F << 7 | R << 4 | N
    40-47 xx    alu.4H.M rD, rS     two registers: xx = M << 6 | D << 3 | S (M = 0..3);
                                    and = alu.46, or = alu.47
    48 xx       bset/bclr rR, BIT   xx = (clear) << 7 | R << 4 | BIT
    49 xx       btst rR, BIT        xx = 1 << 7 | R << 4 | BIT
    6R/7R xx    ld.b/ld.w rR, [rB+OFF]   xx = B << 5 | OFF (bytes, 0..31)
    8R/9R xx    st.b/st.w rR, [rB+OFF]   R = 8..f: the register is R - 8
    9R xx       st.bh rR, [rB+OFF]  R = 0..7: the high byte of rR
    aH xx       op.aH N, rP         access through pointer rP: xx = N << 3 | P
    b0 0R       pop rR
    b4 0R       push rR
    b8+R 00     jmp rR
    c0+R dd     ldl rR, LABEL       rR = the word at LABEL (PC + dd, signed 8 bits)
    e0-e7       bra LABEL           11-bit signed displacement
    e8-ef       call LABEL          11-bit signed displacement
    f0 dd       beq LABEL           (the bit tested, or the compare, gave equal/zero)
    f1 dd       bne LABEL
    fC dd       b.C LABEL           other conditions, C = 2..e
    ff80        ret

A source line is a label (NAME:), an instruction, or a directive: `.equ NAME, VALUE` or
`.word VALUE[, VALUE...]`. A comment runs from ';' to the end of the line. Numbers are
decimal or 0x hex; a NAME defined by .equ or as a label may stand for any number, and
`NAME + N` / `NAME - N` are allowed. Registers are r0-r7.
"""
import re
import sys
from pathlib import Path

ALU4 = {'and': 0x46, 'or': 0x47}
BRANCH_COND = {'beq': 0xf0, 'bne': 0xf1}


class AsmError(Exception):
    pass


def reg(text):
    m = re.fullmatch(r'r([0-7])', text.strip())
    if not m:
        raise AsmError('not a register: %r' % text)
    return int(m.group(1))


def split_operands(text):
    out, depth, cur = [], 0, ''
    for ch in text:
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


class Assembler:
    def __init__(self, path):
        self.path = path
        self.symbols = {}
        self.lines = []

    def value(self, text):
        text = text.strip()
        m = re.fullmatch(r'(\w+)\s*([+-])\s*(\w+)', text)
        if m:
            a, b = self.value(m.group(1)), self.value(m.group(3))
            return a + b if m.group(2) == '+' else a - b
        if re.fullmatch(r'0x[0-9a-fA-F]+', text):
            return int(text, 16)
        if re.fullmatch(r'-?[0-9]+', text):
            return int(text)
        if text in self.symbols:
            return self.symbols[text]
        raise AsmError('undefined: %r' % text)

    def parse(self):
        pc = 0
        for number, raw in enumerate(Path(self.path).read_text().splitlines(), 1):
            line = raw.split(';', 1)[0].strip()
            while True:
                m = re.match(r'(\w+):\s*(.*)', line)
                if not m:
                    break
                if m.group(1) in self.symbols:
                    raise AsmError('%s:%d: %s defined twice' % (self.path, number, m.group(1)))
                self.symbols[m.group(1)] = pc
                line = m.group(2).strip()
            if not line:
                continue
            m = re.fullmatch(r'\.equ\s+(\w+)\s*,\s*(.+)', line)
            if m:
                self.symbols[m.group(1)] = self.value(m.group(2))
                continue
            op, _, rest = line.partition(' ')
            operands = split_operands(rest)
            size = len(operands) if op == '.word' else 1
            self.lines.append((number, pc, op, operands))
            pc += size

    def mem(self, text):
        m = re.fullmatch(r'\[\s*(r[0-7])\s*(?:\+\s*(.+?))?\s*\]', text)
        if not m:
            raise AsmError('not a memory operand: %r' % text)
        offset = self.value(m.group(2)) if m.group(2) else 0
        if not 0 <= offset <= 31:
            raise AsmError('offset out of range: %d' % offset)
        return reg(m.group(1)), offset

    def disp(self, target, pc, bits):
        d = self.value(target) - pc
        if not -(1 << (bits - 1)) <= d < (1 << (bits - 1)):
            raise AsmError('%s out of range' % target)
        return d & ((1 << bits) - 1)

    def encode(self, pc, op, a):
        def imm8(text):
            v = self.value(text)
            if not 0 <= v <= 0xff:
                raise AsmError('immediate out of range: %r' % text)
            return v

        if op == 'ret':
            return 0xff80
        if op in ('addi', 'subi', 'cmpi', 'movi'):
            base = {'addi': 0x00, 'subi': 0x08, 'cmpi': 0x10, 'movi': 0x18}[op]
            return (base + reg(a[0])) << 8 | imm8(a[1])
        m = re.fullmatch(r'op\.2([0-7])', op)
        if m:
            flag, r, n = self.value(a[0]), reg(a[1]), self.value(a[2])
            if flag not in (0, 1) or not 0 <= n <= 15:
                raise AsmError('bad operands for %s' % op)
            return (0x20 + int(m.group(1))) << 8 | flag << 7 | r << 4 | n
        m = re.fullmatch(r'(and|or|alu\.4([0-7]))\.([0-3])', op)
        if m:
            high = ALU4[m.group(1)] if m.group(2) is None else 0x40 + int(m.group(2))
            return high << 8 | int(m.group(3)) << 6 | reg(a[0]) << 3 | reg(a[1])
        if op in ('bset', 'bclr', 'btst'):
            bit = self.value(a[1])
            if not 0 <= bit <= 15:
                raise AsmError('bit out of range')
            high = 0x49 if op == 'btst' else 0x48
            top = 1 if op in ('bclr', 'btst') else 0
            return high << 8 | top << 7 | reg(a[0]) << 4 | bit
        if op in ('ld.b', 'ld.w', 'st.b', 'st.w', 'st.bh'):
            base, offset = self.mem(a[1])
            high = {'ld.b': 0x60, 'ld.w': 0x70, 'st.b': 0x88, 'st.w': 0x98, 'st.bh': 0x90}[op]
            return (high + reg(a[0])) << 8 | base << 5 | offset
        m = re.fullmatch(r'op\.a([0-9a-f])', op)
        if m:
            n, p = self.value(a[0]), reg(a[1])
            if not 0 <= n <= 31:
                raise AsmError('bad operands for %s' % op)
            return (0xa0 + int(m.group(1), 16)) << 8 | n << 3 | p
        if op == 'pop':
            return 0xb000 | reg(a[0])
        if op == 'push':
            return 0xb400 | reg(a[0])
        if op == 'jmp':
            return (0xb8 + reg(a[0])) << 8
        if op == 'ldl':
            return (0xc0 + reg(a[0])) << 8 | self.disp(a[1], pc, 8)
        if op == 'bra':
            return 0xe000 | self.disp(a[0], pc, 11)
        if op == 'call':
            return 0xe800 | self.disp(a[0], pc, 11)
        if op in BRANCH_COND:
            return BRANCH_COND[op] << 8 | self.disp(a[0], pc, 8)
        m = re.fullmatch(r'b\.([2-9a-e])', op)
        if m:
            return (0xf0 + int(m.group(1), 16)) << 8 | self.disp(a[0], pc, 8)
        raise AsmError('unknown instruction: %s' % op)

    def assemble(self):
        self.parse()
        words = []
        for number, pc, op, operands in self.lines:
            try:
                if op == '.word':
                    for v in operands:
                        x = self.value(v)
                        if not 0 <= x <= 0xffff:
                            raise AsmError('word out of range: %r' % v)
                        words.append(x)
                else:
                    words.append(self.encode(pc, op, operands))
            except AsmError as e:
                raise AsmError('%s:%d: %s' % (self.path, number, e)) from None
        return words


def c_array(name, words):
    data = b''.join(w.to_bytes(2, 'little') for w in words)
    rows = []
    for i in range(0, len(data), 16):
        rows.append('\t' + ' '.join('0x%02x,' % b for b in data[i:i + 16]))
    return ('static const unsigned char %s[] __attribute__((aligned(4))) = {\n%s\n};\n'
            % (name, '\n'.join(rows)))


def main(argv):
    if len(argv) < 3 or argv[1].startswith('-'):
        sys.exit(__doc__)
    text = ['/* Generated by tools/rtlmcuasm.py; do not edit. */\n']
    for spec in argv[2:]:
        name, _, source = spec.partition('=')
        try:
            words = Assembler(source).assemble()
        except AsmError as e:
            sys.exit('rtlmcuasm: %s' % e)
        text.append(c_array(name, words))
    Path(argv[1]).write_text(''.join(text))


if __name__ == '__main__':
    main(sys.argv)
