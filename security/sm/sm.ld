/*
 * Secure module link: one segment from 0x80b000 (entry first), the stack below
 * 0x81ff80. gp points 0x8000 past the start of the small data, so that both directions
 * of its signed 16-bit offsets are used.
 */

OUTPUT_FORMAT("elf32-mep-little")
OUTPUT_ARCH(mep)
ENTRY(_start)

__stack_top = 0x81ff80;

SECTIONS
{
	. = 0x80b000;
	.text : { KEEP(*(.text.entry)) *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	.data : { *(.data .data.*) }
	.ctors : { __ctors_start = .; KEEP(*(.ctors)) __ctors_end = .; }
	.dtors : { __dtors_start = .; KEEP(*(.dtors)) __dtors_end = .; }
	.sdata : { __sdata_start = .; *(.sdata .sdata.*) }
	__gp = __sdata_start + 0x8000;
	__sdabase = __gp;
	.sbss : { *(.sbss .sbss.*) }
	.bss : { *(.bss .bss.* COMMON) }

	/DISCARD/ : { *(.comment) *(.note*) }
}
