/*
 * USE instruction parser: one line of USE assembly, in the syntax Imagination's
 * disassembler prints, into the USE_INST its encoder (UseAssembleInstruction) takes.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "useparse.h"

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

typedef struct NameValue {
	const char *name;
	IMG_UINT32 value;
} NameValue;

/* the instruction modifiers that are one bit of uFlags1, uFlags2 or uFlags3 */
static const NameValue flags1Names[] = {
	{ "skipinv",	USEASM_OPFLAGS1_SKIPINVALID },
	{ "syncs",	USEASM_OPFLAGS1_SYNCSTART },
	{ "nosched",	USEASM_OPFLAGS1_NOSCHED },
	{ "end",	USEASM_OPFLAGS1_END },
	{ "synce",	USEASM_OPFLAGS1_SYNCEND },
	{ "savelink",	USEASM_OPFLAGS1_SAVELINK },
	{ "partial",	USEASM_OPFLAGS1_PARTIAL },
	{ "mask",	USEASM_OPFLAGS1_MOVC_FMTMASK },
	{ "mon",	USEASM_OPFLAGS1_MONITOR },
	{ "abs",	USEASM_OPFLAGS1_ABS },
	{ "renable",	USEASM_OPFLAGS1_ROUNDENABLE },
	{ "flt16",	USEASM_OPFLAGS1_MOVC_FMTF16 },
	{ "rangeenable", USEASM_OPFLAGS1_RANGEENABLE },
};

static const NameValue flags2Names[] = {
	{ "onceonly",	USEASM_OPFLAGS2_ONCEONLY },
	{ "bpcache",	USEASM_OPFLAGS2_BYPASSCACHE },
	{ "fcfill",	USEASM_OPFLAGS2_FORCELINEFILL },
	{ "tasks",	USEASM_OPFLAGS2_TASKSTART },
	{ "taske",	USEASM_OPFLAGS2_TASKEND },
	{ "scale",	USEASM_OPFLAGS2_SCALE },
	{ "i16",	USEASM_OPFLAGS2_MOVC_FMTI16 },
	{ "i32",	USEASM_OPFLAGS2_MOVC_FMTI32 },
	{ "flt",	USEASM_OPFLAGS2_MOVC_FMTF32 },
	{ "tpres",	USEASM_OPFLAGS2_TYPEPRESERVE },
	{ "i10",	USEASM_OPFLAGS2_MOVC_FMTI10 },
	{ "fsel",	USEASM_OPFLAGS2_FORMATSELECT },
	{ "c10",	USEASM_OPFLAGS2_C10 },
	{ "tog",	USEASM_OPFLAGS2_TOGGLEOUTFILES },
	{ "moepoff",	USEASM_OPFLAGS2_MOEPOFF },
	{ "u",		USEASM_OPFLAGS2_UNSIGNED },
	{ "s",		USEASM_OPFLAGS2_SIGNED },
	{ "usat",	USEASM_OPFLAGS2_UNSIGNED | USEASM_OPFLAGS2_SAT },
	{ "ssat",	USEASM_OPFLAGS2_SIGNED | USEASM_OPFLAGS2_SAT },
	{ "sat",	USEASM_OPFLAGS2_SAT },
};

static const NameValue flags3Names[] = {
	{ "pcf",	USEASM_OPFLAGS3_PCFF32 },
	{ "pcff16",	USEASM_OPFLAGS3_PCFF16 },
	{ "rsd",	USEASM_OPFLAGS3_SAMPLEDATA },
	{ "sinf",	USEASM_OPFLAGS3_SAMPLEINFO },
	{ "syncent",	USEASM_OPFLAGS3_SYNCENDNOTTAKEN },
	{ "trigger",	USEASM_OPFLAGS3_TRIGGER },
	{ "twopart",	USEASM_OPFLAGS3_TWOPART },
	{ "threepart",	USEASM_OPFLAGS3_THREEPART },
	{ "cut",	USEASM_OPFLAGS3_CUT },
	{ "bypassl1",	USEASM_OPFLAGS3_BYPASSL1 },
	{ "gpiext",	USEASM_OPFLAGS3_GPIEXT },
	{ "global",	USEASM_OPFLAGS3_GLOBAL },
	{ "dm_nomatch",	USEASM_OPFLAGS3_DM_NOMATCH },
	{ "noread",	USEASM_OPFLAGS3_NOREAD },
	{ "pwait",	USEASM_OPFLAGS3_PWAIT },
	{ "irsd",	USEASM_OPFLAGS3_SAMPLEDATAANDINFO },
	{ "minp",	USEASM_OPFLAGS3_MINPACK },
	{ "s1",		USEASM_OPFLAGS3_STEP1 },
	{ "s2",		USEASM_OPFLAGS3_STEP2 },
	{ "smsk",	USEASM_OPFLAGS3_SAMPLEMASK },
	{ "freep",	USEASM_OPFLAGS3_FREEP },
	{ "allinst",	USEASM_OPFLAGS3_ALLINSTANCES },
	{ "anyinst",	USEASM_OPFLAGS3_ANYINSTANCES },
};

/* the predicates, by their USEASM_PRED_* value */
static const char *const predicateNames[] = {
	"", "p0", "p1", "!p0", "!p1", "p2", "p3", "Pn", "!p2",
};

