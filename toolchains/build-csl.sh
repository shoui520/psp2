#!/bin/bash
# The project's GCC 4.8 (C and C++ for arm-none-eabi, on Linux; see common.sh): CodeSourcery's
# GCC 4.8.3 20140320 (Sourcery CodeBench Lite 2014.05-28), the base of Sony's compiler, with
# the changes toward Sony's that the firmware's code shows.
#
# Sony's compiler names itself "4.8.3 20140320 (SCEI rev. 5.3.3)", and its libgcc was built
# in Sony's tree under sys/binary/current/psp2-gcc/src/release/arm-csl-2014q1-scei-5.3.3
# (the SDK libraries' DWARF): a CodeSourcery GCC with Sony's changes. 4.8.3 20140320 is
# this release's version and date. Mentor's download server is gone; the release's gcc
# tarball is kept in Fedora's lookaside cache (the arm-none-eabi-gcc-cs package, f20).
# Sony's changes include trunk work that Linaro 4.8 backported too: -fstack-protector-strong
# (strong-full.diff) and the backports of csl-backports.diff (see corrections()).
#
# configure tests the assembler's features (e.g. SHF_MERGE string sections), so it is
# pointed at vitasdk's ARM assembler (VITASDK must be set).
#
# usage: toolchains/build-csl.sh   (needs gmp, mpfr, mpc, flex, bison, texinfo, curl;
#                                    installed with apt-get where it exists)
set -e
. "$(dirname "$0")/common.sh"
S=$R/toolchains
T=gcc-4.8-2014.05
A=gcc-2014.05-28.tar.bz2
URL=https://src.fedoraproject.org/repo/pkgs/arm-none-eabi-gcc-cs/$A/f17815dccade0fea6e37ebcc9aca6e2f/$A
SHA256=40a4a23215c77a0bb2032f8862f43b40a34d8ed261b6f6a551ce88c7a1a470c7
D=$TC/downloads/gcc-2014.05-28
P=${PREFIX:-$TC/csl-4.8-2014.05-strong$([ "$RAW" = 1 ] || echo -strd)}
W=$HOME/gcc48-csl                                 # build on the machine's own disk (fast)
AS=${VITASDK:?set VITASDK}/bin/arm-vita-eabi-as
LANGS=${LANGS:-c,c++}

if command -v apt-get >/dev/null && ! command -v flex >/dev/null; then
	sudo apt-get update -qq
	sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential libgmp-dev \
		libmpfr-dev libmpc-dev flex bison texinfo curl >/dev/null
fi

