/*
 * second_loader: loaded into Cmep SRAM at 0x40000 (aliased to 0x800000).
 * Vector table and reset entry at 0x800000..0x8001e4 jump to 0x800290 (_start).
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)
ENTRY(reset_entry)

SECTIONS
{
	. = 0x800000;
	.vectors 0x800000 : { KEEP(*(.vectors)) }
	.trampoline0 0x800100 : { KEEP(*(.trampoline0)) }
	.trampoline1 0x800140 : { KEEP(*(.trampoline1)) }
	.reset 0x800180 : { KEEP(*(.reset)) }
	.entry 0x800200 : { KEEP(*(.entry)) }
	.text 0x8003e0 : {
		*(.text .text.*)
		. = ALIGN(8);
	}
	.ctors : {
		__ctors_start = .;
		KEEP(*(.ctors))
		__ctors_end = .;
		LONG(0)
	}
	.dtors : {
		__dtors_start = .;
		KEEP(*(.dtors))
		__dtors_end = .;
		LONG(0)
	}
	.rodata : {
		. = ALIGN(8);
		*(.rodata .rodata.* .srodata .srodata.*)
	}
	. = ALIGN(8) + 4;
	.data . : {
		*(.data .data.*)
	}
	. = ALIGN(64) + 8;
	.sdata . : {
		__sdata_start = .;
		*(.sdata .sdata.*)
	}
	__gp = __sdata_start + 0x8000;
	__sdabase = __gp;
	__bss_start = .;
	.bss (NOLOAD) : {
		. = ALIGN(64);
		*(.bss .bss.*)
		*(COMMON)
		*(.sbss .sbss.*)
		. = ALIGN(16);
		__bss_end = .;
	}

	/DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
