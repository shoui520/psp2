/*
 * Raw boot images (psp2_image): one region at IMAGE_ADDRESS (--defsym), the reset
 * vector table first, then code, read-only data, data and .bss.
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
	.data : {
		*(.data .data.*)
	}
	.bss (NOLOAD) : {
		*(.bss .bss.* COMMON)
	}

	/DISCARD/ : {
		*(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*)
	}
}
