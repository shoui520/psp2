/* Preserve the interrupted registers before entering C policy. */
#include "excpmgr_entry.inc"
	.section .text.excpmgr_initial_undef, "ax", %progbits
	INITIAL_HANDLER initialUndefHandler, excpUndefEntry, thumb
