/*
 * NSKBL: one region at IMAGE_ADDRESS (0x51000000), the reset vector table first,
 * then code, read-only data, the SceKblForKernel export table psp2bootconfig links
 * to, data on the next page and .bss, the boot stacks first.
 */
ENTRY(_start)

SECTIONS
{
	. = IMAGE_ADDRESS;

	.text : {
		KEEP(*(.text.vectors))
		*(.text .text.*)
	}
	.rodata : {
		*(.rodata .rodata.*)
	}
	.sceLib.ent ALIGN(4) : {
		__sce_libent_top = .;
		KEEP(*(.sceLib.ent))
		__sce_libent_btm = .;
		KEEP(*(.sceExport.rodata.name))
		KEEP(*(.sceExport.rodata.nid))
		KEEP(*(.sceExport.rodata.hash))
		KEEP(*(.sceExport.rodata.ent))
	}
	/*
	 * The unwinder's table, empty: the boot loader keeps no entry of the
	 * assembly files it shares with os0 (discarded below).
	 */
	__exidx_start = .;
	__exidx_end = .;
	.data ALIGN(0x1000) : {
		*(.data .data.*)
	}
	.bss (NOLOAD) : {
		KEEP(*(.bss.stacks))
		*(.bss .bss.* COMMON)
		. = ALIGN(4);
		_end = .;
	}

	/DISCARD/ : {
		*(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*)
	}
}
