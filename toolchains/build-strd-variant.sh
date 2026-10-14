#!/bin/bash
# The project's GCC 4.8 (C and C++): build-cxx.sh's compiler with the Thumb-2 LDRD/STRD
# peephole2s (config/arm/ldrdstrd.md, and the arm.md insns they emit) enabled for every
# tune, not only those with prefer_ldrd_strd (Cortex-A15). Sony's kernel code pairs
# adjacent sp-relative word loads/stores into ldrd/strd (iofilemgr: 607 strd sp), stock
# 4.8 for Cortex-A9 does not. Measured 2026-10-11, 4.00 match.py: threadmgr and sysmem
# gain ~45 byte-identical functions, lose 2. Prologue/epilogue STRD pushes stay as they are.
# Installs to <toolchains>/linaro-4.8-2014.04-strong-strd (see common.sh). Run on Linux
# after build-cxx.sh (it reuses that source tree and restores ldrdstrd.md afterwards).
set -e
. "$(dirname "$0")/common.sh"
T=gcc-linaro-4.8-2014.04
P=${PREFIX:-$TC/linaro-4.8-2014.04-strong-strd}
AS=${VITASDK:?set VITASDK}/bin/arm-vita-eabi-as
LANGS=${LANGS:-c,c++}
W=$HOME/gcc48-cxx
M=$W/$T/gcc/config/arm/ldrdstrd.md
D=$W/$T/gcc/config/arm/arm.md
C=$W/$T/gcc/config/arm/arm.c
U=$W/$T/gcc/config/arm/thumb2.md
cp $M $M.orig; cp $D $D.orig; cp $C $C.orig; cp $U $U.orig
trap 'mv $M.orig $M; mv $D.orig $D; mv $C.orig $C; mv $U.orig $U' EXIT
sed -i 's/&& current_tune->prefer_ldrd_strd/\&\& 1/' $M
# the ldrd/strd insns the peepholes produce (arm.md "*thumb2_ldrd" ... "*thumb2_strd_base_neg")
sed -i '12570,12670s/&& current_tune->prefer_ldrd_strd/\&\& 1/' $D
# The ldm/stm insns (ldmstm.md) are recognized before *thumb2_ldrd/strd, so a pair of
# stores to [rn] and [rn, #4] with ascending registers prints as stm, as in Sony's code
# (threadmgr: 118 two-register stm sp, 6 such strd; measured 2026-10-11: +18 functions
# byte-identical in threadmgr/sysmem, none lost).
sed -i -e '/^(include "ldmstm.md")$/d' \
	-e 's/^(define_insn "\*thumb2_ldrd"$/(include "ldmstm.md")\n\n(define_insn "*thumb2_ldrd"/' $D
