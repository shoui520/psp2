/*
 * SKBL: the reset vector page at physical 0, the code from IMAGE_ADDRESS (0x40020000,
 * the reset handler first), then read-only data, data and .bss.
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
		*(.rodata .rodata.*)
	}
	.data ALIGN(0x100) : {
		*(.data .data.*)
	}
	.bss (NOLOAD) : {
		*(.bss .bss.* COMMON)
	}

	/DISCARD/ : {
		*(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*)
	}
}
