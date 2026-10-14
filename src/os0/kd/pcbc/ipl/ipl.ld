/*
 * ePSP IPL memory map. Code from 0x88600000; the data the code names (strings,
 * keys, boot state) stays at the addresses the original uses, so both images can be
 * compared in memory and the scratchpad/boot-info contracts keep their layout.
 */
OUTPUT_ARCH(mips:allegrex)
ENTRY(_start)

SECTIONS
{
	. = 0x88600000;
	.text : {
		KEEP(*(.text.start))
		KEEP(*(.text.halt))
		haltStubEnd = .;
		*(.text .text.*)
		*(.rodata .rodata.*)		/* compiler data: jump tables, literals */
		ASSERT(. <= 0x8860508c, "IPL code overruns the pinned data at 0x8860508c");
	}
	.data 0x8860508c : {
		KEEP(*(.data.pinned))
		*(.data .data.* .sdata .sdata.*)
	}
	.bss 0x88605440 (NOLOAD) : {
		KEEP(*(.bss.pinned))
		*(.bss .bss.* .sbss .sbss.* COMMON)
	}
	/DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) }
}
