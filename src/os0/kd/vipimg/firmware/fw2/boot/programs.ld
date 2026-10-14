/* fw2's program records (programs.S), at VIP_PROGRAM_RECORDS, in the boot block */
SECTIONS
{
	.boot.programs 0x2000 : AT(0x802000) { *(.boot.programs) }
	ASSERT(g_resetHooks == 0x2200, "the reset hooks are not at VIP_RESET_HOOKS")
	ASSERT(. <= 0x2ff8, "the program records overrun the reported values")

	/DISCARD/ : { *(.comment) }
}
