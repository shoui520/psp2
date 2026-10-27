/*
 * SKBL: the reset vector page at physical 0, the code from IMAGE_ADDRESS (0x40020000,
 * the reset handler first), then read-only data, data and .bss, the boot stacks first.
 */
ENTRY(reset)

SECTIONS
{
	.vectors 0 : {
		KEEP(*(.text.vectors))
	}
	.text IMAGE_ADDRESS : {
		KEEP(*(.text.reset))
		*(.text .text.*)
	}
	.rodata : {
		*(.rodata .rodata.* .sce_dbginfo .sce_dbginfo.*)
	}
	/*
	 * The unwinder's table, empty: the boot loader keeps no entry of the
	 * assembly files it shares with os0 (discarded below).
	 */
	__exidx_start = .;
	__exidx_end = .;
	.data ALIGN(0x100) : {
		*(.data .data.*)
	}
	.bss (NOLOAD) : {
		KEEP(*(.bss.stacks))
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
		_end = .;
	}

	/* the debug records' file, line and function, which dbginfo.py hashes into them */
	.sce_dbgsrc 0 : {
		*(.debug_sce_dbgsrc)
	}

	/DISCARD/ : {
		*(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*)
	}
}
