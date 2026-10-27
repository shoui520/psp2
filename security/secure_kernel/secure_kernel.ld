/*
 * secure_kernel.ld: linker script for the PS Vita Secure Kernel (enp.bin),
 * loaded by second_loader into Cmep SRAM at 0x800000 and entered at 0x800100.
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)
ENTRY(_start)

SECTIONS
{
	. = 0x800000;
	.vectors 0x800000 : {
		KEEP(*(.vectors))
	}
	.text 0x8000b0 : {
		KEEP(*(.init_text))
		KEEP(*(.ctors_text))
		*(.text .text.*)
		. = ALIGN(8);
		__ctors_start = .;
		KEEP(*(.ctors))
		__ctors_end = .;
		__dtors_start = .;
		KEEP(*(.dtors))
		__dtors_end = .;
	}
	.rodata : {
		. = ALIGN(8);
		*(.rodata .rodata.* .srodata .srodata.*)
		. = ALIGN(8);
	}
	.data : {
		*(.data .data.*)
		. = ALIGN(8);
	}
	.sdata : {
		__sdata_start = .;
		*(.sdata .sdata.*)
		. = ALIGN(8);
	}
	__gp = __sdata_start + 0x8000;
	__sdabase = __gp;
	__bss_start = .;
	.bss (NOLOAD) : {
		. = ALIGN(64);
		*(.bss .bss.*)
		*(COMMON)
		*(.sbss .sbss.*)
		. = ALIGN(8);
		__bss_end = .;
	}

	/DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