static const NameValue chanSelNames[] = {
	{ "chan0",	USEASM_TEST_CHANSEL_C0 },
	{ "chan1",	USEASM_TEST_CHANSEL_C1 },
	{ "chan2",	USEASM_TEST_CHANSEL_C2 },
	{ "chan3",	USEASM_TEST_CHANSEL_C3 },
	{ "andall",	USEASM_TEST_CHANSEL_ANDALL },
	{ "orall",	USEASM_TEST_CHANSEL_ORALL },
	{ "and02",	USEASM_TEST_CHANSEL_AND02 },
	{ "or02",	USEASM_TEST_CHANSEL_OR02 },
	{ "cperchan",	USEASM_TEST_CHANSEL_PERCHAN },
};

static const NameValue testMaskNames[] = {
	{ "bmsk",	USEASM_TEST_MASK_BYTE },
	{ "wmsk",	USEASM_TEST_MASK_WORD },
	{ "dmsk",	USEASM_TEST_MASK_DWORD },
	{ "msk",	USEASM_TEST_MASK_PREC },
	{ "num",	USEASM_TEST_MASK_NUM },
};

#define TEST(zero, sign, comb) \
	((USEASM_TEST_ZERO_##zero << USEASM_TEST_ZERO_SHIFT) | \
	 (USEASM_TEST_SIGN_##sign << USEASM_TEST_SIGN_SHIFT) | USEASM_TEST_CRCOMB_##comb)

/* the short test forms of the instructions with USE_DESCFLAG_DISASM_SIMPLE_TESTS */
static const NameValue simpleTestNames[] = {
	{ "tn",		TEST(NONZERO, NEGATIVE, AND) },
	{ "tn|z",	TEST(ZERO, NEGATIVE, OR) },
	{ "tz",		TEST(ZERO, TRUE, AND) },
	{ "tnz",	TEST(NONZERO, TRUE, AND) },
	{ "tp|z",	TEST(ZERO, POSITIVE, OR) },
};

static const NameValue argFlagNames[] = {
	{ "neg",	USEASM_ARGFLAGS_NEGATE },
	{ "abs",	USEASM_ARGFLAGS_ABSOLUTE },
	{ "low",	USEASM_ARGFLAGS_LOW },
	{ "high",	USEASM_ARGFLAGS_HIGH },
	{ "alpha",	USEASM_ARGFLAGS_ALPHA },
	{ "comp",	USEASM_ARGFLAGS_COMPLEMENT },
	{ "flt32",	USEASM_ARGFLAGS_FMTF32 },
	{ "flt16",	USEASM_ARGFLAGS_FMTF16 },
	{ "u8",		USEASM_ARGFLAGS_FMTU8 },
	{ "c10",	USEASM_ARGFLAGS_FMTC10 },
	{ "moe",	USEASM_ARGFLAGS_REQUIREMOE },
	{ "swzll",	USEASM_MAD16SWZ_LOWLOW << USEASM_ARGFLAGS_MAD16SWZ_SHIFT },
	{ "swzhh",	USEASM_MAD16SWZ_HIGHHIGH << USEASM_ARGFLAGS_MAD16SWZ_SHIFT },
	{ "swzf32",	USEASM_MAD16SWZ_CVTFROMF32 << USEASM_ARGFLAGS_MAD16SWZ_SHIFT },
};

/* the register banks, by prefix; longer prefixes first */
static const NameValue regTypeNames[] = {
	{ "drc",	USEASM_REGTYPE_DRC },
	{ "fcs",	USEASM_REGTYPE_FILTERCOEFF },
	{ "pa",		USEASM_REGTYPE_PRIMATTR },
	{ "sa",		USEASM_REGTYPE_SECATTR },
	{ "cp",		USEASM_REGTYPE_CLIPPLANE },
	{ "r",		USEASM_REGTYPE_TEMP },
	{ "o",		USEASM_REGTYPE_OUTPUT },
	{ "g",		USEASM_REGTYPE_GLOBAL },
	{ "c",		USEASM_REGTYPE_FPCONSTANT },
	{ "i",		USEASM_REGTYPE_FPINTERNAL },
	{ "p",		USEASM_REGTYPE_PREDICATE },
};

/* the source selector keywords */
static const NameValue intSrcSelNames[] = {
	{ "zero", USEASM_INTSRCSEL_ZERO }, { "one", USEASM_INTSRCSEL_ONE },
	{ "asat", USEASM_INTSRCSEL_SRCALPHASAT }, { "rasat", USEASM_INTSRCSEL_RSRCALPHASAT },
	{ "s0", USEASM_INTSRCSEL_SRC0 }, { "s1", USEASM_INTSRCSEL_SRC1 },
	{ "s2", USEASM_INTSRCSEL_SRC2 }, { "s0a", USEASM_INTSRCSEL_SRC0ALPHA },
	{ "s1a", USEASM_INTSRCSEL_SRC1ALPHA }, { "s2a", USEASM_INTSRCSEL_SRC2ALPHA },
	{ "Cx1", USEASM_INTSRCSEL_CX1 }, { "Cx2", USEASM_INTSRCSEL_CX2 },
	{ "Cx4", USEASM_INTSRCSEL_CX4 }, { "Cx8", USEASM_INTSRCSEL_CX8 },
	{ "Ax1", USEASM_INTSRCSEL_AX1 }, { "Ax2", USEASM_INTSRCSEL_AX2 },
	{ "Ax4", USEASM_INTSRCSEL_AX4 }, { "Ax8", USEASM_INTSRCSEL_AX8 },
	{ "add", USEASM_INTSRCSEL_ADD }, { "sub", USEASM_INTSRCSEL_SUB },
	{ "neg", USEASM_INTSRCSEL_NEG }, { "none", USEASM_INTSRCSEL_NONE },
	{ "min", USEASM_INTSRCSEL_MIN }, { "max", USEASM_INTSRCSEL_MAX },
	{ "s2scale", USEASM_INTSRCSEL_SRC2SCALE }, { "zeros", USEASM_INTSRCSEL_ZEROS },
	{ "comp", USEASM_INTSRCSEL_COMP }, { "+src2.2", USEASM_INTSRCSEL_PLUSSRC2DOT2 },
	{ "iadd", USEASM_INTSRCSEL_IADD }, { "scale", USEASM_INTSRCSEL_SCALE },
	{ "u8", USEASM_INTSRCSEL_U8 }, { "s8", USEASM_INTSRCSEL_S8 },
	{ "o8", USEASM_INTSRCSEL_O8 }, { "z16", USEASM_INTSRCSEL_Z16 },
	{ "s16", USEASM_INTSRCSEL_S16 }, { "u32", USEASM_INTSRCSEL_U32 },
	{ "cie", USEASM_INTSRCSEL_CINENABLE }, { "coe", USEASM_INTSRCSEL_COUTENABLE },
	{ "u16", USEASM_INTSRCSEL_U16 }, { "interleaved", USEASM_INTSRCSEL_INTERLEAVED },
	{ "planar", USEASM_INTSRCSEL_PLANAR }, { "src01", USEASM_INTSRCSEL_SRC01 },
	{ "src23", USEASM_INTSRCSEL_SRC23 }, { "dst01", USEASM_INTSRCSEL_DST01 },
	{ "dst23", USEASM_INTSRCSEL_DST23 }, { "rnd", USEASM_INTSRCSEL_RND },
	{ "twosided", USEASM_INTSRCSEL_TWOSIDED }, { "fb", USEASM_INTSRCSEL_FEEDBACK },
	{ "nfb", USEASM_INTSRCSEL_NOFEEDBACK }, { "optdwd", USEASM_INTSRCSEL_OPTDWD },
	{ "idst", USEASM_INTSRCSEL_IDST }, { "down", USEASM_INTSRCSEL_ROUNDDOWN },
	{ "nearest", USEASM_INTSRCSEL_ROUNDNEAREST }, { "up", USEASM_INTSRCSEL_ROUNDUP },
	{ "pixel", USEASM_INTSRCSEL_PIXEL }, { "sample", USEASM_INTSRCSEL_SAMPLE },
	{ "selective", USEASM_INTSRCSEL_SELECTIVE }, { "pt", USEASM_INTSRCSEL_PT },
	{ "vcull", USEASM_INTSRCSEL_VCULL }, { "end", USEASM_INTSRCSEL_END },
	{ "parallel", USEASM_INTSRCSEL_PARALLEL }, { "perinstance", USEASM_INTSRCSEL_PERINSTANCE },
	{ "srcneg", USEASM_INTSRCSEL_SRCNEG }, { "srcabs", USEASM_INTSRCSEL_SRCABS },
	{ "incuss", USEASM_INTSRCSEL_INCREMENTUS }, { "incgpi", USEASM_INTSRCSEL_INCREMENTGPI },
	{ "incboth", USEASM_INTSRCSEL_INCREMENTBOTH }, { "moe", USEASM_INTSRCSEL_INCREMENTMOE },
	{ "incall", USEASM_INTSRCSEL_INCREMENTALL }, { "incs1", USEASM_INTSRCSEL_INCREMENTS1 },
	{ "incs2", USEASM_INTSRCSEL_INCREMENTS2 }, { "incs1s2", USEASM_INTSRCSEL_INCREMENTS1S2 },
	{ "f32", USEASM_INTSRCSEL_F32 }, { "f16", USEASM_INTSRCSEL_F16 },
	{ "or", USEASM_INTSRCSEL_OR }, { "xor", USEASM_INTSRCSEL_XOR },
	{ "and", USEASM_INTSRCSEL_AND }, { "revsub", USEASM_INTSRCSEL_REVSUB },
	{ "xchg", USEASM_INTSRCSEL_XCHG }, { "true", USEASM_INTSRCSEL_TRUE },
	{ "false", USEASM_INTSRCSEL_FALSE }, { "imin", USEASM_INTSRCSEL_IMIN },
	{ "imax", USEASM_INTSRCSEL_IMAX }, { "umin", USEASM_INTSRCSEL_UMIN },
	{ "umax", USEASM_INTSRCSEL_UMAX },
};

static const NameValue addressModeNames[] = {
	{ "none",	EURASIA_USE1_MOECTRL_ADDRESSMODE_NONE },
	{ "mirror",	EURASIA_USE1_MOECTRL_ADDRESSMODE_MIRROR },
	{ "repeat",	EURASIA_USE1_MOECTRL_ADDRESSMODE_REPEAT },
	{ "clamp",	EURASIA_USE1_MOECTRL_ADDRESSMODE_CLAMP },
};

/* the fence paths of idf */
static const char *const idfPathNames[] = { "bif", "pixelbe" };

static const char swizzleSelNames[] = "xyzw012h";

typedef struct Parser {
	const char *pos;
	char *error;
	UseParseLabelFn label;
	void *labelContext;
} Parser;

static int fail(Parser *p, const char *fmt, ...)
{
	va_list ap;

	if (p->error[0] == '\0') {
		va_start(ap, fmt);
		vsnprintf(p->error, USEPARSE_ERROR_SIZE, fmt, ap);
		va_end(ap);
	}
	return 0;
}

static const NameValue *findName(const NameValue *table, size_t count, const char *name)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (strcmp(table[i].name, name) == 0)
			return &table[i];
	}
	return NULL;
}

static int parseNumber(const char *s, IMG_UINT32 *value, const char **end)
{
	char *e;
	unsigned long long v;
	int negative = 0;

	if (*s == '-') {
		negative = 1;
		s++;
	}
	if (!isdigit((unsigned char)*s))
		return 0;
	v = strtoull(s, &e, 0);
	*value = negative ? (IMG_UINT32)-(long long)v : (IMG_UINT32)v;
	*end = e;
	return 1;
}

/* splits the text on its top-level commas */
static int splitArgs(char *text, char **args, int max)
{
	int count = 0;
	int depth = 0;
	char *start = text;
	char *s;

	while (*start == ' ')
		start++;
	if (*start == '\0')
		return 0;
	for (s = start; ; s++) {
		if (*s == '[' || *s == '(')
			depth++;
		else if (*s == ']' || *s == ')')
			depth--;
		if ((*s == ',' && depth == 0) || *s == '\0') {
			char *e = s;
			int last = *s == '\0';

			while (e > start && e[-1] == ' ')
				e--;
			*e = '\0';
			if (count == max)
				return -1;
			args[count++] = start;
			if (last)
				break;
			start = s + 1;
			while (*start == ' ')
				start++;
		}
	}
	return count;
}

/* the modifiers after a register: .neg, .low, .bytemask1010, .2 (component), ... */
static int parseArgSuffix(Parser *p, PUSE_INST inst, PUSE_REGISTER reg, char *s, int isDest)
{
	while (*s == '.') {
		char word[32];
		size_t n = 0;
		const NameValue *nv;

		s++;
		while (*s != '.' && *s != '\0' && n < sizeof(word) - 1)
			word[n++] = *s++;
		word[n] = '\0';
		if ((nv = findName(argFlagNames, ARRAY_SIZE(argFlagNames), word)) != NULL) {
			reg->uFlags |= nv->value;
		} else if (strncmp(word, "bytemask", 8) == 0 && strlen(word) == 12) {
			IMG_UINT32 mask = 0;
			int i;

			for (i = 0; i < 4; i++)
				mask = (mask << 1) | (word[8 + i] == '1');
			reg->uFlags |= USEASM_ARGFLAGS_BYTEMSK_PRESENT | (mask << USEASM_ARGFLAGS_BYTEMSK_SHIFT);
		} else if (isdigit((unsigned char)word[0]) && word[1] == '\0') {
			reg->uFlags |= (IMG_UINT32)(word[0] - '0') << USEASM_ARGFLAGS_COMP_SHIFT;
		} else if (isDest && strspn(word, "xyzw") == n && n > 0) {
			IMG_UINT32 mask = 0;
			size_t i;

			for (i = 0; i < n; i++)
				mask |= 1U << (strchr("xyzw", word[i]) - "xyzw");
			inst->uFlags1 = (inst->uFlags1 & USEASM_OPFLAGS1_MASK_CLRMSK) |
					(mask << USEASM_OPFLAGS1_MASK_SHIFT);
		} else {
			return fail(p, "unknown argument modifier .%s", word);
		}
	}
	if (*s != '\0')
		return fail(p, "unexpected '%s' after an argument", s);
	return 1;
}

static int parseRegister(Parser *p, PUSE_INST inst, int argIndex, PUSE_REGISTER reg, char *s)
{
	int isDest = argIndex == 0;
	const char *end;
	char word[32];
	size_t n;
	size_t i;

	UseAsmInitReg(reg);
	if (*s == '~') {
		reg->uFlags |= USEASM_ARGFLAGS_INVERT;
		s++;
	}
	if (*s == '!') {
		reg->uFlags |= isDest ? USEASM_ARGFLAGS_DISABLEWB : USEASM_ARGFLAGS_NOT;
		s++;
	}

	/* immediates: #12, #0x0000000C, #-1, -#0x0000003C (a backward relative branch) */
	if (*s == '#' || (s[0] == '-' && s[1] == '#')) {
		int negative = *s == '-';

		s += negative ? 2 : 1;
		if (!parseNumber(s, &reg->uNumber, &end))
			return fail(p, "bad immediate '%s'", s);
		if (negative)
			reg->uNumber = (IMG_UINT32)-(IMG_INT32)reg->uNumber;
		reg->uType = USEASM_REGTYPE_IMMEDIATE;
		return parseArgSuffix(p, inst, reg, (char *)end, 0);
	}

	if (strncmp(s, "swizzle(", 8) == 0) {
		char *close = strchr(s, ')');

		if (close == NULL)
			return fail(p, "unterminated swizzle");
		reg->uType = USEASM_REGTYPE_SWIZZLE;
		for (i = 0; s + 8 + i < close; i++) {
			const char *sel = strchr(swizzleSelNames, s[8 + i]);

			if (sel == NULL || s[8 + i] == '\0')
				return fail(p, "bad swizzle '%s'", s);
			reg->uNumber |= (IMG_UINT32)(sel - swizzleSelNames) << (USEASM_SWIZZLE_FIELD_SIZE * i);
		}
		return parseArgSuffix(p, inst, reg, close + 1, 0);
	}

	/* the index registers: i.l, i.h, i.lh */
	if (strncmp(s, "i.", 2) == 0 && (s[2] == 'l' || s[2] == 'h')) {
		reg->uType = USEASM_REGTYPE_INDEX;
		s += 2;
		if (*s == 'l') {
			reg->uNumber |= 1;
			s++;
		}
		if (*s == 'h') {
			reg->uNumber |= 2;
			s++;
		}
		return parseArgSuffix(p, inst, reg, s, isDest);
	}

	if (strncmp(s, "pclink", 6) == 0) {
		reg->uType = USEASM_REGTYPE_LINK;
		return parseArgSuffix(p, inst, reg, s + 6, isDest);
	}

	/* a bank and a number, or an indexed access: sa[il + #4] */
	for (i = 0; i < ARRAY_SIZE(regTypeNames); i++) {
		size_t len = strlen(regTypeNames[i].name);

		if (strncmp(s, regTypeNames[i].name, len) != 0)
			continue;
		if (isdigit((unsigned char)s[len])) {
			reg->uType = regTypeNames[i].value;
			reg->uNumber = (IMG_UINT32)strtoul(s + len, (char **)&end, 10);
			return parseArgSuffix(p, inst, reg, (char *)end, isDest);
		}
		if (s[len] == '[' && s[len + 1] == 'i' && (s[len + 2] == 'l' || s[len + 2] == 'h') &&
		    strncmp(s + len + 3, " + #", 4) == 0) {
			reg->uType = regTypeNames[i].value;
			reg->uIndex = s[len + 2] == 'l' ? USEREG_INDEX_L : USEREG_INDEX_H;
			if (!parseNumber(s + len + 7, &reg->uNumber, &end) || *end != ']')
				return fail(p, "bad indexed register '%s'", s);
			return parseArgSuffix(p, inst, reg, (char *)end + 1, isDest);
		}
	}

	/* keywords: source selectors, address modes, smlsi's modes, idf's paths */
	n = strcspn(s, ".");
	if (n >= sizeof(word))
		return fail(p, "bad argument '%s'", s);
	memcpy(word, s, n);
	word[n] = '\0';
	if (inst->uOpcode == USEASM_OP_SMLSI && argIndex >= 4 && argIndex < 8) {
		if (strcmp(word, "incrementmode") == 0 || strcmp(word, "swizzlemode") == 0) {
			reg->uType = USEASM_REGTYPE_IMMEDIATE;
			reg->uNumber = word[0] == 's';
			return parseArgSuffix(p, inst, reg, s + n, 0);
		}
	}
	if (inst->uOpcode == USEASM_OP_IDF && argIndex == 1) {
		for (i = 0; i < ARRAY_SIZE(idfPathNames); i++) {
			if (strcmp(word, idfPathNames[i]) == 0) {
				reg->uType = USEASM_REGTYPE_IMMEDIATE;
				reg->uNumber = (IMG_UINT32)i;
				return 1;
			}
		}
	}
	/* phas's wait condition "none" is the address mode the encoder takes */
	if ((inst->uOpcode == USEASM_OP_PHAS || inst->uOpcode == USEASM_OP_PHASIMM) && argIndex == 3) {
		const NameValue *nv = findName(addressModeNames, ARRAY_SIZE(addressModeNames), word);

		if (nv != NULL && nv->value == EURASIA_USE1_MOECTRL_ADDRESSMODE_NONE) {
			reg->uType = USEASM_REGTYPE_ADDRESSMODE;
			reg->uNumber = nv->value;
			return 1;
		}
	}
	{
		const NameValue *nv = findName(intSrcSelNames, ARRAY_SIZE(intSrcSelNames), word);

		if (nv != NULL) {
			reg->uType = USEASM_REGTYPE_INTSRCSEL;
			reg->uNumber = nv->value;
			return parseArgSuffix(p, inst, reg, s + n, 0);
		}
		nv = findName(addressModeNames, ARRAY_SIZE(addressModeNames), word);
		if (nv != NULL) {
			reg->uType = USEASM_REGTYPE_ADDRESSMODE;
			reg->uNumber = nv->value;
			return 1;
		}
	}
	/* a label */
	if ((isalpha((unsigned char)*s) || *s == '_') && p->label != NULL) {
		n = strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$");
		if (s[n] == '\0') {
			reg->uType = USEASM_REGTYPE_LABEL;
			if (!p->label(p->labelContext, s, &reg->uNumber))
				return fail(p, "unknown label '%s'", s);
			return 1;
		}
	}
	return fail(p, "bad argument '%s'", s);
}

/* the opcode's modifiers: .skipinv, .fetch2, .testtanz, .chan0, ... */
static int parseOpcodeFlags(Parser *p, PUSE_INST inst, char *s)
{
	while (*s == '.') {
		char word[32];
		size_t n = 0;
		const NameValue *nv;
		IMG_UINT32 v;

		s++;
		while (*s != '.' && *s != '\0' && n < sizeof(word) - 1)
			word[n++] = *s++;
		word[n] = '\0';
		if ((nv = findName(flags1Names, ARRAY_SIZE(flags1Names), word)) != NULL) {
			inst->uFlags1 |= nv->value;
		} else if ((OpcodeDescFlags(inst->uOpcode) & USE_DESCFLAG_DISASM_SIMPLE_TESTS) &&
			   (nv = findName(simpleTestNames, ARRAY_SIZE(simpleTestNames), word)) != NULL) {
			inst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
			inst->uTest = nv->value;
		} else if ((nv = findName(flags2Names, ARRAY_SIZE(flags2Names), word)) != NULL) {
			inst->uFlags2 |= nv->value;
		} else if ((nv = findName(flags3Names, ARRAY_SIZE(flags3Names), word)) != NULL) {
			inst->uFlags3 |= nv->value;
		} else if (strncmp(word, "fetch", 5) == 0 && isdigit((unsigned char)word[5])) {
			v = (IMG_UINT32)atoi(word + 5);
			inst->uFlags1 = (inst->uFlags1 & USEASM_OPFLAGS1_REPEAT_CLRMSK) |
					USEASM_OPFLAGS1_FETCHENABLE | (v << USEASM_OPFLAGS1_REPEAT_SHIFT);
		} else if (strncmp(word, "repeat", 6) == 0 && isdigit((unsigned char)word[6])) {
			v = (IMG_UINT32)atoi(word + 6);
			inst->uFlags1 = (inst->uFlags1 & USEASM_OPFLAGS1_REPEAT_CLRMSK) |
					(v << USEASM_OPFLAGS1_REPEAT_SHIFT);
		} else if (strncmp(word, "rs", 2) == 0 && isdigit((unsigned char)word[2])) {
			v = (IMG_UINT32)atoi(word + 2);
			inst->uFlags2 |= v << USEASM_OPFLAGS2_RSHIFT_SHIFT;
		} else if (strncmp(word, "incs", 4) == 0 && strlen(word) == 7) {
			inst->uFlags2 |= USEASM_OPFLAGS2_PERINSTMOE |
					 (word[4] == '1' ? USEASM_OPFLAGS2_PERINSTMOE_INCS0 : 0) |
					 (word[5] == '1' ? USEASM_OPFLAGS2_PERINSTMOE_INCS1 : 0) |
					 (word[6] == '1' ? USEASM_OPFLAGS2_PERINSTMOE_INCS2 : 0);
		} else if (strcmp(word, "i8") == 0 ||
			   (strcmp(word, "u8") == 0 && inst->uOpcode == USEASM_OP_MOVMSK)) {
			inst->uFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8;
		} else if (strncmp(word, "test", 4) == 0 && n >= 6) {
			/* .test<sign><a|o><zero> */
			const char *t = word + 4;
			IMG_UINT32 sign, zero;

			switch (*t++) {
			case 't': sign = USEASM_TEST_SIGN_TRUE; break;
			case 'n': sign = USEASM_TEST_SIGN_NEGATIVE; break;
			case 'p': sign = USEASM_TEST_SIGN_POSITIVE; break;
			default: return fail(p, "bad test .%s", word);
			}
			if (*t != 'a' && *t != 'o')
				return fail(p, "bad test .%s", word);
			v = *t++ == 'a' ? USEASM_TEST_CRCOMB_AND : USEASM_TEST_CRCOMB_OR;
			if (strcmp(t, "t") == 0)
				zero = USEASM_TEST_ZERO_TRUE;
			else if (strcmp(t, "z") == 0)
				zero = USEASM_TEST_ZERO_ZERO;
			else if (strcmp(t, "nz") == 0)
				zero = USEASM_TEST_ZERO_NONZERO;
			else
				return fail(p, "bad test .%s", word);
			inst->uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
			inst->uTest = (inst->uTest & ~USEASM_TEST_TESTTYPE_MASK) |
				      (sign << USEASM_TEST_SIGN_SHIFT) | (zero << USEASM_TEST_ZERO_SHIFT) | v;
		} else if ((nv = findName(chanSelNames, ARRAY_SIZE(chanSelNames), word)) != NULL) {
			inst->uTest = (inst->uTest & USEASM_TEST_CHANSEL_CLRMSK) |
				      (nv->value << USEASM_TEST_CHANSEL_SHIFT);
		} else if ((nv = findName(testMaskNames, ARRAY_SIZE(testMaskNames), word)) != NULL) {
			inst->uTest = (inst->uTest & USEASM_TEST_MASK_CLRMSK) |
				      (nv->value << USEASM_TEST_MASK_SHIFT);
		} else {
			return fail(p, "unknown modifier .%s", word);
		}
	}
	if (*s != '\0')
		return fail(p, "unexpected '%s' after the opcode", s);
	return 1;
}

/* whether a mov of an immediate must be limm */
static int UseParseNeedsLimm(PUSE_INST inst)
{
	(void)inst;
	return 1;
}

/*
 * The opcode of a name. Where several opcodes share one, the one whose argument count
 * holds `argCount`: phas (wait on the next phase) and its immediate form. nop is the
 * padding instruction; USEASM_OP_NOP is the assembler's empty placeholder.
 */
static int findOpcode(const char *name, int argCount, USEASM_OPCODE *op)
{
	IMG_UINT32 i;
	int found = 0;

	if (strcmp(name, "nop") == 0) {
		*op = USEASM_OP_PADDING;
		return 1;
	}
	for (i = 0; i < USEASM_OP_MAXIMUM; i++) {
		const char *opName = OpcodeName(i);

		if (opName == NULL || strcmp(opName, name) != 0)
			continue;
		if (!found || (int)OpcodeArgumentCount(*op) < argCount) {
			*op = (USEASM_OPCODE)i;
			found = 1;
		}
	}
	return found;
}

/* a load or store: ldad.bpcache r5, [r0,+#8], drc1 / stad [sa1,+#14], r16 */
static int parseLdSt(Parser *p, PUSE_INST inst, char **args, int count)
{
	int isStore = (OpcodeDescFlags(inst->uOpcode) & USE_DESCFLAG_DISASM_MEMORY_ST) != 0;
	int a = 0;
	int next = 0;
	char *base, *offset, *close;

	if (!isStore) {
		if (count < 1 || !parseRegister(p, inst, 0, &inst->asArg[next++], args[a++]))
			return 0;
	}
	if (a >= count || args[a][0] != '[')
		return fail(p, "expected a [base,offset] address");
	base = args[a] + 1;
	close = strrchr(base, ']');
	offset = strchr(base, ',');
	if (close == NULL || offset == NULL)
		return fail(p, "bad address '%s'", args[a]);
	*close = '\0';
	*offset++ = '\0';
	if (!parseRegister(p, inst, 1, &inst->asArg[next++], base))
		return 0;
	if (strncmp(offset, "++", 2) == 0 || strncmp(offset, "--", 2) == 0) {
		inst->uFlags1 |= USEASM_OPFLAGS1_PREINCREMENT;
		if (*offset == '-')
			inst->uFlags2 |= USEASM_OPFLAGS2_INCSGN;
		offset += 2;
	} else if (*offset == '+' || *offset == '-') {
		if (*offset == '-')
			inst->uFlags2 |= USEASM_OPFLAGS2_INCSGN;
		offset++;
	} else {
		size_t len = strlen(offset);

		if (len > 2 && (strcmp(offset + len - 2, "++") == 0 || strcmp(offset + len - 2, "--") == 0)) {
			inst->uFlags1 |= USEASM_OPFLAGS1_POSTINCREMENT;
			if (offset[len - 1] == '-')
				inst->uFlags2 |= USEASM_OPFLAGS2_INCSGN;
			offset[len - 2] = '\0';
		}
	}
	if (!parseRegister(p, inst, 2, &inst->asArg[next++], offset))
		return 0;
	a++;
	if (isStore) {
		if (a >= count || !parseRegister(p, inst, 3, &inst->asArg[next++], args[a++]))
			return 0;
	} else {
		/* a load's range argument is optional, its DRC always comes fourth */
		if (count - a == 2) {
			inst->uFlags1 |= USEASM_OPFLAGS1_RANGEENABLE;
			if (!parseRegister(p, inst, 3, &inst->asArg[next], args[a++]))
				return 0;
		}
		next = 4;
		if (a >= count || !parseRegister(p, inst, 4, &inst->asArg[next++], args[a++]))
			return 0;
	}
	if (a != count)
		return fail(p, "too many arguments");
	return 1;
}

int UseParseInstruction(PCSGX_CORE_DESC target, const char *text, IMG_UINT32 address,
			PUSE_INST inst, UseParseLabelFn label, void *labelContext, char *error)
{
	Parser parser = { text, error, label, labelContext };
	Parser *p = &parser;
	char buf[512];
	char *args[USE_MAX_ARGUMENTS + 2];
	char *s, *name, *argText;
	int count, expected, i, a;

	size_t n;

	(void)target;
	(void)address;
	error[0] = '\0';
	UseAsmInitInst(inst);
	for (i = 0; i < USE_MAX_ARGUMENTS; i++)
		UseAsmInitReg(&inst->asArg[i]);
	if (strlen(text) >= sizeof(buf))
		return fail(p, "line too long");
	strcpy(buf, text);
	s = buf;
	while (*s == ' ' || *s == '\t')
		s++;

	/* the predicate */
	for (i = (int)ARRAY_SIZE(predicateNames) - 1; i > 0; i--) {
		n = strlen(predicateNames[i]);
		if (strncmp(s, predicateNames[i], n) == 0 && s[n] == ' ') {
			inst->uFlags1 |= (IMG_UINT32)i << USEASM_OPFLAGS1_PRED_SHIFT;
			s += n + 1;
			break;
		}
	}

	/* the opcode and its modifiers */
	name = s;
	n = strcspn(s, ". ");
	argText = s + strcspn(s, " ");
	if (*argText != '\0')
		*argText++ = '\0';
	count = splitArgs(argText, args, (int)ARRAY_SIZE(args));
	if (count < 0)
		return fail(p, "too many arguments");
	{
		char save = name[n];

		name[n] = '\0';
		if (!findOpcode(name, count, &inst->uOpcode))
			return fail(p, "unknown opcode '%s'", name);
		name[n] = save;
	}
	inst->uFlags1 |= USEREG_MASK_X << USEASM_OPFLAGS1_MASK_SHIFT;
	if (!parseOpcodeFlags(p, inst, name + n))
		return 0;

	switch (inst->uOpcode) {
	case USEASM_OP_LDAB: case USEASM_OP_LDAW: case USEASM_OP_LDAD: case USEASM_OP_LDAQ:
	case USEASM_OP_STAB: case USEASM_OP_STAW: case USEASM_OP_STAD: case USEASM_OP_STAQ:
	case USEASM_OP_LDTB: case USEASM_OP_LDTW: case USEASM_OP_LDTD: case USEASM_OP_LDTQ:
	case USEASM_OP_STTB: case USEASM_OP_STTW: case USEASM_OP_STTD: case USEASM_OP_STTQ:
	case USEASM_OP_LDLB: case USEASM_OP_LDLW: case USEASM_OP_LDLD: case USEASM_OP_LDLQ:
	case USEASM_OP_STLB: case USEASM_OP_STLW: case USEASM_OP_STLD: case USEASM_OP_STLQ:
		return parseLdSt(p, inst, args, count);
	case USEASM_OP_EMITVERTEX: case USEASM_OP_EMITSTATE: case USEASM_OP_EMITPRIMITIVE:
	case USEASM_OP_EMITPIXEL1: case USEASM_OP_EMITPIXEL2: case USEASM_OP_EMITPDS:
	case USEASM_OP_EMITPIXEL:
		/* the first argument carries a comment: #1 incp */
		if (count > 0) {
			char *c = strstr(args[0], "/*");

			if (c != NULL) {
				while (c > args[0] && c[-1] == ' ')
					c--;
				*c = '\0';
			}
		}
		for (a = 0; a < count; a++) {
			if (!parseRegister(p, inst, a, &inst->asArg[a], args[a]))
				return 0;
		}
		return 1;
	default:
		break;
	}

	/* the standard form: the disassembler leaves out source selectors that are "none" */
	expected = (int)OpcodeArgumentCount(inst->uOpcode);
	if ((inst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE) &&
	    ((inst->uTest & ~USEASM_TEST_MASK_CLRMSK) >> USEASM_TEST_MASK_SHIFT) == USEASM_TEST_MASK_NONE &&
	    inst->uOpcode != USEASM_OP_MOVC && inst->uOpcode != USEASM_OP_VMOVC &&
	    inst->uOpcode != USEASM_OP_VMOVCU8)
		expected++;
	if (inst->uOpcode == USEASM_OP_IDF || inst->uOpcode == USEASM_OP_WDF ||
	    inst->uOpcode == USEASM_OP_SETM)
		expected = count;
	if (count > expected)
		return fail(p, "too many arguments (%d, expected %d)", count, expected);
	{
		int isNone[USE_MAX_ARGUMENTS + 1] = { 0 };
		int missing = expected - count;

		/* the arguments left out are "none": ima32's upper destination first, else the last */
		if (inst->uOpcode == USEASM_OP_IMA32 && missing > 0) {
			isNone[1] = 1;
			missing--;
		}
		for (i = expected - 1; missing > 0; i--, missing--)
			isNone[i] = 1;
		for (a = 0, i = 0; i < expected; i++) {
			if (isNone[i]) {
				inst->asArg[i].uType = USEASM_REGTYPE_INTSRCSEL;
				inst->asArg[i].uNumber = USEASM_INTSRCSEL_NONE;
				continue;
			}
			if (!parseRegister(p, inst, i, &inst->asArg[i], args[a++]))
				return 0;
		}
	}

	/* a mov of an immediate that no source field holds is limm */
	if (inst->uOpcode == USEASM_OP_MOV && inst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
	    UseParseNeedsLimm(inst))
		inst->uOpcode = USEASM_OP_LIMM;

	/* the encoder takes imae's third source as a signed 7-bit immediate */
	if (inst->uOpcode == USEASM_OP_IMAE && inst->asArg[3].uType == USEASM_REGTYPE_IMMEDIATE &&
	    (inst->asArg[3].uNumber & 0x40) && inst->asArg[3].uNumber < 0x80)
		inst->asArg[3].uNumber |= ~0x7fU;

	/* a branch to a number: ba's is the address, br's the distance from the branch */
	if ((inst->uOpcode == USEASM_OP_BA || inst->uOpcode == USEASM_OP_BR) &&
	    inst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE)
		inst->asArg[0].uType = USEASM_REGTYPE_REF;
	return 1;
}
