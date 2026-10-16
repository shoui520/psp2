/* Preserve the interrupted registers before entering C policy. */
#include "excpmgr_entry.inc"
	.section .text.excpmgr_initial_pabt, "ax", %progbits
	INITIAL_HANDLER initialPabtHandler, excpPabtEntry, 4
