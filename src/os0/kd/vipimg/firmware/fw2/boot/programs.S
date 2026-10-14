/*
 * fw2's program records, at VIP_PROGRAM_RECORDS (0x2000) and VIP_RESET_HOOKS
 * (0x2200): the boot block's tables that describe the programs. They are linked
 * after the programs, from the symbols each program's link defines (-R).
 */
#include "../common/runtime.h"

/*
 * The programs of each core (VipProgram), from each program's link: core 0 has
 * the two stream parsers, core 1 the codec.
 */
	.section .boot.programs, "a"
	.global	g_programRecords
g_programRecords:
	.macro	program name
	.word	\name\()Entry
	.word	__\name\()_code_load, __\name\()_code_size
	.word	__\name\()_local_load
	.word	__\name\()_globals_load, __\name\()_globals_size
	.word	__\name\()_zero_size
	.word	__\name\()_local_size
	.endm
	program	parser0
	program	parser1
	.space	0x100 - 2 * 32
	program	codec
	.space	0x100 - 32

/*
 * Each core's hardware reset, which vipHalt calls first; then the entry of a
 * core numbered 255.
 */
	.global	g_resetHooks
g_resetHooks:
	.word	parser1ResetHardware
	.word	codecResetHardware
	.word	0
