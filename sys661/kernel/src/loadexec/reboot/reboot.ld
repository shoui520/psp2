/*
 * ePSP 6.61 reboot.bin memory map (loaded and entered at 0x88600000 by sceLoadExec).
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
	}
	.rodata : ALIGN(4) {
		*(.rodata .rodata.*)
	}
	.data : ALIGN(4) {
		*(.data .data.* .sdata .sdata.*)
	}
	.bss (NOLOAD) : ALIGN(64) {
		__bss_start = .;
		*(.bss .bss.* .sbss .sbss.* COMMON)
		__bss_end = .;
	}
	/DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.comment) *(.gnu.attributes) }
}