# stack_tie (the prologue's sp/fp barrier) as in FSF 4.8: no type of its own, so the default
# alu_reg (a Cortex-A9 ALU reservation, latency 2); Linaro 4.8-2014.04 backported "block"
# (no reservation, latency 0), which moves the stack guard's movt above the prologue's
# sub sp in frame-pointer functions (Sony: movw, sub sp, movt, add r7; measured 2026-10-11:
# +86 functions byte-identical in threadmgr/vshbridge/iofilemgr, none lost).
sed -i '/^(define_insn "stack_tie"$/,/^)$/s/(set_attr "type" "block")/(set_attr "type" "alu_reg")/' $D
# A single-register pop that returns prints as pop {pc} in Thumb-2 (16-bit), not
# ldr pc, [sp], #4 (32-bit ldr.w): Sony's kernel modules contain no ldr.w pc, [sp], #4
# (sysmem, threadmgr, iofilemgr, modulemgr, processmgr, intrmgr, excpmgr, acmgr: 0)
# and use pop {pc} after push {lr}. Both the RTL epilogue (arm.md "*ldr_with_return")
# and the text return (arm.c output_return_instruction) are changed.
perl -0pi -e 's/  "ldr%\?\\t%\|pc, \[%0\], #4"/  "*return TARGET_THUMB2 \&\& REGNO (operands[0]) == SP_REGNUM\n    ? \\"pop%?\\\\t{%|pc}\\" : \\"ldr%?\\\\t%|pc, [%0], #4\\";"/' $D
perl -0pi -e 's/sprintf \(instr, "ldr%s\\t%%\|%s, \[%%\|sp\], #4", conditional,/sprintf (instr, TARGET_THUMB2 ? "pop%s\\t{%%|%s}"\n\t\t\t\t     : "ldr%s\\t%%|%s, [%%|sp], #4", conditional,/' $C
grep -qF 'pop%?\\t{%|pc}' $D && grep -qF 'pop%s\t{%%|%s}' $C
# A store of a condition (x = a == b) prints as one ite block, as in FSF 4.8:
# "ite ne; movne rd, #0; moveq rd, #1". Linaro 4.8-2014.04 backported trunk r197925,
# which splits *thumb2_mov_scc (and negscc, notscc) into if_then_else moves before
# register allocation, so they merge with the comparison's IT block ("itte eq; cmpeq;
# moveq rd, #1; movne rd, #0"); Sony's code has the separate ite form (acmgr).
python3 - $U <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
for t in ('ite\\\\t%D1\\;mov%D1\\\\t%0, #0\\;mov%d1\\\\t%0, #1',
	  'ite\\\\t%D1\\;mov%D1\\\\t%0, #0\\;mvn%d1\\\\t%0, #0',
	  'ite\\\\t%D1\\;mvn%D1\\\\t%0, #0\\;mvn%d1\\\\t%0, #1'):
	old = '"#"   ; "%s"\n  "TARGET_THUMB2"\n' % t
	assert s.count(old) == 1, t
	s = s.replace(old, '"%s"\n  "0"\n' % t)
open(p, 'w').write(s)
PYEOF
# A symbol plus a constant offset (&s[1], s + 0x4c) is loaded with a movw/movt pair, like a
# bare symbol, not from the literal pool: Sony's code has movw/movt with the offset in the
# relocation (lowio: movw r4, #0xb1ec for SceGpio's second context, s_gpio + 0x4c), stock
# 4.8 splits only SYMBOL_REF into movw/movt and puts symbol + offset in a literal word
# (measured 2026-10-14, 4.00 match.py on lowio/threadmgr/sysmem/iofilemgr: +5 exact, none lost).
# The offset must fit the 16-bit addend a REL MOVW/MOVT relocation keeps in the instruction
# (-0x8000..0x7fff, as later GCC's arm_valid_symbolic_address_p); larger ones stay as before.
python3 - $D <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = '''   && TARGET_USE_MOVT && GET_CODE (operands[1]) == SYMBOL_REF
   && !flag_pic && !target_word_relocations'''
new = '''   && TARGET_USE_MOVT
   && (GET_CODE (operands[1]) == SYMBOL_REF
       || (GET_CODE (operands[1]) == CONST
	   && GET_CODE (XEXP (operands[1], 0)) == PLUS
	   && GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == SYMBOL_REF
	   && CONST_INT_P (XEXP (XEXP (operands[1], 0), 1))
	   && IN_RANGE (INTVAL (XEXP (XEXP (operands[1], 0), 1)),
			-0x8000, 0x7fff)))
   && !flag_pic && !target_word_relocations'''
assert s.count(old) == 1
s = s.replace(old, new)
open(p, 'w').write(s)
PYEOF
B=$W/build-$(basename $P)
rm -rf $B && mkdir -p $B && cd $B
CC="gcc -std=gnu89 -fcommon -w" CXX="g++ -std=gnu++98 -fpermissive -w" $W/$T/configure \
	--target=arm-none-eabi --prefix=$P --enable-languages=$LANGS --disable-multilib \
	--disable-nls --without-headers --with-newlib --disable-libssp --disable-shared \
	--disable-threads --with-as=$AS > configure.log 2>&1
make all-gcc -j8 > make.log 2>&1
make install-gcc > install.log 2>&1
echo "built $P"
