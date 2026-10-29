/*
 * SKBL: the reset vector page at physical 0, the code from IMAGE_ADDRESS (0x40020000,
 * the reset handler first), then read-only data, data and .bss, the boot stacks first.
 * Three segments, as kernel_boot_loader.self holds them (tools/mkkbl.py): the vectors,
 * the code and read-only data, the data and .bss. The data is the boot save area
 * (boot.c's first), then as long a backup that reset.S fills or restores it from.
 */
ENTRY(reset)

PHDRS
{
	vectors PT_LOAD FLAGS(7);
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	.vectors 0 : {
		KEEP(*(.text.vectors))
	} :vectors
	.text IMAGE_ADDRESS : {
		KEEP(*(.text.reset))
		*(.text .text.*)
	} :text
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
		_fdata = .;
		KEEP(*(.data.bootSaveArea))
		*(.data .data.*)
		. = ALIGN(4);
		bootSaveAreaBackup = .;
		. = . + (bootSaveAreaBackup - _fdata);
	} :data
	.bss : {
		KEEP(*(.bss.stacks))
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
		_end = .;
	}

	/* the debug records' file, line and function, which dbginfo.py hashes into them */
	.sce_dbgsrc 0 : {
		*(.debug_sce_dbgsrc)
	} :NONE

	/DISCARD/ : {
		*(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*)
	}
}