# The corrections toward Sony's output that this base needs (RAW=1 leaves them out; each
# measured with match.py against the firmware). Two things Sony's code shows that Linaro 4.8
# changed are this release's own code: an untyped stack_tie (FSF 4.8) and a stored condition
# printed as one ite block.
corrections() {
	local A=gcc/config/arm
	# Trunk changes this release lacks and Sony's compiler has (csl-backports.diff, as Linaro
	# 4.8 backported them):
	# * the Thumb-2 LDRD/STRD peephole2s (r197530 and its helpers; this release has only the
	#   ldrd/strd insns), enabled here for every tune, not only those with prefer_ldrd_strd
	#   (Cortex-A15): Sony's kernel code pairs adjacent sp-relative word loads/stores into
	#   ldrd/strd (iofilemgr: 607 strd sp);
	# * lr used by the epilogue only once the epilogue exists (r198879, PR 42017): before that,
	#   lr is free for the allocator in a function that saves it anyway (sysmem
	#   ObjectHeapSetSizeClass: push {r4-r7, lr}, sub.w lr, r4, #0xc), and a copy of the return
	#   address ends lr's life early (sysmem MemBlockSetNameRaw: mov r6, lr after the push);
	# * ivopts lets an address use after the increment take the auto-increment (r202164), so a
	#   pointer stepped before its load becomes a pre-increment (iofilemgr vfs_is_full_path:
	#   ldrsb r3, [r0, #1]!);
	# * an add with carry into one of its low operands prints as the 16-bit adcs when the
	#   flags are dead (thumb2_reorg, r201411): rtc SceRtcForDriver_A82AC5B3 adds a 64-bit
	#   tick with adds r2, r2, r4; adcs r3, r5;
	# * one DImode and/ior/xor insn each, with the core and the NEON alternatives (r197965,
	#   r199527, r199792, r199814, PR 56315; this release has NEON-only insns that disfavour
	#   core registers): Sony's code builds a 64-bit mask in core registers (acmgr
	#   SceSblACMgrForDriver_2E992B02: movt r1, #0xffff; and.w r5, r0, r1), not with vmov.i64.
	patch -p1 -s < $S/csl-backports.diff
	sed -i 's/&& current_tune->prefer_ldrd_strd/\&\& 1/' $A/ldrdstrd.md
	sed -i '/^(define_insn "\*thumb2_ldrd"$/,/^(include "ldmstm.md")$/s/&& current_tune->prefer_ldrd_strd/\&\& 1/' $A/arm.md
	# The ldm/stm insns (ldmstm.md) are recognized before *thumb2_ldrd/strd, so a pair of
	# stores to [rn] and [rn, #4] with ascending registers prints as stm, as in Sony's code
	# (threadmgr: 118 two-register stm sp, 6 such strd).
	sed -i -e '/^(include "ldmstm.md")$/d' \
		-e 's/^(define_insn "\*thumb2_ldrd"$/(include "ldmstm.md")\n\n(define_insn "*thumb2_ldrd"/' $A/arm.md
	# A single-register pop that returns prints as pop {pc} in Thumb-2 (16-bit), not
	# ldr pc, [sp], #4 (32-bit): Sony's kernel modules contain no ldr.w pc, [sp], #4 and use
	# pop {pc} after push {lr}. Both the RTL epilogue (arm.md "*ldr_with_return") and the
	# text return (arm.c output_return_instruction) are changed.
	perl -0pi -e 's/  "ldr%\?\\t%\|pc, \[%0\], #4"/  "*return TARGET_THUMB2 \&\& REGNO (operands[0]) == SP_REGNUM\n    ? \\"pop%?\\\\t{%|pc}\\" : \\"ldr%?\\\\t%|pc, [%0], #4\\";"/' $A/arm.md
	perl -0pi -e 's/sprintf \(instr, "ldr%s\\t%%\|%s, \[%%\|sp\], #4", conditional,/sprintf (instr, TARGET_THUMB2 ? "pop%s\\t{%%|%s}"\n\t\t\t\t     : "ldr%s\\t%%|%s, [%%|sp], #4", conditional,/' $A/arm.c
	grep -qF 'pop%?\\t{%|pc}' $A/arm.md && grep -qF 'pop%s\t{%%|%s}' $A/arm.c
	# A symbol plus a constant offset (&s[1], s + 0x4c) is loaded with a movw/movt pair, like
	# a bare symbol, not from the literal pool (lowio: movw r4, #0xb1ec, s_gpio + 0x4c). The
	# offset must fit the 16-bit addend a REL MOVW/MOVT relocation keeps in the instruction.
	python3 - $A/arm.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """   && TARGET_USE_MOVT && GET_CODE (operands[1]) == SYMBOL_REF
   && !flag_pic && !target_word_relocations"""
new = """   && TARGET_USE_MOVT
   && (GET_CODE (operands[1]) == SYMBOL_REF
       || (GET_CODE (operands[1]) == CONST
	   && GET_CODE (XEXP (operands[1], 0)) == PLUS
	   && GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == SYMBOL_REF
	   && CONST_INT_P (XEXP (XEXP (operands[1], 0), 1))
	   && IN_RANGE (INTVAL (XEXP (XEXP (operands[1], 0), 1)),
			-0x8000, 0x7fff)))
   && !flag_pic && !target_word_relocations"""
assert s.count(old) == 1
open(p, 'w').write(s.replace(old, new))
PYEOF
	# x == 0 stored in a register prints as clz rd, rn; lsrs rd, rd, #5 (ARMv5 and later), the
	# define_split Linaro 4.8 backported from trunk; this release has only the conditional
	# rsbs/it/movlo form. Sony's code has the clz form for a comparison with 0 (sysmem
	# UIDEntryIsUnused) and keeps this release's sub/negs/adc form for a comparison with
	# another value (sysmem SceKernelUtilsForDriver_D8FAEFD4).
	python3 - $A/arm.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """(define_split
  [(set (match_operand:SI 0 "s_register_operand" "")
	(eq:SI (match_operand:SI 1 "s_register_operand" "")
	       (const_int 0)))
   (clobber (reg:CC CC_REGNUM))]
  "TARGET_32BIT && reload_completed"
"""
new = """(define_split
  [(set (match_operand:SI 0 "s_register_operand" "")
	(eq:SI (match_operand:SI 1 "s_register_operand" "")
	       (const_int 0)))
   (clobber (reg:CC CC_REGNUM))]
  "arm_arch5 && TARGET_32BIT"
  [(set (match_dup 0) (clz:SI (match_dup 1)))
   (set (match_dup 0) (lshiftrt:SI (match_dup 0) (const_int 5)))]
)

"""
assert s.count(old) == 1
open(p, 'w').write(s.replace(old, new + old))
PYEOF
	# The length the branch shortening counts for a register move (mov rd, rm: 2 bytes, as
	# Linaro 4.8's Thumb-2 length attributes give it; this release counts 4) and for a stored
	# condition into a low register (ite; mov; mov: 8, otherwise 10). With 4 per move, branches
	# Sony's compiler made cbz/cbnz come out as cmp; beq (deci4p_rdrfp rdrfpRead).
	python3 - $A/vfp.md $A/thumb2.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
i = s.index('(define_insn "*thumb2_movsi_vfp"')
old = """   (set_attr "pool_range"     "*,*,*,*,1018,4094,*,*,*,*,*,1018,*")"""
assert s.count(old, i, i + 4000) == 1, 'thumb2_movsi_vfp attributes'
new = """   (set_attr "length" "2,4,4,4,4,4,4,4,4,4,4,4,4")\n""" + old
open(p, 'w').write(s[:i] + s[i:].replace(old, new, 1))
p = sys.argv[2]
s = open(p).read()
old = r"""(define_insn "*thumb2_mov_scc"
  [(set (match_operand:SI 0 "s_register_operand" "=r")
	(match_operator:SI 1 "arm_comparison_operator"
	 [(match_operand 2 "cc_register" "") (const_int 0)]))]
  "TARGET_THUMB2"
  "ite\\t%D1\;mov%D1\\t%0, #0\;mov%d1\\t%0, #1"
  [(set_attr "conds" "use")
   (set_attr "length" "10")]"""
new = r"""(define_insn "*thumb2_mov_scc"
  [(set (match_operand:SI 0 "s_register_operand" "=l,r")
	(match_operator:SI 1 "arm_comparison_operator"
	 [(match_operand 2 "cc_register" "") (const_int 0)]))]
  "TARGET_THUMB2"
  "ite\\t%D1\;mov%D1\\t%0, #0\;mov%d1\\t%0, #1"
  [(set_attr "conds" "use")
   (set_attr "length" "8,10")]"""
assert s.count(old) == 1, 'thumb2_mov_scc'
open(p, 'w').write(s.replace(old, new))
PYEOF
	# An unsigned 64-bit comparison is split after reload into cmp and a conditional cmpeq
	# (trunk r197518-197528, Greta Yorsh's series beside the LDRD/STRD peephole2s, with the
	# 16-bit alternatives of r200593-201126), so the cmpeq shares one IT block with the
	# conditional insns that follow (deci4p_rdrfp rdrfpFindFileCache: cmp r5, r3; itt eq;
	# cmpeq r4, r2; moveq r0, #7). This release prints the pair as one insn with its own it.
	python3 - $A/arm.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = r"""(define_insn "*arm_cmpdi_unsigned"
  [(set (reg:CC_CZ CC_REGNUM)
	(compare:CC_CZ (match_operand:DI 0 "s_register_operand" "r")
		       (match_operand:DI 1 "arm_di_operand"	"rDi")))]
  "TARGET_32BIT"
  "cmp\\t%R0, %R1\;it eq\;cmpeq\\t%Q0, %Q1"
  [(set_attr "conds" "set")
   (set_attr "length" "8")]
)"""
new = r"""(define_insn_and_split "*arm_cmpdi_unsigned"
  [(set (reg:CC_CZ CC_REGNUM)
	(compare:CC_CZ (match_operand:DI 0 "s_register_operand" "l,r,r")
		       (match_operand:DI 1 "arm_di_operand"	"Py,r,rDi")))]
  "TARGET_32BIT"
  "#"   ; "cmp\\t%R0, %R1\;it eq\;cmpeq\\t%Q0, %Q1"
  "&& reload_completed"
  [(set (reg:CC CC_REGNUM)
	(compare:CC (match_dup 2) (match_dup 3)))
   (cond_exec (eq:SI (reg:CC CC_REGNUM) (const_int 0))
	      (set (reg:CC CC_REGNUM)
		   (compare:CC (match_dup 0) (match_dup 1))))]
  {
    operands[2] = gen_highpart (SImode, operands[0]);
    operands[0] = gen_lowpart (SImode, operands[0]);
    if (CONST_INT_P (operands[1]))
      operands[3] = gen_highpart_mode (SImode, DImode, operands[1]);
    else
      operands[3] = gen_highpart (SImode, operands[1]);
    operands[1] = gen_lowpart (SImode, operands[1]);
  }
  [(set_attr "conds" "set")
   (set_attr "arch" "t2,t2,*")
   (set_attr "length" "6,6,8")]
)"""
assert s.count(old) == 1, 'arm_cmpdi_unsigned'
open(p, 'w').write(s.replace(old, new))
PYEOF
	# 64-bit arithmetic stays in core registers: the NEON alternatives of the DImode
	# patterns are off, the core ones on (trunk r196876 with its default, no
	# -mneon-for-64bits; this release turns the NEON ones on for every tune but Cortex-A8).
	# Sony's code adds 64-bit ticks with adds/adcs (rtc SceRtcForDriver_A82AC5B3: ldrd, adds,
	# adcs, strd), not vadd.i64.
	python3 - $A/arm.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """	 (and (eq_attr "arch" "onlya8")
	      (eq_attr "tune" "cortexa8"))
	 (const_string "yes")

	 (and (eq_attr "arch" "neon_onlya8")
	      (eq_attr "tune" "cortexa8")
	      (match_test "TARGET_NEON"))
	 (const_string "yes")

	 (and (eq_attr "arch" "nota8")
	      (not (eq_attr "tune" "cortexa8")))
	 (const_string "yes")

	 (and (eq_attr "arch" "neon_nota8")
	      (not (eq_attr "tune" "cortexa8"))
	      (match_test "TARGET_NEON"))
	 (const_string "yes")
"""
new = """	 (and (eq_attr "arch" "onlya8,neon_onlya8")
	      (match_test "TARGET_NEON"))
	 (const_string "yes")
"""
assert s.count(old) == 1, 'arch_enabled a8'
open(p, 'w').write(s.replace(old, new))
PYEOF
	# A subtraction of two registers prints as sub, never rsb with the operands swapped: the
	# rsb alternative of *arm_subsi3_insn takes only an immediate first operand. Sony's kernel
	# code has no register-form rsb (4.00 os0:kd) and uses sub.w also for high registers
	# (sysmem: sub.w lr, r9, r2; init_user_mstate: sub.w r4, r4, r3).
	grep -qF '(minus:SI (match_operand:SI 1 "reg_or_int_operand" "rI,r,r,k,?n")' $A/arm.md
	sed -i 's/(minus:SI (match_operand:SI 1 "reg_or_int_operand" "rI,r,r,k,?n")/(minus:SI (match_operand:SI 1 "reg_or_int_operand" "I,r,r,k,?n")/' $A/arm.md
	# -fuse-caller-save (CodeSourcery's, on at -O2) is off by default: Sony's code keeps a
	# value live across a call to a function of the same file in a callee-saved register
	# (avcodec _audiodecInternalGetInternalError: push {r3, r4, r5, lr}), not in r2/r3.
	grep -q 'OPT_LEVELS_2_PLUS, OPT_fuse_caller_save, NULL, 1' gcc/opts.c
	sed -i '/OPT_LEVELS_2_PLUS, OPT_fuse_caller_save, NULL, 1/d' gcc/opts.c
	# A word load or store that post-increments a low base register by 4 prints as a
	# single-register ldmia/stmia with writeback (16-bit), not ldr/str rt, [rn], #4: Sony's
	# kernel code uses the ldm/stm form (4.00 sysmem, threadmgr, iofilemgr, modulemgr: 104
	# ldm rn!, {rt} and 84 stm rn!, {rt}, against 4 ldr and 5 str post-increments). The base
	# must not be the transferred register (its writeback would be suppressed). Alternatives
	# 4 and 6 of *thumb2_movsi_vfp are the low-register load and store.
	python3 - $A/vfp.md <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = """    case 4:
    case 5:
      return \\"ldr%?\\\\t%0, %1\\";
    case 6:
    case 7:
      return \\"str%?\\\\t%1, %0\\";
    case 8:
      return \\"fmsr%?\\\\t%0, %1\\\\t%@ int\\";"""
new = """    case 4:
    case 6:
      {
	rtx mem = operands[which_alternative == 4 ? 1 : 0];
	rtx reg = operands[which_alternative == 4 ? 0 : 1];
	rtx addr = MEM_P (mem) ? XEXP (mem, 0) : NULL_RTX;
	if (addr && GET_CODE (addr) == POST_INC && REG_P (XEXP (addr, 0))
	    && REGNO (XEXP (addr, 0)) <= LAST_LO_REGNUM
	    && REGNO (XEXP (addr, 0)) != REGNO (reg))
	  {
	    rtx ops[2];
	    ops[0] = XEXP (addr, 0);
	    ops[1] = reg;
	    output_asm_insn (which_alternative == 4 ? \\"ldmia%?\\\\t%0!, {%1}\\"
			     : \\"stmia%?\\\\t%0!, {%1}\\", ops);
	    return \\"\\";
	  }
      }
      return which_alternative == 4 ? \\"ldr%?\\\\t%0, %1\\" : \\"str%?\\\\t%1, %0\\";
    case 5:
      return \\"ldr%?\\\\t%0, %1\\";
    case 7:
      return \\"str%?\\\\t%1, %0\\";
    case 8:
      return \\"fmsr%?\\\\t%0, %1\\\\t%@ int\\";"""
i = s.index('(define_insn "*thumb2_movsi_vfp"')
assert s.count(old, i, i + 3000) == 1, 'thumb2_movsi_vfp template'
open(p, 'w').write(s[:i] + s[i:].replace(old, new, 1))
PYEOF
}

mkdir -p $D
if [ ! -f $D/$A ]; then
	curl -fsSL -o $D/$A.part $URL
	mv $D/$A.part $D/$A
fi
echo "$SHA256  $D/$A" | sha256sum -c --quiet

rm -rf $W/$T && mkdir -p $W && tar -xjf $D/$A -C $W
cd $W/$T
patch -p1 -s < $S/strong-full.diff
[ "$RAW" = 1 ] || corrections

B=$W/build-$(basename $P)
rm -rf $B && mkdir -p $B && cd $B
CC="gcc -std=gnu89 -fcommon -w" CXX="g++ -std=gnu++98 -fpermissive -w" $W/$T/configure \
	--target=arm-none-eabi --prefix=$P --enable-languages=$LANGS --disable-multilib \
	--disable-nls --without-headers --with-newlib --disable-libssp --disable-shared \
	--disable-threads --with-as=$AS > configure.log 2>&1
make all-gcc -j8 > make.log 2>&1
make install-gcc > install.log 2>&1
echo "built $P"
