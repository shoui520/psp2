/* Preserve the interrupted registers before entering C policy. */
#include "excpmgr_entry.inc"
	.section .text.excpmgr_initial_dabt, "ax", %progbits
	INITIAL_HANDLER initialDabtHandler, excpDabtEntry, 8
